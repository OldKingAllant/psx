#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/include/psxemu/GPUCommands.hpp>
#include <psxemu/renderer/GLRenderer.hpp>

#include <common/Errors.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <fmt/format.h>

namespace psx {
	static constexpr u32 MAX_VERTEX_X_DISTANCE = 1023;
	static constexpr u32 MAX_VERTEX_Y_DISTANCE = 511;

	void Gpu::FlushDrawOps() {
		m_renderer->FlushCommands();
	}

	template <u8 NumVertices, bool Textured, 
		bool Gouraud, bool Transparent, bool Raw>
	void DrawPolygon(Gpu& gpu, PolygonCmd cmd) {
		static_assert(NumVertices == 3 || NumVertices == 4);

		auto& cmd_fifo = gpu.GetCmdFifo();
		auto renderer = gpu.GetRenderer();

		auto first_color = cmd.color();
		
		video::GenericVertex vertices[NumVertices] = {};

		u32 flags = 0;

		if constexpr (Gouraud) {
			flags |= u32(video::TexturedVertexFlags::GOURAUD);
		}

		if constexpr (Transparent) {
			flags |= u32(video::TexturedVertexFlags::SEMI_TRANSPARENT);
		}

		if constexpr (Raw) {
			flags |= u32(video::TexturedVertexFlags::RAW_TEXTURE);
		}

		u32 clut_and_page = 0;

		/*
		*
		Vertex     YYYYXXXX               - required, two signed 16 bits values
		Color      xxBBGGRR               - optional, only present for gouraud shading
		UV         ClutVVUU or PageVVUU   - optional, only present for textured polygons
		*/

		for (u8 curr_vertex = 0; curr_vertex < NumVertices; curr_vertex++) {
			if constexpr (Gouraud) {
				if (curr_vertex != 0) {
					vertices[curr_vertex].color = ColorAttribute{ cmd_fifo.deque() }.rgb();
				}
				else {
					vertices[curr_vertex].color = first_color;
				}
			}
			else {
				vertices[curr_vertex].color = first_color;
			}

			auto vertex_pos = PositionAttribute{ cmd_fifo.deque() };

			vertices[curr_vertex].x = vertex_pos.x();
			vertices[curr_vertex].y = vertex_pos.y();

			if constexpr (Textured) {
				auto uv = UVAttribute{cmd_fifo.deque()};

				if (curr_vertex == 0) {
					u32 clut = uv.clut_or_page();
					clut_and_page |= (clut << 16);
				}
				else if (curr_vertex == 1) {
					u32 page = uv.clut_or_page();
					clut_and_page |= page;
				}

				u32 u = uv.u();
				u32 v = uv.v();

				vertices[curr_vertex].uv = (v << 16) | u;
				vertices[curr_vertex].flags = flags;
			}
		}

		vertices[0].clut_page = clut_and_page;
		vertices[1].clut_page = clut_and_page;
		vertices[2].clut_page = clut_and_page;

		if constexpr (NumVertices == 4) {
			vertices[3].clut_page = clut_and_page;
		}

		video::GenericPrimitive prim{};
		prim.vertex_count = 3;
		prim.vertices[0] = vertices[0];
		prim.vertices[1] = vertices[1];
		prim.vertices[2] = vertices[2];
		prim.semi_transparent = cmd.is_semi_transparent();

		if constexpr (Textured) {
			prim.type = video::PipelineType::TEXTURED_TRIANGLE;
		}
		else if constexpr (Gouraud) {
			prim.type = video::PipelineType::BASIC_GOURAUD_TRIANGLE;
		}
		else {
			prim.type = video::PipelineType::UNTEXTURED_OPAQUE_FLAT_TRIANGLE;
		}
		
		if constexpr (Transparent) {
			if constexpr (Textured) {
				u16 page = u16(vertices[0].clut_page);
				u8 semi_transparency = u8((page >> 5) & 0x3);
				prim.semi_transparency_type = semi_transparency;
			}
			else {
				auto transparency_type = u8(gpu.GetStat().semi_transparency);
				prim.semi_transparency_type = transparency_type;
			}
		}

		renderer->DrawPrimitive(prim);

		if constexpr (NumVertices == 4) {
			video::GenericPrimitive prim2{};
			prim2.semi_transparency_type = prim.semi_transparency_type;
			prim2.semi_transparent = prim.semi_transparent;
			prim2.vertex_count = 3;
			prim2.type = prim.type;
			prim2.vertices[0] = vertices[1];
			prim2.vertices[1] = vertices[2];
			prim2.vertices[2] = vertices[3];
			renderer->DrawPrimitive(prim2);
		}

		if constexpr (Textured) {
			gpu.TryUpdateTexpage(u16(vertices[0].clut_page));
		}

		if (gpu.GetRecordingCommands()) {
			GPUCommand cmd_copy{};
			cmd_copy.value = cmd.cmd;
			cmd_copy.reg = CommandRegister::GP0;
			cmd_copy.frame_of_recording = gpu.GetVblankCount();
			cmd_copy.gp0.type = GP0CommandType::POLYGON;
			cmd_copy.gp0.polygon = cmd;
			cmd_copy.params.rendering.semi_transparent = Transparent;
			cmd_copy.params.rendering.transparency_type = prim.semi_transparency_type;
			cmd_copy.params.rendering.vertex_count = NumVertices;
			cmd_copy.params.rendering.vertices[0].color.attribute = vertices[0].color;
			cmd_copy.params.rendering.vertices[1].color.attribute = vertices[1].color;
			cmd_copy.params.rendering.vertices[2].color.attribute = vertices[2].color;

			cmd_copy.params.rendering.vertices[0].x = vertices[0].x + gpu.GetXOffset();
			cmd_copy.params.rendering.vertices[1].x = vertices[1].x + gpu.GetXOffset();
			cmd_copy.params.rendering.vertices[2].x = vertices[2].x + gpu.GetXOffset();

			cmd_copy.params.rendering.vertices[0].y = vertices[0].y + gpu.GetYOffset();
			cmd_copy.params.rendering.vertices[1].y = vertices[1].y + gpu.GetYOffset();
			cmd_copy.params.rendering.vertices[2].y = vertices[2].y + gpu.GetYOffset();

			//vertices[curr_vertex].uv = (v << 16) | u;
			cmd_copy.params.rendering.vertices[0].u = vertices[0].uv & 0xFFFF;
			cmd_copy.params.rendering.vertices[1].u = vertices[1].uv & 0xFFFF;
			cmd_copy.params.rendering.vertices[2].u = vertices[2].uv & 0xFFFF;

			cmd_copy.params.rendering.vertices[0].v = (vertices[0].uv >> 16) & 0xFFFF;
			cmd_copy.params.rendering.vertices[1].v = (vertices[1].uv >> 16) & 0xFFFF;
			cmd_copy.params.rendering.vertices[2].v = (vertices[2].uv >> 16) & 0xFFFF;

			cmd_copy.params.rendering.vertices[0].clut_page = vertices[0].clut_page;
			cmd_copy.params.rendering.vertices[1].clut_page = vertices[0].clut_page;
			cmd_copy.params.rendering.vertices[2].clut_page = vertices[0].clut_page;

			if constexpr (NumVertices == 4) {
				cmd_copy.params.rendering.vertices[3].color.attribute = vertices[3].color;
				cmd_copy.params.rendering.vertices[3].x = vertices[3].x + gpu.GetXOffset();
				cmd_copy.params.rendering.vertices[3].y = vertices[3].y + gpu.GetYOffset();
				cmd_copy.params.rendering.vertices[3].u = vertices[3].uv & 0xFFFF;
				cmd_copy.params.rendering.vertices[3].v = (vertices[3].uv >> 16) & 0xFFFF;
				cmd_copy.params.rendering.vertices[3].clut_page = vertices[3].clut_page;
			}

			cmd_copy.start_index = gpu.GetLatestIdleIndex();

			gpu.GetRecordedCommands().emplace_back(cmd_copy);
		}
	}

	template <bool Textured, bool Transparent, bool Raw>
	void DrawRectangle(Gpu& gpu, RectCmd cmd) {
		auto& cmd_fifo = gpu.GetCmdFifo();
		auto renderer = gpu.GetRenderer();

		auto color = cmd.color();

		//Manually form texpage attribute
		u16 texpage = 0;

		if constexpr (Textured) {
			auto& gpu_stat = gpu.GetStat();
			texpage |= gpu_stat.texture_page_x_base;
			texpage |= ((u16)gpu_stat.texture_page_y_base << 4);
			texpage |= ((u16)gpu_stat.semi_transparency << 5);
			texpage |= ((u16)gpu_stat.tex_page_colors << 7);
			texpage |= ((u16)gpu_stat.texture_page_y_base2 << 11);
		}

		u32 flags{ 0 };

		if constexpr (Transparent) {
			flags |= video::TexturedVertexFlags::SEMI_TRANSPARENT;
		}

		if constexpr (Raw) {
			flags |= video::TexturedVertexFlags::RAW_TEXTURE;
		}

		auto vertex1 = PositionAttribute{ cmd_fifo.deque() };

		auto x1 = vertex1.x();
		auto y1 = vertex1.y();

		u32 tex_and_clut{};
		u32 u1{}, v1{};

		if constexpr (Textured) {
			auto clutuv = UVAttribute{ cmd_fifo.deque() };
			auto clut = clutuv.clut_or_page();

			u1 = clutuv.u();
			v1 = clutuv.v();

			tex_and_clut = (clut << 16) | texpage;
		}		

		u32 sizex = 0;
		u32 sizey = 0;
		std::tie(sizex, sizey) = cmd.get_size();

		if (sizex == u32(-1) || sizey == u32(-1)) {
			//variable
			auto wh = SizeAttribute{ cmd_fifo.deque() };
			sizex = wh.sizex();
			sizey = wh.sizey();

			if (sizex > MAX_VERTEX_X_DISTANCE || sizey > MAX_VERTEX_Y_DISTANCE) {
				return;
			}
		}

		//Bottom left
		i32 x2 = x1;
		i32 y2 = y1 + sizey;

		u32 u2 = u1;
		u32 v2 = v1 + sizey;

		//Top right
		i32 x3 = x1 + sizex;
		i32 y3 = y1;

		u32 u3 = u1 + sizex;
		u32 v3 = v1;

		//Bottom right
		i32 x4 = x1 + sizex;
		i32 y4 = y1 + sizey;

		u32 u4 = u1 + sizex;
		u32 v4 = v1 + sizey;

		if constexpr (Textured) {
			if (gpu.GetXFlip()) {
				u4 = u3 = (u32)std::max(0, i32(u1 + 1 - sizex));
			}

			if (gpu.GetYFlip()) {
				v4 = v2 = (u32)std::max(0, i32(v1 + 1 - sizey));
			}
		}

		video::GenericVertex vertices[4] = {};

		vertices[0].clut_page = tex_and_clut;
		vertices[0].color = color;
		vertices[0].flags = flags;
		vertices[0].x = x1;
		vertices[0].y = y1;
		vertices[0].uv = u1 | (v1 << 16);

		vertices[1].clut_page = tex_and_clut;
		vertices[1].color = color;
		vertices[1].flags = flags;
		vertices[1].x = x2;
		vertices[1].y = y2;
		vertices[1].uv = u2 | (v2 << 16);

		vertices[2].clut_page = tex_and_clut;
		vertices[2].color = color;
		vertices[2].flags = flags;
		vertices[2].x = x3;
		vertices[2].y = y3;
		vertices[2].uv = u3 | (v3 << 16);

		vertices[3].clut_page = tex_and_clut;
		vertices[3].color = color;
		vertices[3].flags = flags;
		vertices[3].x = x4;
		vertices[3].y = y4;
		vertices[3].uv = u4 | (v4 << 16);

		video::GenericPrimitive triangle1 = {};
		video::GenericPrimitive triangle2 = {};

		triangle1.vertices[0] = vertices[0];
		triangle1.vertices[1] = vertices[1];
		triangle1.vertices[2] = vertices[2];

		triangle2.vertices[0] = vertices[1];
		triangle2.vertices[1] = vertices[2];
		triangle2.vertices[2] = vertices[3];

		triangle2.vertex_count = triangle1.vertex_count = 3;
		
		if constexpr (Transparent) {
			u8 transparency_type = u8(gpu.GetStat().semi_transparency);
			triangle2.semi_transparency_type = triangle1.semi_transparency_type = transparency_type;
			triangle2.semi_transparent = triangle1.semi_transparent = true;
		}
		
		if constexpr (Textured) {
			triangle2.type = triangle1.type = video::PipelineType::TEXTURED_TRIANGLE;
		}
		else {
			triangle2.type = triangle1.type = video::PipelineType::UNTEXTURED_OPAQUE_FLAT_TRIANGLE;
		}

		renderer->DrawPrimitive(triangle1);
		renderer->DrawPrimitive(triangle2);

		if (gpu.GetRecordingCommands()) {
			GPUCommand cmd_copy{};
			cmd_copy.value = cmd.cmd;
			cmd_copy.reg = CommandRegister::GP0;
			cmd_copy.frame_of_recording = gpu.GetVblankCount();
			cmd_copy.gp0.type = GP0CommandType::RECTANGLE;
			cmd_copy.gp0.rect = cmd;
			cmd_copy.params.rendering.semi_transparent = Transparent;
			cmd_copy.params.rendering.transparency_type = triangle1.semi_transparency_type;
			cmd_copy.params.rendering.vertex_count = 4;
			cmd_copy.params.rendering.vertices[0].color.attribute = vertices[0].color;
			cmd_copy.params.rendering.vertices[1].color.attribute = vertices[1].color;
			cmd_copy.params.rendering.vertices[2].color.attribute = vertices[2].color;

			cmd_copy.params.rendering.vertices[0].x = vertices[0].x + gpu.GetXOffset();
			cmd_copy.params.rendering.vertices[1].x = vertices[1].x + gpu.GetXOffset();
			cmd_copy.params.rendering.vertices[2].x = vertices[2].x + gpu.GetXOffset();

			cmd_copy.params.rendering.vertices[0].y = vertices[0].y + gpu.GetYOffset();
			cmd_copy.params.rendering.vertices[1].y = vertices[1].y + gpu.GetYOffset();
			cmd_copy.params.rendering.vertices[2].y = vertices[2].y + gpu.GetYOffset();

			//vertices[curr_vertex].uv = (v << 16) | u;
			cmd_copy.params.rendering.vertices[0].u = vertices[0].uv & 0xFFFF;
			cmd_copy.params.rendering.vertices[1].u = vertices[1].uv & 0xFFFF;
			cmd_copy.params.rendering.vertices[2].u = vertices[2].uv & 0xFFFF;

			cmd_copy.params.rendering.vertices[0].v = (vertices[0].uv >> 16) & 0xFFFF;
			cmd_copy.params.rendering.vertices[1].v = (vertices[1].uv >> 16) & 0xFFFF;
			cmd_copy.params.rendering.vertices[2].v = (vertices[2].uv >> 16) & 0xFFFF;

			cmd_copy.params.rendering.vertices[0].clut_page = vertices[0].clut_page;
			cmd_copy.params.rendering.vertices[1].clut_page = vertices[0].clut_page;
			cmd_copy.params.rendering.vertices[2].clut_page = vertices[0].clut_page;

			
			cmd_copy.params.rendering.vertices[3].color.attribute = vertices[3].color;
			cmd_copy.params.rendering.vertices[3].x = vertices[3].x + gpu.GetXOffset();
			cmd_copy.params.rendering.vertices[3].y = vertices[3].y + gpu.GetYOffset();
			cmd_copy.params.rendering.vertices[3].u = vertices[3].uv & 0xFFFF;
			cmd_copy.params.rendering.vertices[3].v = (vertices[3].uv >> 16) & 0xFFFF;
			cmd_copy.params.rendering.vertices[3].clut_page = vertices[3].clut_page;

			cmd_copy.start_index = gpu.GetLatestIdleIndex();

			gpu.GetRecordedCommands().emplace_back(cmd_copy);
		}
	}

	template <bool Gouraud, bool Polyline, bool Transparent>
	void DrawLines(Gpu& gpu, LineCmd cmd) {
		auto& cmd_fifo = gpu.GetCmdFifo();
		auto renderer = gpu.GetRenderer();

		auto prev_color = cmd.color();

		auto first_vertex = PositionAttribute{ cmd_fifo.deque() };
		auto prev_x = first_vertex.x();
		auto prev_y = first_vertex.y();

		video::GenericPrimitive line{};
		video::GenericVertex v0{};
		video::GenericVertex v1{};

		line.vertex_count = 2;

		if constexpr (Gouraud) {
			line.type = video::PipelineType::SHADED_LINE;
		}
		else {
			line.type = video::PipelineType::MONO_LINE;
		}

		if constexpr (Transparent) {
			auto transparency_type = u8(gpu.GetStat().semi_transparency);
			line.semi_transparency_type = transparency_type;
			line.semi_transparent = true;
		}

		u64 curr_iteration{};

		while (!cmd_fifo.empty()) {
			u32 curr_color = prev_color;
			if constexpr (Gouraud) {
				curr_color = ColorAttribute{ cmd_fifo.deque() }.rgb();
			}
			
			auto curr_vertex = PositionAttribute{ cmd_fifo.deque() };
			i32 curr_x = curr_vertex.x();
			i32 curr_y = curr_vertex.y();

			v0.x = prev_x;
			v0.y = prev_y;
			v0.color = prev_color;

			v1.x = curr_x;
			v1.y = curr_y;
			v1.color = curr_color;

			line.vertices[0] = v0;
			line.vertices[1] = v1;

			renderer->DrawPrimitive(line);

			if (gpu.GetRecordingCommands()) {
				GPUCommand cmd_copy{};
				cmd_copy.value = cmd.cmd;
				cmd_copy.reg = CommandRegister::GP0;
				cmd_copy.frame_of_recording = gpu.GetVblankCount();
				cmd_copy.gp0.type = GP0CommandType::LINE;
				cmd_copy.gp0.line = cmd;
				cmd_copy.params.rendering.semi_transparent = Transparent;
				cmd_copy.params.rendering.transparency_type = line.semi_transparency_type;
				cmd_copy.params.rendering.vertex_count = 2;

				cmd_copy.params.rendering.vertices[0].color.attribute = v0.color;
				cmd_copy.params.rendering.vertices[1].color.attribute = v1.color;

				cmd_copy.params.rendering.vertices[0].x = v0.x + gpu.GetXOffset();
				cmd_copy.params.rendering.vertices[1].x = v1.x + gpu.GetXOffset();

				cmd_copy.params.rendering.vertices[0].y = v0.y + gpu.GetYOffset();
				cmd_copy.params.rendering.vertices[1].y = v1.y + gpu.GetYOffset();

				cmd_copy.start_index = gpu.GetLatestIdleIndex();

				if (curr_iteration == 0) {
					cmd_copy.polyline_begin = true;
				}

				gpu.GetRecordedCommands().emplace_back(cmd_copy);
			}

			if constexpr (!Polyline) {
				break;
			}
			else {
				if (cmd_fifo.empty() && gpu.GetRecordingCommands()) {
					GPUCommand cmd_copy{};
					cmd_copy.value = cmd.cmd;
					cmd_copy.reg = CommandRegister::GP0;
					cmd_copy.frame_of_recording = gpu.GetVblankCount();
					cmd_copy.gp0.type = GP0CommandType::POLYLINE_END;
					cmd_copy.gp0.end_marker = {};
					cmd_copy.start_index = gpu.GetLatestIdleIndex();
					gpu.GetRecordedCommands().emplace_back(cmd_copy);
				}
			}

			prev_x = curr_x;
			prev_y = curr_y;
			prev_color = curr_color;

			curr_iteration++;
		}
	}

	void Gpu::DrawQuad() {
		auto cmd = PolygonCmd{ m_cmd_fifo.deque() };

		if (!cmd.is_quad()) {
			LOG_ERROR("DRAW", "[DRAW] DRAW QUAD CALLED WITH NON-QUAD COMMAND");
			LOG_FLUSH();
			error::DebugBreak();
		}

		auto flags = cmd.get_flags();

		// gouraud | quad | textured | transparent | raw
		// 01000 -> flat shaded/no texture/opaque
		// 01001 -> flat shaded/no texture/opaque (should not make sense)
		// 01010 -> flat shaded/no texture/transparent
		// 01011 -> flat shaded/no texture/transparent (should not make sense)
		// 01100 -> flat shaded/textured/opaque
		// 01101 -> flat shaded/textured/opaque raw texture
		// 01110 -> flat shaded/textured/transparent 
		// 01111 -> flat shaded/textured/transparent raw texture
		// 11000 -> gouraud shaded/no texture/opaque
		// 11001 -> gouraud shaded/no texture/opaque (should not make sense)
		// 11010 -> gouraud shaded/no texture/transparent
		// 11011 -> gouraud shaded/no texture/transparent (should not make sense)
		// 11100 -> gouraud shaded/textured/opaque
		// 11101 -> gouraud shaded/textured/opaque raw texture
		// 
		// 11110 -> gouraud shaded/textured/transparent 
		// 11111 -> gouraud shaded/textured/transparent raw texture
		switch (flags)
		{
		case 0b01000:
		case 0b01001:
			DrawPolygon<4, false, false, false, false>(*this, cmd);
			break;
		case 0b01010:
		case 0b01011:
			DrawPolygon<4, false, false, true, false>(*this, cmd);
			break;
		case 0b01100:
			DrawPolygon<4, true, false, false, false>(*this, cmd);
			break;
		case 0b01101:
			DrawPolygon<4, true, false, false, true>(*this, cmd);
			break;
		case 0b01110:
			DrawPolygon<4, true, false, true, false>(*this, cmd);
			break;
		case 0b01111:
			DrawPolygon<4, true, false, true, true>(*this, cmd);
			break;
		case 0b11000:
		case 0b11001:
			DrawPolygon<4, false, true, false, false>(*this, cmd);
			break;
		case 0b11010:
		case 0b11011:
			DrawPolygon<4, false, true, true, false>(*this, cmd);
			break;
		case 0b11100:
			DrawPolygon<4, true, true, false, false>(*this, cmd);
			break;
		case 0b11101:
			DrawPolygon<4, true, true, false, true>(*this, cmd);
			break;
		case 0b11110:
			DrawPolygon<4, true, true, true, false>(*this, cmd);
			break;
		case 0b11111:
			DrawPolygon<4, true, true, true, true>(*this, cmd);
			break;
		default:
			error::Unreachable();
			break;
		}
	}

	void Gpu::DrawTriangle() {
		auto cmd = PolygonCmd{ m_cmd_fifo.deque() };

		if (cmd.is_quad()) {
			LOG_ERROR("DRAW", "[DRAW] DRAW TRIANGLE CALLED WITH QUAD COMMAND");
			LOG_FLUSH();
			error::DebugBreak();
		}

		auto flags = cmd.get_flags();

		// gouraud | quad | textured | transparent | raw
		// 00000 -> flat shaded/no texture/opaque
		// 00001 -> flat shaded/no texture/opaque (should not make sense)
		// 00010 -> flat shaded/no texture/transparent
		// 00011 -> flat shaded/no texture/transparent (should not make sense)
		// 00100 -> flat shaded/textured/opaque
		// 00101 -> flat shaded/textured/opaque raw texture
		// 00110 -> flat shaded/textured/transparent 
		// 00111 -> flat shaded/textured/transparent raw texture
		// 10000 -> gouraud shaded/no texture/opaque
		// 10001 -> gouraud shaded/no texture/opaque (should not make sense)
		// 10010 -> gouraud shaded/no texture/transparent
		// 10011 -> gouraud shaded/no texture/transparent (should not make sense)
		// 10100 -> gouraud shaded/textured/opaque
		// 10101 -> gouraud shaded/textured/opaque raw texture
		// 10110 -> gouraud shaded/textured/transparent 
		// 10111 -> gouraud shaded/textured/transparent raw texture
		switch (flags)
		{
		case 0b00000:
		case 0b00001:
			DrawPolygon<3, false, false, false, false>(*this, cmd);
			break;
		case 0b00010:
		case 0b00011:
			DrawPolygon<3, false, false, true, false>(*this, cmd);
			break;
		case 0b00100:
			DrawPolygon<3, true, false, false, false>(*this, cmd);
			break;
		case 0b00101:
			DrawPolygon<3, true, false, false, true>(*this, cmd);
			break;
		case 0b00110:
			DrawPolygon<3, true, false, true, false>(*this, cmd);
			break;
		case 0b00111:
			DrawPolygon<3, true, false, true, true>(*this, cmd);
			break;
		case 0b10000:
		case 0b10001:
			DrawPolygon<3, false, true, false, false>(*this, cmd);
			break;
		case 0b10010:
		case 0b10011:
			DrawPolygon<3, false, true, true, false>(*this, cmd);
			break;
		case 0b10100:
			DrawPolygon<3, true, true, false, false>(*this, cmd);
			break;
		case 0b10101:
			DrawPolygon<3, true, true, false, true>(*this, cmd);
			break;
		case 0b10110:
			DrawPolygon<3, true, true, true, false>(*this, cmd);
			break;
		case 0b10111:
			DrawPolygon<3, true, true, true, true>(*this, cmd);
			break;
		default:
			error::Unreachable();
			break;
		}
	}

	void Gpu::QuickFill() {
		FlushDrawOps();
		m_renderer->SyncTextures();

		u32 cmd = m_cmd_fifo.peek();

		u32 color = m_cmd_fifo.deque() & 0xFFFFFF;
		u32 top_left = m_cmd_fifo.deque();

		i32 x_off = u16(top_left & 0x3F0); //Aligned to 16 pixels
		i32 y_off = u16(top_left >> 16) & 0x1FF;

		u32 size = m_cmd_fifo.deque();

		u32 w = (u32(size & 0x3FF) + 0xF) & ~0xF;
		u32 h = u32(size >> 16) & 0x1FF;

		if (w == 0 || h == 0) {
			LOG_WARN("GPU", "FILL has either X or Y size equal to zero");
			return;
		}

		if (x_off < 0 || y_off < 0) {
			LOG_WARN("GPU", "[GPU] QUICK-FILL offsets less than zero");
			return;
		}
			
		if (x_off + w > VRAM_X_SIZE || y_off + h > VRAM_Y_SIZE) {
			LOG_WARN("GPU", "[GPU] QUICK-FILL size goes out of bounds");
			LOG_WARN("GPU", "      X = {}, Y = {}", x_off, y_off);
			LOG_WARN("GPU", "      W = {}, H = {}", w, h);
		}
			
		m_renderer->Fill(x_off, y_off, w, h, color);

		if (m_recording_commands) {
			GPUCommand cmd_copy{};
			cmd_copy.value = cmd;
			cmd_copy.reg = CommandRegister::GP0;
			cmd_copy.frame_of_recording = m_curr_vblank_count;
			cmd_copy.gp0.type = GP0CommandType::MISC;
			cmd_copy.gp0.misc.type = MiscCommandType::QUICK_FILL;
			cmd_copy.gp0.misc.cmd = cmd;
			cmd_copy.params.quick_fill.x = x_off;
			cmd_copy.params.quick_fill.y = y_off;
			cmd_copy.params.quick_fill.w = w;
			cmd_copy.params.quick_fill.h = h;
			cmd_copy.start_index = m_latest_idle_index;
			m_recorded_cmds.emplace_back(cmd_copy);
		}
	}

	void Gpu::DrawRect() {
		auto cmd = RectCmd{ m_cmd_fifo.deque() };
		auto flags = cmd.get_flags();

		//textured | transparent | raw
		switch (flags)
		{
		case 0b000:
		case 0b001: //raw texture without texture?
			DrawRectangle<false, false, false>(*this, cmd);
			break;
		case 0b010:
		case 0b011: //raw texture without texture?
			DrawRectangle<false, true, false>(*this, cmd);
			break;
		case 0b100:
			DrawRectangle<true, false, false>(*this, cmd);
			break;
		case 0b101:
			DrawRectangle<true, false, true>(*this, cmd);
			break;
		case 0b110:
			DrawRectangle<true, true, false>(*this, cmd);
			break;
		case 0b111:
			DrawRectangle<true, true, true>(*this, cmd);
			break;
		default:
			error::Unreachable();
			break;
		}
	}

	void Gpu::DrawLine() {
		auto cmd = LineCmd{ m_cmd_fifo.deque() };
		auto flags = cmd.get_flags();

		//gouraud | polyline | transparent
		switch (flags)
		{
		case 0b000:
			DrawLines<false, false, false>(*this, cmd);
			break;
		case 0b001:
			DrawLines<false, false, true>(*this, cmd);
			break;
		case 0b010:
			DrawLines<false, true, false>(*this, cmd);
			break;
		case 0b011:
			DrawLines<false, true, true>(*this, cmd);
			break;
		case 0b100:
			DrawLines<true, false, false>(*this, cmd);
			break;
		case 0b101:
			DrawLines<true, false, true>(*this, cmd);
			break;
		case 0b110:
			DrawLines<true, true, false>(*this, cmd);
			break;
		case 0b111:
			DrawLines<true, true, true>(*this, cmd);
			break;
		default:
			error::Unreachable();
			break;
		}
	}
}