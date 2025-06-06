#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/renderer/GLRenderer.hpp>

#include <common/Errors.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <fmt/format.h>

namespace psx {
	void Gpu::CheckIfDrawNeeded() {
		//If this is true, batching becomes
		//impossible, since previous draw
		//calls can immediately affect 
		//others, for this reason, we
		//need to always update the VRAM
		//input texture after every draw
		//command
		if (m_stat.draw_over_mask_disable) {
			FlushDrawOps();
			m_renderer->SyncTextures();
		}
	}

	void Gpu::FlushDrawOps() {
		m_renderer->FlushCommands();
	}

	void Gpu::DrawFlatUntexturedOpaqueQuad() {
		u32 color = m_cmd_fifo.deque() & 0xFFFFFF;

		u32 vertex_1 = m_cmd_fifo.deque();
		u32 vertex_2 = m_cmd_fifo.deque();
		u32 vertex_3 = m_cmd_fifo.deque();
		u32 vertex_4 = m_cmd_fifo.deque();

		video::UntexturedOpaqueFlatVertex v1 = {};
		video::UntexturedOpaqueFlatVertex v2 = {};
		video::UntexturedOpaqueFlatVertex v3 = {};
		video::UntexturedOpaqueFlatVertex v4 = {};

		v1.x = sign_extend<i32, 10>(vertex_1 & 0xFFFF);
		v1.y = sign_extend<i32, 10>((vertex_1 >> 16) & 0xFFFF);

		v2.x = sign_extend<i32, 10>(vertex_2 & 0xFFFF);
		v2.y = sign_extend<i32, 10>((vertex_2 >> 16) & 0xFFFF);

		v3.x = sign_extend<i32, 10>(vertex_3 & 0xFFFF);
		v3.y = sign_extend<i32, 10>((vertex_3 >> 16) & 0xFFFF);

		v4.x = sign_extend<i32, 10>(vertex_4 & 0xFFFF);
		v4.y = sign_extend<i32, 10>((vertex_4 >> 16) & 0xFFFF);

		u32 r = color & 0xFF;
		u32 g = (color >> 8) & 0xFF;
		u32 b = (color >> 16) & 0xFF;

		v1.r = r;
		v1.g = g;
		v1.b = b;

		v2.r = r;
		v2.g = g;
		v2.b = b;

		v3.r = r;
		v3.g = g;
		v3.b = b;

		v4.r = r;
		v4.g = g;
		v4.b = b;

		video::UntexturedOpaqueFlatTriangle triangle1 = {};
		video::UntexturedOpaqueFlatTriangle triangle2 = {};

		triangle1.v0 = v1;
		triangle1.v1 = v2;
		triangle1.v2 = v3;

		triangle2.v0 = v2;
		triangle2.v1 = v3;
		triangle2.v2 = v4;

		LOG_INFO("DRAW", "[GPU] DRAW QUAD");
		LOG_INFO("DRAW", "      R = {}, G = {}, B = {}",
			r, g, b);
		LOG_INFO("DRAW", "      V0 X = {}, Y = {}",
			v1.x, v1.y);
		LOG_INFO("DRAW", "      V1 X = {}, Y = {}",
			v2.x, v2.y);
		LOG_INFO("DRAW", "      V2 X = {}, Y = {}",
			v3.x, v3.y);
		LOG_INFO("DRAW", "      V3 X = {}, Y = {}",
			v4.x, v4.y);

		m_renderer->DrawFlatUntexturedOpaque(
			triangle1
		);

		m_renderer->DrawFlatUntexturedOpaque(
			triangle2
		);
	}

	void Gpu::DrawBasicGouraudQuad() {
		u32 color1 = m_cmd_fifo.deque() & 0xFFFFFF;

		u32 vertex1 = m_cmd_fifo.deque();
		u32 color2 = m_cmd_fifo.deque();
		u32 vertex2 = m_cmd_fifo.deque();
		u32 color3 = m_cmd_fifo.deque();
		u32 vertex3 = m_cmd_fifo.deque();
		u32 color4 = m_cmd_fifo.deque();
		u32 vertex4 = m_cmd_fifo.deque();

		video::BasicGouraudVertex v1 = {};
		video::BasicGouraudVertex v2 = {};
		video::BasicGouraudVertex v3 = {};
		video::BasicGouraudVertex v4 = {};

		v1.x = sign_extend<i32, 10>(vertex1 & 0xFFFF);
		v1.y = sign_extend<i32, 10>((vertex1 >> 16) & 0xFFFF);

		v2.x = sign_extend<i32, 10>(vertex2 & 0xFFFF);
		v2.y = sign_extend<i32, 10>((vertex2 >> 16) & 0xFFFF);

		v3.x = sign_extend<i32, 10>(vertex3 & 0xFFFF);
		v3.y = sign_extend<i32, 10>((vertex3 >> 16) & 0xFFFF);

		v4.x = sign_extend<i32, 10>(vertex4 & 0xFFFF);
		v4.y = sign_extend<i32, 10>((vertex4 >> 16) & 0xFFFF);

		v1.color = color1;
		v2.color = color2;
		v3.color = color3;
		v4.color = color4;

		video::BasicGouraudTriangle triangle1 = {};
		video::BasicGouraudTriangle triangle2 = {};

		triangle1.v0 = v1;
		triangle1.v1 = v2;
		triangle1.v2 = v3;

		triangle2.v0 = v2;
		triangle2.v1 = v3;
		triangle2.v2 = v4;

		LOG_INFO("DRAW", "[GPU] DRAW GOURAUD QUAD");
		//LOG_INFO("DRAW", "      R = {}, G = {}, B = {}",
			//r, g, b);
		LOG_INFO("DRAW", "      V0 X = {}, Y = {}",
			v1.x, v1.y);
		LOG_INFO("DRAW", "      V1 X = {}, Y = {}",
			v2.x, v2.y);
		LOG_INFO("DRAW", "      V2 X = {}, Y = {}",
			v3.x, v3.y);
		LOG_INFO("DRAW", "      V3 X = {}, Y = {}",
			v4.x, v4.y);

		m_renderer->DrawBasicGouraud(triangle1);
		m_renderer->DrawBasicGouraud(triangle2);
	}

	void Gpu::DrawBasicGouraudTriangle() {
		u32 color1 = m_cmd_fifo.deque() & 0xFFFFFF;

		u32 vertex1 = m_cmd_fifo.deque();
		u32 color2 = m_cmd_fifo.deque();
		u32 vertex2 = m_cmd_fifo.deque();
		u32 color3 = m_cmd_fifo.deque();
		u32 vertex3 = m_cmd_fifo.deque();
		
		video::BasicGouraudVertex v1 = {};
		video::BasicGouraudVertex v2 = {};
		video::BasicGouraudVertex v3 = {};

		v1.x = sign_extend<i32, 10>(vertex1 & 0xFFFF);
		v1.y = sign_extend<i32, 10>((vertex1 >> 16) & 0xFFFF);

		v2.x = sign_extend<i32, 10>(vertex2 & 0xFFFF);
		v2.y = sign_extend<i32, 10>((vertex2 >> 16) & 0xFFFF);

		v3.x = sign_extend<i32, 10>(vertex3 & 0xFFFF);
		v3.y = sign_extend<i32, 10>((vertex3 >> 16) & 0xFFFF);

		v1.color = color1;
		v2.color = color2;
		v3.color = color3;

		video::BasicGouraudTriangle triangle1 = {};

		triangle1.v0 = v1;
		triangle1.v1 = v2;
		triangle1.v2 = v3;

		LOG_INFO("DRAW", "[GPU] DRAW GOURAUD TRIANGLE");

		m_renderer->DrawBasicGouraud(triangle1);
	}

	void Gpu::DrawTexturedQuad() {
		u32 cmd = m_cmd_fifo.deque();

		u32 gouraud = (cmd >> 28) & 1;
		u32 semi_trans = (cmd >> 25) & 1;
		u32 raw = (cmd >> 24) & 1;
		u32 first_color = (cmd & 0xFFFFFF);

		uint32_t flags{ 0 };

		if (gouraud)
			flags |= video::TexturedVertexFlags::GOURAUD;

		if (semi_trans)
			flags |= video::TexturedVertexFlags::SEMI_TRANSPARENT;

		if (raw)
			flags |= video::TexturedVertexFlags::RAW_TEXTURE;

		video::TexturedVertex vertices[4] = {};

		u32 clut_and_page = 0;

		for (u32 i = 0; i < 4; i++) {
			if (gouraud && i != 0) {
				vertices[i].color = m_cmd_fifo.deque() & 0xFFFFFF;
			}
			else {
				vertices[i].color = first_color;
			}

			u32 vertex_pos = m_cmd_fifo.deque();

			vertices[i].x = sign_extend<i32, 10>(vertex_pos & 0xFFFF);
			vertices[i].y = sign_extend<i32, 10>((vertex_pos >> 16) & 0xFFFF);

			u32 uv = m_cmd_fifo.deque();

			if (i == 0) {
				u32 clut = (uv >> 16) & 0xFFFF;
				clut_and_page |= (clut << 16);
			}
			else if(i == 1) {
				u32 page = (uv >> 16) & 0xFFFF;
				clut_and_page |= page;
			}

			vertices[i].uv = uv & 0xFFFF;

			vertices[i].flags = flags;
		}

		vertices[0].clut_page = clut_and_page;
		vertices[1].clut_page = clut_and_page;
		vertices[2].clut_page = clut_and_page;
		vertices[3].clut_page = clut_and_page;

		video::TexturedTriangle triangle1 = {};
		video::TexturedTriangle triangle2 = {};

		triangle1.v0 = vertices[0];
		triangle1.v1 = vertices[1];
		triangle1.v2 = vertices[2];

		triangle2.v0 = vertices[1];
		triangle2.v1 = vertices[2];
		triangle2.v2 = vertices[3];

		u16 page = (u16)clut_and_page;

		TryUpdateTexpage(page);

		LOG_INFO("DRAW", "[GPU] DRAW TEXTURED QUAD");

		m_renderer->DrawTexturedTriangle(triangle1);
		m_renderer->DrawTexturedTriangle(triangle2);
	}

	void Gpu::DrawTexturedTriangle() {
		u32 cmd = m_cmd_fifo.deque();

		u32 gouraud = (cmd >> 28) & 1;
		u32 semi_trans = (cmd >> 25) & 1;
		u32 raw = (cmd >> 24) & 1;
		u32 first_color = (cmd & 0xFFFFFF);

		uint32_t flags{ 0 };

		if (gouraud)
			flags |= video::TexturedVertexFlags::GOURAUD;

		if (semi_trans)
			flags |= video::TexturedVertexFlags::SEMI_TRANSPARENT;

		if (raw)
			flags |= video::TexturedVertexFlags::RAW_TEXTURE;

		video::TexturedVertex vertices[3] = {};

		u32 clut_and_page = 0;

		for (u32 i = 0; i < 3; i++) {
			if (gouraud && i != 0) {
				vertices[i].color = m_cmd_fifo.deque() & 0xFFFFFF;
			}
			else {
				vertices[i].color = first_color;
			}

			u32 vertex_pos = m_cmd_fifo.deque();

			vertices[i].x = sign_extend<i32, 15>(vertex_pos & 0xFFFF);
			vertices[i].y = sign_extend<i32, 15>((vertex_pos >> 16) & 0xFFFF);

			u32 uv = m_cmd_fifo.deque();

			if (i == 0) {
				u32 clut = (uv >> 16) & 0xFFFF;
				clut_and_page |= (clut << 16);
			}
			else if (i == 1) {
				u32 page = (uv >> 16) & 0xFFFF;
				clut_and_page |= page;
			}

			vertices[i].uv = uv & 0xFFFF;

			vertices[i].flags = flags;
		}

		vertices[0].clut_page = clut_and_page;
		vertices[1].clut_page = clut_and_page;
		vertices[2].clut_page = clut_and_page;

		video::TexturedTriangle triangle = {};

		triangle.v0 = vertices[0];
		triangle.v1 = vertices[1];
		triangle.v2 = vertices[2];

		u16 page = (u16)clut_and_page;

		TryUpdateTexpage(page);

		LOG_INFO("DRAW", "[GPU] DRAW TEXTURED TRIANGLE");

		m_renderer->DrawTexturedTriangle(triangle);
	}

	void Gpu::DrawQuad() {
		u32 cmd = m_cmd_fifo.peek();

		bool tex = (cmd >> 26) & 1;
		bool gouraud = (cmd >> 28) & 1;
		bool transparent = (cmd >> 25) & 1;
		bool raw = (cmd >> 24) & 1;

		u32 params_vert = 1;

		u32 curr_params = m_required_params;

		if (!tex && !transparent && !gouraud) {
			DrawFlatUntexturedOpaqueQuad();
		}
		else if (gouraud && !tex && !transparent) {
			DrawBasicGouraudQuad();
		}
		else if (tex) {
			DrawTexturedQuad();
		}
		else {
			error::DebugBreak();
		}

		//CheckIfDrawNeeded();
	}

	void Gpu::DrawTriangle() {
		u32 cmd = m_cmd_fifo.peek();

		bool tex = (cmd >> 26) & 1;
		bool gouraud = (cmd >> 28) & 1;
		bool transparent = (cmd >> 25) & 1;
		bool raw = (cmd >> 24) & 1;

		u32 params_vert = 1;

		u32 curr_params = m_required_params;

		if (gouraud && !tex) {
			if (transparent) {
				fmt::println("[GPU] TRANSPARENT GOURAUD TRIANGLE");
				error::DebugBreak();
			}
			DrawBasicGouraudTriangle();
		}
		else if (tex) {
			DrawTexturedTriangle();
		}
		else {
			DrawNormalTriangle();
		}

		//CheckIfDrawNeeded();
	}

	void Gpu::QuickFill() {
		FlushDrawOps();
		m_renderer->SyncTextures();

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
	}

	void Gpu::DrawTexturedRect() {
		u32 cmd = m_cmd_fifo.deque();
		u32 color = cmd & 0xFFFFFF;
		//Manually form texpage attribute
		u16 texpage = 0;

		texpage |= m_stat.texture_page_x_base;
		texpage |= ((u16)m_stat.texture_page_y_base << 4);
		texpage |= ((u16)m_stat.semi_transparency << 5);
		texpage |= ((u16)m_stat.tex_page_colors << 7);
		texpage |= ((u16)m_stat.texture_page_y_base2 << 11);

		uint32_t flags{ 0 };

		bool semi_trans = bool((cmd >> 25) & 1);
		bool raw = bool((cmd >> 24) & 1);

		if (semi_trans)
			flags |= video::TexturedVertexFlags::SEMI_TRANSPARENT;

		if (raw)
			flags |= video::TexturedVertexFlags::RAW_TEXTURE;

		u32 vertex1 = m_cmd_fifo.deque();

		i32 x1 = sign_extend<i32, 15>(vertex1 & 0xFFFF);
		i32 y1 = sign_extend<i32, 15>((vertex1 >> 16) & 0xFFFF);

		u32 clutuv = m_cmd_fifo.deque();
		u16 clut = (clutuv >> 16) & 0xFFFF;

		u8 u1 = u8(clutuv & 0xFF);
		u8 v1 = u8((clutuv >> 8) & 0xFF);

		u32 tex_and_clut = (clut << 16) | texpage;

		u8 size = (cmd >> 27) & 3;
		u32 sizes[] = { 0, 1, 8, 16 };

		u32 sizex = 0;
		u32 sizey = 0;

		if (size == 0) {
			//variable
			u32 wh = m_cmd_fifo.deque();
			sizex = (wh & 0xFFFF);
			sizey = (wh >> 16) & 0xFFFF;

			if (sizex > 1023 || sizey > 511) {
				return;
			}
		}
		else {
			sizex = sizes[size];
			sizey = sizex;
		}

		//Bottom left
		i32 x2 = x1;
		i32 y2 = y1 + sizey;

		u8 u2 = u1;
		u8 v2 = v1 + sizey;

		//Top right
		i32 x3 = x1 + sizex;
		i32 y3 = y1;

		u8 u3 = u1 + sizex;
		u8 v3 = v1;

		//Bottom right
		i32 x4 = x1 + sizex;
		i32 y4 = y1 + sizey;

		u8 u4 = u1 + sizex;
		u8 v4 = v1 + sizey;

		if (m_tex_x_flip) {
			u4 = u3 = u8(std::max(0, i32(u1 + 1 - sizex)));
		}

		if (m_tex_y_flip) {
			v4 = v2 = u8(std::max(0, i32(v1 + 1 - sizey)));
		}

		video::TexturedVertex vertices[4] = {};

		vertices[0].clut_page = tex_and_clut;
		vertices[0].color = color;
		vertices[0].flags = flags;
		vertices[0].x = x1;
		vertices[0].y = y1;
		vertices[0].uv = u1 | ((u16)v1 << 8);

		vertices[1].clut_page = tex_and_clut;
		vertices[1].color = color;
		vertices[1].flags = flags;
		vertices[1].x = x2;
		vertices[1].y = y2;
		vertices[1].uv = u2 | ((u16)v2 << 8);

		vertices[2].clut_page = tex_and_clut;
		vertices[2].color = color;
		vertices[2].flags = flags;
		vertices[2].x = x3;
		vertices[2].y = y3;
		vertices[2].uv = u3 | ((u16)v3 << 8);

		vertices[3].clut_page = tex_and_clut;
		vertices[3].color = color;
		vertices[3].flags = flags;
		vertices[3].x = x4;
		vertices[3].y = y4;
		vertices[3].uv = u4 | ((u16)v4 << 8);

		video::TexturedTriangle triangle1 = {};
		video::TexturedTriangle triangle2 = {};

		triangle1.v0 = vertices[0];
		triangle1.v1 = vertices[1];
		triangle1.v2 = vertices[2];

		triangle2.v0 = vertices[1];
		triangle2.v1 = vertices[2];
		triangle2.v2 = vertices[3];

		LOG_INFO("DRAW", "[GPU] DRAW TEXTURED RECT");

		m_renderer->DrawTexturedTriangle(triangle1);
		m_renderer->DrawTexturedTriangle(triangle2);
	}

	void Gpu::DrawUntexturedRect() {
		u32 cmd = m_cmd_fifo.deque();
		u32 color = cmd & 0xFFFFFF;

		bool semi_trans = bool((cmd >> 25) & 1);

		u32 vertex1 = m_cmd_fifo.deque();

		i32 x1 = sign_extend<i32, 15>(vertex1 & 0xFFFF);
		i32 y1 = sign_extend<i32, 15>((vertex1 >> 16) & 0xFFFF);

		u8 size = (cmd >> 27) & 3;
		u32 sizes[] = { 0, 1, 8, 16 };

		u32 sizex = 0;
		u32 sizey = 0;

		if (size == 0) {
			//variable
			u32 wh = m_cmd_fifo.deque();
			sizex = (wh & 0xFFFF);
			sizey = (wh >> 16) & 0xFFFF;

			if (sizex > 1023 || sizey > 511) {
				return;
			}
		}
		else {
			sizex = sizes[size];
			sizey = sizex;
		}

		//Bottom left
		i32 x2 = x1;
		i32 y2 = y1 + sizey;

		//Top right
		i32 x3 = x1 + sizex;
		i32 y3 = y1;

		//Bottom right
		i32 x4 = x1 + sizex;
		i32 y4 = y1 + sizey;

		video::UntexturedOpaqueFlatVertex vertices[4] = {};

		vertices[0].x = x1;
		vertices[0].y = y1;
		vertices[0].r = (color & 0xFF);
		vertices[0].g = ((color >> 8) & 0xFF);
		vertices[0].b = ((color >> 16) & 0xFF);

		vertices[1].x = x2;
		vertices[1].y = y2;
		vertices[1].r = (color & 0xFF);
		vertices[1].g = ((color >> 8) & 0xFF);
		vertices[1].b = ((color >> 16) & 0xFF);
		
		vertices[2].x = x3;
		vertices[2].y = y3;
		vertices[2].r = (color & 0xFF);
		vertices[2].g = ((color >> 8) & 0xFF);
		vertices[2].b = ((color >> 16) & 0xFF);
		
		vertices[3].x = x4;
		vertices[3].y = y4;
		vertices[3].r = (color & 0xFF);
		vertices[3].g = ((color >> 8) & 0xFF);
		vertices[3].b = ((color >> 16) & 0xFF);

		video::UntexturedOpaqueFlatTriangle triangle1 = {};
		video::UntexturedOpaqueFlatTriangle triangle2 = {};

		triangle1.v0 = vertices[0];
		triangle1.v1 = vertices[1];
		triangle1.v2 = vertices[2];

		triangle2.v0 = vertices[1];
		triangle2.v1 = vertices[2];
		triangle2.v2 = vertices[3];

		LOG_INFO("DRAW", "[GPU] DRAW RECT");
		LOG_INFO("DRAW", "      R = {}, G = {}, B = {}",
			triangle1.v0.r, triangle1.v0.g, triangle1.v0.b);

		if (semi_trans) {
			u8 transparency_type = u8(m_stat.semi_transparency);
			m_renderer->DrawTransparentUntexturedTriangle(triangle1, transparency_type);
			m_renderer->DrawTransparentUntexturedTriangle(triangle2, transparency_type);
		}
		else {
			m_renderer->DrawFlatUntexturedOpaque(triangle1);
			m_renderer->DrawFlatUntexturedOpaque(triangle2);
		}
		
	}

	void Gpu::DrawRect() {
		u32 cmd = m_cmd_fifo.peek();
		bool tex = (cmd >> 26) & 1;

		if (tex) {
			DrawTexturedRect();
		}
		else {
			DrawUntexturedRect();
		}

		//CheckIfDrawNeeded();
	}

	void Gpu::DrawLine() {
		u32 cmd = m_cmd_fifo.peek();
		bool is_polyline = (cmd >> 27) & 1;
		bool is_gouraud = (cmd >> 28) & 1;

		if (!is_polyline && !is_gouraud) {
			DrawMonoLine();
		}
		else if (!is_polyline && is_gouraud) {
			DrawShadedLine();
		}
		else if (is_polyline && !is_gouraud) {
			DrawMonoPolyline();
		} 
		else {
			DrawShadedPolyLine();
		}
	}

	void Gpu::DrawMonoLine() {
		u32 cmd = m_cmd_fifo.deque();

		if (m_cmd_fifo.len() != 2) {
			LOG_ERROR("DRAW", "[GPU] INVALID NUMBER OF PARAMS FOR LINE RENDER");
			m_cmd_fifo.clear();
			return;
		}

		u32 vertex0_pos = m_cmd_fifo.deque();
		u32 vertex1_pos = m_cmd_fifo.deque();

		u32 color = (cmd & 0xFFFFFF);

		video::MonoLine line{};

		video::UntexturedOpaqueFlatVertex v0{};
		video::UntexturedOpaqueFlatVertex v1{};

		v0.x = sign_extend<i32, 15>(vertex0_pos & 0xFFFF);
		v0.y = sign_extend<i32, 15>((vertex0_pos >> 16) & 0xFFFF);

		v0.r = (color & 0xFF);
		v0.g = ((color >> 8) & 0xFF);
		v0.b = ((color >> 16) & 0xFF);

		v1.x = sign_extend<i32, 15>(vertex1_pos & 0xFFFF);
		v1.y = sign_extend<i32, 15>((vertex1_pos >> 16) & 0xFFFF);

		v1.r = v0.r;
		v1.g = v0.g;
		v1.b = v0.b;

		line.v0 = v0;
		line.v1 = v1;

		if ((cmd >> 25) & 1) {
			LOG_DEBUG("DRAW", "[GPU] DRAW LINE MONO COLOR (TRANSPARENT)");
			u8 transparency_type = u8(m_stat.semi_transparency);
			m_renderer->DrawMonoTransparentLine(line, transparency_type);
		}
		else {
			LOG_DEBUG("DRAW", "[GPU] DRAW LINE MONO COLOR (OPAQUE)");
			m_renderer->DrawMonoLine(line);
		}
	}

	void Gpu::DrawShadedLine() {
		u32 cmd = m_cmd_fifo.deque();

		if (m_cmd_fifo.len() != 3) {
			LOG_ERROR("DRAW", "[GPU] INVALID NUMBER OF PARAMS FOR SHADED LINE RENDER");
			m_cmd_fifo.clear();
			return;
		}

		video::ShadedLine line{};

		video::BasicGouraudVertex v0{};
		video::BasicGouraudVertex v1{};

		u32 vertex0_color = cmd & 0xFFFFFF;
		u32 vertex0_pos = m_cmd_fifo.deque();

		u32 vertex1_color = m_cmd_fifo.deque() & 0xFFFFFF;
		u32 vertex1_pos = m_cmd_fifo.deque();

		v0.color = vertex0_color;
		v1.color = vertex1_color;

		v0.x = sign_extend<i32, 15>(vertex0_pos & 0xFFFF);
		v0.y = sign_extend<i32, 15>((vertex0_pos >> 16) & 0xFFFF);

		v1.x = sign_extend<i32, 15>(vertex1_pos & 0xFFFF);
		v1.y = sign_extend<i32, 15>((vertex1_pos >> 16) & 0xFFFF);

		line.v0 = v0;
		line.v1 = v1;

		if ((cmd >> 25) & 1) {
			LOG_DEBUG("DRAW", "[GPU] DRAW SHADED LINE (TRANSPARENT)");
			u8 transparency_type = u8(m_stat.semi_transparency);
			m_renderer->DrawShadedTransparentLine(line, transparency_type);
		}
		else {
			LOG_DEBUG("DRAW", "[GPU] DRAW SHADED LINE (OPAQUE)");
			m_renderer->DrawShadedLine(line);
		}
		
	}

	void Gpu::DrawMonoPolyline() {
		u32 cmd = m_cmd_fifo.deque();

		if (m_cmd_fifo.len() < 2) {
			LOG_ERROR("DRAW", "[GPU] POLYLINE RENDER REQUIRES AT LEAST TWO VERTICES");
			m_cmd_fifo.clear();
			return;
		}

		u32 color_packed = cmd & 0xFFFFFF;
		u32 r = color_packed & 0xFF;
		u32 g = (color_packed >> 8) & 0xFF;
		u32 b = (color_packed >> 16) & 0xFF;

		u32 first_vertex = m_cmd_fifo.deque();
		i32 prev_x = sign_extend<i32, 15>(first_vertex & 0xFFFF);
		i32 prev_y = sign_extend<i32, 15>((first_vertex >> 16) & 0xFFFF);

		video::MonoLine line{};

		video::UntexturedOpaqueFlatVertex v0{};
		video::UntexturedOpaqueFlatVertex v1{};

		u8 transparency_type = u8(m_stat.semi_transparency);
		bool is_semi_transparent = (cmd >> 25) & 1;

		while (!m_cmd_fifo.empty()) {
			u32 curr_vertex = m_cmd_fifo.deque();
			i32 curr_x = sign_extend<i32, 15>(curr_vertex & 0xFFFF);
			i32 curr_y = sign_extend<i32, 15>((curr_vertex >> 16) & 0xFFFF);

			v0.x = prev_x;
			v0.y = prev_y;
			v0.r = r;
			v0.g = g;
			v0.b = b;

			v1.x = curr_x;
			v1.y = curr_y;
			v1.r = r;
			v1.g = g;
			v1.b = b;

			line.v0 = v0;
			line.v1 = v1;

			if(is_semi_transparent) {
				m_renderer->DrawMonoTransparentLine(line, transparency_type);
			}
			else {
				m_renderer->DrawMonoLine(line);
			}

			prev_x = curr_x;
			prev_y = curr_y;
		}
	}

	void Gpu::DrawShadedPolyLine() {
		u32 cmd = m_cmd_fifo.deque();

		if (m_cmd_fifo.len() < 3) {
			LOG_ERROR("DRAW", "[GPU] SHADED POLYLINE RENDER REQUIRES AT LEAST TWO VERTICES");
			m_cmd_fifo.clear();
			return;
		}

		u32 prev_color = cmd & 0xFFFFFF;

		u32 first_vertex = m_cmd_fifo.deque();
		i32 prev_x = sign_extend<i32, 15>(first_vertex & 0xFFFF);
		i32 prev_y = sign_extend<i32, 15>((first_vertex >> 16) & 0xFFFF);

		video::ShadedLine line{};
		video::BasicGouraudVertex v0{};
		video::BasicGouraudVertex v1{};

		u8 transparency_type = u8(m_stat.semi_transparency);
		bool is_semi_transparent = (cmd >> 25) & 1;

		while (!m_cmd_fifo.empty()) {
			u32 curr_color = m_cmd_fifo.deque();
			u32 curr_vertex = m_cmd_fifo.deque();
			i32 curr_x = sign_extend<i32, 15>(curr_vertex & 0xFFFF);
			i32 curr_y = sign_extend<i32, 15>((curr_vertex >> 16) & 0xFFFF);

			v0.x = prev_x;
			v0.y = prev_y;
			v0.color = prev_color;

			v1.x = curr_x;
			v1.y = curr_y;
			v1.color = curr_color;

			line.v0 = v0;
			line.v1 = v1;

			if (is_semi_transparent) {
				m_renderer->DrawShadedTransparentLine(line, transparency_type);
			}
			else {
				m_renderer->DrawShadedLine(line);
			}

			prev_x = curr_x;
			prev_y = curr_y;
			prev_color = curr_color;
		}
	}

#pragma optimize("", off)
	void Gpu::DrawNormalTriangle() {
		u32 cmd = m_cmd_fifo.deque();

		u32 color_packed = cmd & 0xFFFFFF;

		video::UntexturedOpaqueFlatVertex v0{};
		video::UntexturedOpaqueFlatVertex v1{};
		video::UntexturedOpaqueFlatVertex v2{};
		video::UntexturedOpaqueFlatTriangle triangle{};

		u32 vertex_1 = m_cmd_fifo.deque();
		u32 vertex_2 = m_cmd_fifo.deque();
		u32 vertex_3 = m_cmd_fifo.deque();

		u32 r = color_packed & 0xFF;
		u32 g = (color_packed >> 8) & 0xFF;
		u32 b = (color_packed >> 16) & 0xFF;

		v0.x = sign_extend<i32, 10>(vertex_1 & 0xFFFF);
		v0.y = sign_extend<i32, 10>((vertex_1 >> 16) & 0xFFFF);

		v1.x = sign_extend<i32, 10>(vertex_2 & 0xFFFF);
		v1.y = sign_extend<i32, 10>((vertex_2 >> 16) & 0xFFFF);

		v2.x = sign_extend<i32, 10>(vertex_3 & 0xFFFF);
		v2.y = sign_extend<i32, 10>((vertex_3 >> 16) & 0xFFFF);

		//fmt::println("[GPU]  R = {}, G = {}, B = {}", r, g, b);
		//fmt::println("       X0 = {:#x}, Y0 = {:#x}", v0.x, v0.y);
		//fmt::println("       X1 = {:#x}, Y1 = {:#x}", v1.x, v1.y);
		//fmt::println("       X2 = {:#x}, Y2 = {:#x}", v2.x, v2.y);

		v0.r = r;
		v0.g = g;
		v0.b = b;

		v1.r = r;
		v1.g = g;
		v1.b = b;

		v2.r = r;
		v2.g = g;
		v2.b = b;

		triangle.v0 = v0;
		triangle.v1 = v1;
		triangle.v2 = v2;

		bool transparent = (cmd >> 25) & 1;

		if (transparent) {
			u8 transparency_type = u8(m_stat.semi_transparency);
			m_renderer->DrawTransparentUntexturedTriangle(triangle,
				transparency_type);
		}
		else {
			m_renderer->DrawFlatUntexturedOpaque(triangle);
		}
	}
#pragma optimize("", on)
}