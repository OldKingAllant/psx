#include "GLRenderer.hpp"
#include "GLContext.hpp"

#include <common/Errors.hpp>

#include <fmt/format.h>
#include <array>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

namespace psx::video {
	Renderer::Renderer() :
		m_resolution_multiplier{1},
		m_vram{}, m_framebuffer{1024, 512},
		m_need_gpu_to_host_copy{false},
		m_need_host_to_gpu_copy{false}, 
		m_processing_cmd{false},
		m_update_ubo{false},
		m_update_scissor{false},
		m_untextured_opaque_flat_pipeline(std::string("../shaders"), std::string("flat_untextured_opaque_triangle")),
		m_basic_gouraud_pipeline(std::string("../shaders"), std::string("basic_gouraud")),
		m_textured_pipeline(std::string("../shaders"), std::string("textured_triangle")),
		m_blit_vertex_buf(6),
		m_blit_shader(std::string("../shaders"), std::string("vram_blit")),
		m_uniform_buf{},
		m_scissor{},
		m_commands{}, m_renderdoc{nullptr},
		m_vram_blit_vertex_buf(6),
		m_vram_blit_shader(std::string("../shaders"), std::string("vram_vram_blit")),
		m_mono_line_pipeline(std::string("../shaders"), std::string("flat_untextured_opaque_triangle")),
		m_shaded_line_pipeline(std::string("../shaders"), std::string("basic_gouraud")),
		m_draw_over_mask_disable{false},
		m_blank_image{},
		m_gl_ctx{GetCurrentGLContext()}
	{
		m_framebuffer.SetLabel("output_vram_fb");
		m_framebuffer.RebuildUpscaledFbo(m_resolution_multiplier);
		m_blit_shader.SetLabel("vram_blit_shader");
		m_untextured_opaque_flat_pipeline.SetLabel("untextured_opaque_flat_pipeline");
		m_basic_gouraud_pipeline.SetLabel("basic_gouraud_pipeline");
		m_textured_pipeline.SetLabel("textured_pipeline");
		m_blit_vertex_buf.SetLabel("vram_blit_vertex_buf");
		m_uniform_buf.SetLabel("global_uniform_buffer");
		m_uniform_buf.buffer.resolution_multiplier = m_resolution_multiplier;
		m_uniform_buf.Bind();
		m_uniform_buf.Upload();
		m_uniform_buf.BindRange(2);
		m_vram_blit_vertex_buf.SetLabel("vram_vram_blit_vertex_buf");
		m_vram_blit_shader.SetLabel("vram_vram_blit_shader");
		m_mono_line_pipeline.SetLabel("mono_line_pipeline");
		m_shaded_line_pipeline.SetLabel("shaded_line_pipeline");
		m_blank_image.resize(1024ULL * 512);

		std::fill(m_blank_image.begin(), m_blank_image.end(), 0x0);
		glTextureSubImage2D(m_framebuffer.GetMaskTexture(),
			0, 0, 0, 1024, 512, GL_RED, GL_UNSIGNED_BYTE,
			m_blank_image.data());
	}

	void Renderer::SyncTextures() {
		if (m_need_host_to_gpu_copy && m_need_gpu_to_host_copy) {
			LOG_ERROR("RENDERER", "[RENDERER] Textures are out of sync!");
			error::DebugBreak();
		}

		if (m_need_gpu_to_host_copy) {
			//Draw call happened:
			//1. Copy output VRAM to input VRAM
			//2. Download input VRAM to host mapped buffer
			m_framebuffer.CopyToTexture(m_vram.GetTextureHandle());
			m_need_gpu_to_host_copy = false;
			glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
		}

		if (m_need_host_to_gpu_copy) {
			m_framebuffer.UpdateInternalTexture(m_vram.GetTextureHandle());
			m_need_host_to_gpu_copy = false;
			glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
		}
	}

	void Renderer::VBlank() {
		FlushCommands();
		SyncTextures();
	}

	void Renderer::DrawPrimitive(GenericPrimitive const& primitive)	{
		constexpr u32 MAX_VERTEX_X_DISTANCE = 1023;
		constexpr u32 MAX_VERTEX_Y_DISTANCE = 511;

		if (primitive.vertex_count > 3) {
			LOG_ERROR("RENDERER", "[RENDERER] PRIMITIVE VERTEX COUNT > 3");
			LOG_FLUSH();
			error::DebugBreak();
		}

		for (size_t i = 0; i < primitive.vertex_count - 1; i++) {
			auto const& v0 = primitive.vertices[i];
			auto const& v1 = primitive.vertices[i + 1];

			if (std::abs(v0.x - v1.x) > MAX_VERTEX_X_DISTANCE ||
				std::abs(v0.y - v1.y) > MAX_VERTEX_Y_DISTANCE) {
				return;
			}
		}

		u32 vertex_count = 3;
		switch (primitive.type)
		{
		case PipelineType::UNTEXTURED_OPAQUE_FLAT_TRIANGLE: {
			if (m_untextured_opaque_flat_pipeline.VertexCount() >= MAX_VERTEX_COUNT) {
				FlushCommands();
			}
			UntexturedOpaqueFlatVertex v0{}, v1{}, v2{};

			v0.x = primitive.vertices[0].x;
			v0.y = primitive.vertices[0].y;

			v1.x = primitive.vertices[1].x;
			v1.y = primitive.vertices[1].y;

			v2.x = primitive.vertices[2].x;
			v2.y = primitive.vertices[2].y;

			u32 r = primitive.vertices[0].color & 0xFF;
			u32 g = (primitive.vertices[0].color >> 8) & 0xFF;
			u32 b = (primitive.vertices[0].color >> 16) & 0xFF;

			v0.r = r;
			v0.g = g;
			v0.b = b;

			v1.r = r;
			v1.g = g;
			v1.b = b;

			v2.r = r;
			v2.g = g;
			v2.b = b;

			m_untextured_opaque_flat_pipeline.PushVertex(v0);
			m_untextured_opaque_flat_pipeline.PushVertex(v1);
			m_untextured_opaque_flat_pipeline.PushVertex(v2);
			m_untextured_opaque_flat_pipeline.AddPrimitiveData({});
		} break;
		case PipelineType::BASIC_GOURAUD_TRIANGLE: {
			if (m_basic_gouraud_pipeline.VertexCount() >= MAX_VERTEX_COUNT) {
				FlushCommands();
			}
			BasicGouraudVertex v0{}, v1{}, v2{};

			v0.x = primitive.vertices[0].x;
			v0.y = primitive.vertices[0].y;

			v1.x = primitive.vertices[1].x;
			v1.y = primitive.vertices[1].y;

			v2.x = primitive.vertices[2].x;
			v2.y = primitive.vertices[2].y;

			v0.color = primitive.vertices[0].color;
			v1.color = primitive.vertices[1].color;
			v2.color = primitive.vertices[2].color;

			m_basic_gouraud_pipeline.PushVertex(v0);
			m_basic_gouraud_pipeline.PushVertex(v1);
			m_basic_gouraud_pipeline.PushVertex(v2);

			m_basic_gouraud_pipeline.AddPrimitiveData({});
		} break;
		case PipelineType::TEXTURED_TRIANGLE: {
			if (m_textured_pipeline.VertexCount() >= MAX_VERTEX_COUNT) {
				FlushCommands();
			}
			TexturedVertex v0{}, v1{}, v2{};

			v0.x = primitive.vertices[0].x;
			v0.y = primitive.vertices[0].y;

			v1.x = primitive.vertices[1].x;
			v1.y = primitive.vertices[1].y;

			v2.x = primitive.vertices[2].x;
			v2.y = primitive.vertices[2].y;

			v0.color = primitive.vertices[0].color;
			v1.color = primitive.vertices[1].color;
			v2.color = primitive.vertices[2].color;

			v0.clut_page = primitive.vertices[0].clut_page;
			v1.clut_page = primitive.vertices[1].clut_page;
			v2.clut_page = primitive.vertices[2].clut_page;

			v0.uv = primitive.vertices[0].uv;
			v1.uv = primitive.vertices[1].uv;
			v2.uv = primitive.vertices[2].uv;

			v0.flags = primitive.vertices[0].flags;
			v1.flags = primitive.vertices[1].flags;
			v2.flags = primitive.vertices[2].flags;

			m_textured_pipeline.PushVertex(v0);
			m_textured_pipeline.PushVertex(v1);
			m_textured_pipeline.PushVertex(v2);
			m_textured_pipeline.AddPrimitiveData({});
		} break;
		case PipelineType::MONO_LINE: {
			if (m_mono_line_pipeline.VertexCount() >= MAX_VERTEX_COUNT) {
				FlushCommands();
			}
			vertex_count = 2;

			UntexturedOpaqueFlatVertex v0{}, v1{};

			v0.x = primitive.vertices[0].x;
			v0.y = primitive.vertices[0].y;

			v1.x = primitive.vertices[1].x;
			v1.y = primitive.vertices[1].y;

			u32 r = primitive.vertices[0].color & 0xFF;
			u32 g = (primitive.vertices[0].color >> 8) & 0xFF;
			u32 b = (primitive.vertices[0].color >> 16) & 0xFF;

			v0.r = r;
			v0.g = g;
			v0.b = b;

			v1.r = r;
			v1.g = g;
			v1.b = b;

			m_mono_line_pipeline.PushVertex(v0);
			m_mono_line_pipeline.PushVertex(v1);
			m_mono_line_pipeline.AddPrimitiveData({});
		} break;
		case PipelineType::SHADED_LINE: {
			if (m_shaded_line_pipeline.VertexCount() >= MAX_VERTEX_COUNT) {
				FlushCommands();
			}
			vertex_count = 2;

			BasicGouraudVertex v0{}, v1{};

			v0.x = primitive.vertices[0].x;
			v0.y = primitive.vertices[0].y;

			v1.x = primitive.vertices[1].x;
			v1.y = primitive.vertices[1].y;

			v0.color = primitive.vertices[0].color;
			v1.color = primitive.vertices[1].color;
			
			m_shaded_line_pipeline.PushVertex(v0);
			m_shaded_line_pipeline.PushVertex(v1);
			m_shaded_line_pipeline.AddPrimitiveData({});
		} break;
		default:
			error::Unreachable();
			break;
		}

		if (vertex_count != primitive.vertex_count) {
			LOG_ERROR("RENDERER", "[RENDERER] PRIMITIVE VERTEX COUNT != EXPECTED");
			LOG_FLUSH();
			error::DebugBreak();
		}

		DrawCommand cmd = {};

		cmd.vertex_count = vertex_count;
		cmd.type = primitive.type;
		cmd.semi_transparent = primitive.semi_transparent;
		cmd.semi_transparency_type = primitive.semi_transparency_type;

		AppendCommand(cmd);
	}

	void Renderer::AppendCommand(DrawCommand cmd) {
		if (m_commands.empty()) {
			m_commands.push_back(cmd);
		}
		else {
			auto& back = m_commands.back();

			if (back.type == cmd.type) {
				bool semi_transparency_check = cmd.semi_transparent && back.semi_transparent &&
					(cmd.semi_transparency_type == back.semi_transparency_type);
				bool opaque_check = !cmd.semi_transparent && !back.semi_transparent;
				if (semi_transparency_check || opaque_check) {
					back.vertex_count += cmd.vertex_count;
				}
				else {
					m_commands.push_back(cmd);
				}
			}
			else
				m_commands.push_back(cmd);
		}
	}

	void Renderer::DrawBatch() {
		if (m_commands.empty())
			return;

		UpdateUbo();

		auto gl_ctx = GetCurrentGLContext();
		gl_ctx->SetViewport(0, 0, 1024ULL * m_resolution_multiplier, 512ULL * m_resolution_multiplier);

		gl_ctx->SetTextureSlot(GL_TEXTURE0);
		gl_ctx->BindTexture({ .type = GL_TEXTURE_2D, .handle = m_vram.GetTextureHandle() });

		gl_ctx->BindImage(3, {
			.texture = m_framebuffer.GetMaskTexture(),
			.level = 0,
			.layered = false,
			.layer = 0,
			.access = GL_READ_WRITE,
			.format = GL_R8
		});

		gl_ctx->SetScissor(
			m_scissor.top_x * m_resolution_multiplier, m_scissor.top_y * m_resolution_multiplier,
			size_t(m_scissor.bottom_x - m_scissor.top_x + 1) * m_resolution_multiplier,
			size_t(m_scissor.bottom_y - m_scissor.top_y + 1) * m_resolution_multiplier
		);

		gl_ctx->ScissorEnable();

		constexpr u32 PIPELINE_COUNT = (u32)PipelineType::ENUM_MAX;

		u32 pipeline_offsets[PIPELINE_COUNT] = {};
		u32 curr_primitive_index[PIPELINE_COUNT] = {};

		m_framebuffer.Bind();

		while (!m_commands.empty()) {
			auto const& cmd = m_commands.front();

			if (cmd.semi_transparent) {
				gl_ctx->BlendEnable();

				if(cmd.semi_transparency_type == 2 /*&& cmd.type != PipelineType::TEXTURED_TRIANGLE*/)
					glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);
				else 
					glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

				glBlendColor(0.25f, 0.25f, 0.25f, 0.5f);

				//glBlendFuncSeparate order: src dst src dst
				if (cmd.type == PipelineType::TEXTURED_TRIANGLE) {
					glBlendFuncSeparate(GL_SRC1_ALPHA, GL_SRC1_COLOR, GL_ONE, GL_ZERO);
				}
				else {
					switch (cmd.semi_transparency_type)
					{
					case 0: {
						glBlendFuncSeparate(GL_CONSTANT_ALPHA, GL_CONSTANT_ALPHA, GL_ONE, GL_ZERO);
					} break;
					case 1:
						glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ZERO);
						break;
					case 2:
						glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ZERO);
						break;
					case 3:
						glBlendFuncSeparate(GL_CONSTANT_COLOR, GL_ONE, GL_ONE, GL_ZERO);
						break;
					default:
						break;
					}
				}
			}
			else {
				gl_ctx->BlendDisable();
			}

			auto pipeline_id = cmd.type;
			auto count = cmd.vertex_count;

			u32 curr_offset = pipeline_offsets[(u32)pipeline_id];
			u32 curr_primitive = curr_primitive_index[(u32)pipeline_id];

			switch (pipeline_id)
			{
			case psx::video::PipelineType::UNTEXTURED_OPAQUE_FLAT_TRIANGLE: {
				m_untextured_opaque_flat_pipeline.Draw(curr_offset, count);
				curr_primitive_index[(u32)pipeline_id] += count / 3;
			}
				break;
			case psx::video::PipelineType::BASIC_GOURAUD_TRIANGLE: {
				m_basic_gouraud_pipeline.Draw(curr_offset, count);
				curr_primitive_index[(u32)pipeline_id] += count / 3;
			}
				break;
			case psx::video::PipelineType::TEXTURED_TRIANGLE: {
				m_textured_pipeline.Draw(curr_offset, count);
				curr_primitive_index[(u32)pipeline_id] += count / 3;
			}
				break;
			case psx::video::PipelineType::MONO_LINE: {
				m_mono_line_pipeline.Draw(curr_offset, count);
				curr_primitive_index[(u32)pipeline_id] += count / 2;
			}
				break;
			case psx::video::PipelineType::SHADED_LINE: {
				m_shaded_line_pipeline.Draw(curr_offset, count);
				curr_primitive_index[(u32)pipeline_id] += count / 2;
			}
				break;
			case psx::video::PipelineType::ENUM_MAX:
				error::DebugBreak();
				break;
			default:
				error::Unreachable();
				break;
			}

			pipeline_offsets[(u32)pipeline_id] += count;

			m_commands.pop_front();

			if (m_draw_over_mask_disable) {
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			}
		}

		m_basic_gouraud_pipeline.ClearPrimitiveData();
		m_basic_gouraud_pipeline.ClearVertices();

		m_untextured_opaque_flat_pipeline.ClearPrimitiveData();
		m_untextured_opaque_flat_pipeline.ClearVertices();

		m_textured_pipeline.ClearPrimitiveData();
		m_textured_pipeline.ClearVertices();

		m_mono_line_pipeline.ClearPrimitiveData();
		m_mono_line_pipeline.ClearVertices();

		m_shaded_line_pipeline.ClearPrimitiveData();
		m_shaded_line_pipeline.ClearVertices();

		m_processing_cmd = true;
	}

	void Renderer::CommandFenceSync() {
		if (!m_processing_cmd)
			return;

		auto sync = glFenceSync(
			GL_SYNC_GPU_COMMANDS_COMPLETE,
			0
		);

		bool flag = true;

		while (flag)
		{
			auto res = glClientWaitSync(
				sync, GL_SYNC_FLUSH_COMMANDS_BIT,
				10000000
			);

			if (res == GL_ALREADY_SIGNALED ||
				res == GL_CONDITION_SATISFIED)
				flag = false;
		}

		glDeleteSync(sync);
		glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);

		m_processing_cmd = false;
		m_need_gpu_to_host_copy = true;
	}

	//Before ANY blit, flush commands, wait 
	//for commands to finish
	//To avoid downloading from GPU  memory to host
	//memory, we could 
	//1. write unmasked to CPU memory
	//2. copy that CPU memory to a blit texture
	//3. blit that texture to the output VRAM considering mask setting
	//4. Copy output VRAM to input VRAM
	//This has also the advantage of waiting for
	// previous commands to finish only after
	// the CPU-side blit has been performed,
	// which could speed up things significantly
	//Otherwise, by performing GPU-CPU download
	//1. after waiting commands to finish 
	//2. copy output VRAM to CPU memory (only required sub-image)
	//3. Perform blit
	//4. After the blit, copy CPU memory to GPU memory
	//5. Copy entire output VRAM to input VRAM
	//This has the disadvantage of needing to actually
	//copy part of the VRAM texture to CPU, and 
	//also the need of waiting for all commands 
	//to finish even before the blit has started

	void Renderer::VramCpuBlit(u32 xoff, u32 yoff, u32 w, u32 h) {
		//Immediately flush all commands and wait 
		//for completion
		FlushCommands();
		m_framebuffer.DownloadSubImage(m_vram.Get(), xoff, yoff, w, h);
	}

	void Renderer::BeginCpuVramBlit() {
		//after calling this method, CPU fills unmasked buffer 
		//therefore, we need to issue all draw commands
		//right now, but we don't want to wait for completion, yet
		UpdateUbo();
		DrawBatch();
	}

	void Renderer::PrepareBlit(bool mask_enable, bool set_mask) {
		CommandFenceSync();
		SyncTextures();

		auto gl_ctx = GetCurrentGLContext();
		gl_ctx->ScissorDisable();
		gl_ctx->BlendDisable();
		gl_ctx->SetViewport(0, 0, 1024ULL * m_resolution_multiplier, 512ULL * m_resolution_multiplier);

		m_framebuffer.Bind();
		m_blit_vertex_buf.Bind();
		m_blit_shader.BindProgram();

		m_blit_shader.UpdateUniform("mask_enable", mask_enable);
		m_blit_shader.UpdateUniform("set_mask", set_mask);
	}

	void Renderer::CpuVramBlit(u32 xoff, u32 yoff, u32 w, u32 h) {
		m_vram.UploadForBlit(xoff, yoff, w, h);

		m_blit_vertex_buf.Clear();

		m_blit_vertex_buf.PushVertex(BlitVertex{ xoff, yoff });
		m_blit_vertex_buf.PushVertex(BlitVertex{ xoff, yoff + h });
		m_blit_vertex_buf.PushVertex(BlitVertex{ xoff + w, yoff + h });

		m_blit_vertex_buf.PushVertex(BlitVertex{ xoff, yoff });
		m_blit_vertex_buf.PushVertex(BlitVertex{ xoff + w, yoff + h });
		m_blit_vertex_buf.PushVertex(BlitVertex{ xoff + w, yoff });

		auto gl_ctx = GetCurrentGLContext();
		gl_ctx->SetTextureSlot(GL_TEXTURE0);
		gl_ctx->BindTexture({ .type = GL_TEXTURE_2D, .handle = m_vram.GetTextureHandle() });
		gl_ctx->SetTextureSlot(GL_TEXTURE1);
		gl_ctx->BindTexture({ .type = GL_TEXTURE_2D, .handle = m_vram.GetBlitTextureHandle() });
		gl_ctx->SetTextureSlot(GL_TEXTURE0);

		gl_ctx->BindImage(3, {
			.texture = m_framebuffer.GetMaskTexture(),
			.level = 0,
			.layered = false,
			.layer = 0,
			.access = GL_READ_WRITE,
			.format = GL_R8
		});

		glDrawArrays(GL_TRIANGLES, 0, 6);

		m_processing_cmd = true;

		CommandFenceSync();
	}

	void Renderer::EndBlit() {
		SyncTextures();
	}

	void Renderer::VramVramBlit(u32 srcx, u32 srcy, u32 dstx, u32 dsty, u32 w, u32 h, bool mask_enable) {
		FlushCommands();
		SyncTextures();

		auto gl_ctx = GetCurrentGLContext();
		gl_ctx->ScissorDisable();
		gl_ctx->BlendDisable();
		gl_ctx->SetViewport(0, 0, 1024ULL * m_resolution_multiplier, 512ULL * m_resolution_multiplier);

		m_framebuffer.Bind();
		m_vram_blit_vertex_buf.Bind();
		m_vram_blit_shader.BindProgram();

		m_vram_blit_shader.UpdateUniform("mask_enable", mask_enable);

		m_vram_blit_vertex_buf.Clear();

		m_vram_blit_vertex_buf.PushVertex(VramBlitVertex{ dstx, dsty, srcx, srcy });
		m_vram_blit_vertex_buf.PushVertex(VramBlitVertex{ dstx, dsty + h, srcx, srcy + h });
		m_vram_blit_vertex_buf.PushVertex(VramBlitVertex{ dstx + w, dsty + h, srcx + w, srcy + h });

		m_vram_blit_vertex_buf.PushVertex(VramBlitVertex{ dstx, dsty, srcx, srcy });
		m_vram_blit_vertex_buf.PushVertex(VramBlitVertex{ dstx + w, dsty + h, srcx + w, srcy + h });
		m_vram_blit_vertex_buf.PushVertex(VramBlitVertex{ dstx + w, dsty, srcx + w, srcy });

		gl_ctx->SetTextureSlot(GL_TEXTURE0);
		gl_ctx->BindTexture({ .type = GL_TEXTURE_2D, .handle = m_vram.GetTextureHandle() });
		
		gl_ctx->BindImage(3, {
			.texture = m_framebuffer.GetMaskTexture(),
			.level = 0,
			.layered = false,
			.layer = 0,
			.access = GL_READ_WRITE,
			.format = GL_R8
		});

		glDrawArrays(GL_TRIANGLES, 0, 6);

		m_processing_cmd = true;
		CommandFenceSync();
		SyncTextures();
	}

	void Renderer::FlushCommands() {
		UpdateUbo();
		DrawBatch();
		CommandFenceSync();
	}

	void Renderer::UpdateUbo() {
		if (!m_update_ubo)
			return;

		m_update_ubo = false;
		m_uniform_buf.Upload();
	}

	void Renderer::Fill(i32 xoff, i32 yoff, u32 w, u32 h, u32 color) {
		m_need_gpu_to_host_copy = true;

		float r = ((color >> 3) & 0x1F) / 31.0f;
		float g = ((color >> 11) & 0x1F) / 31.0f;
		float b = ((color >> 19) & 0x1F) / 31.0f;

		m_framebuffer.Bind();

		auto gl_ctx = GetCurrentGLContext();
		gl_ctx->ScissorEnable();
		gl_ctx->BlendDisable();
		gl_ctx->SetViewport(0, 0, 1024ULL * m_resolution_multiplier, 512ULL * m_resolution_multiplier);

		glClearColor(r, g, b, 0.0);

		if (xoff + w > 1024 && yoff + h > 512) {
			gl_ctx->SetScissor(xoff * m_resolution_multiplier, yoff * m_resolution_multiplier,
				size_t(1024 - (GLsizei)xoff) * m_resolution_multiplier, 
				size_t(512 - (GLsizei)yoff) * m_resolution_multiplier);
			glClear(GL_COLOR_BUFFER_BIT);
			gl_ctx->SetScissor(0, 0,
				size_t((xoff + w) - 1024) * m_resolution_multiplier, 
				size_t((yoff + h) - 512) * m_resolution_multiplier);
			glClear(GL_COLOR_BUFFER_BIT);
			gl_ctx->SetScissor(0, yoff * m_resolution_multiplier,
				size_t((xoff + w) - 1024) * m_resolution_multiplier, 
				size_t(512 - (GLsizei)yoff) * m_resolution_multiplier);
			glClear(GL_COLOR_BUFFER_BIT);
			gl_ctx->SetScissor(xoff * m_resolution_multiplier, 0,
				size_t(1024 - (GLsizei)xoff) * m_resolution_multiplier, 
				size_t((yoff + h) - 512) * m_resolution_multiplier);
			glClear(GL_COLOR_BUFFER_BIT);
			
			glTextureSubImage2D(m_framebuffer.GetMaskTexture(),
				0, xoff, yoff, 1024 - xoff, 512 - yoff, GL_RED, GL_UNSIGNED_BYTE,
				m_blank_image.data());
			glTextureSubImage2D(m_framebuffer.GetMaskTexture(),
				0, 0, 0, (xoff + w) - 1024, (yoff + h) - 512, GL_RED, GL_UNSIGNED_BYTE,
				m_blank_image.data());
			glTextureSubImage2D(m_framebuffer.GetMaskTexture(),
				0, 0, yoff, (xoff + w) - 1024, 512 - yoff, GL_RED, GL_UNSIGNED_BYTE,
				m_blank_image.data());
			glTextureSubImage2D(m_framebuffer.GetMaskTexture(),
				0, xoff, 0, 1024 - xoff, (yoff + h) - 512, GL_RED, GL_UNSIGNED_BYTE,
				m_blank_image.data());
		}
		else if (xoff + w > 1024) {
			gl_ctx->SetScissor(xoff * m_resolution_multiplier, yoff * m_resolution_multiplier,
				size_t(1024 - (GLsizei)xoff) * m_resolution_multiplier, 
				size_t((GLsizei)h) * m_resolution_multiplier);
			glClear(GL_COLOR_BUFFER_BIT);
			gl_ctx->SetScissor(0, yoff * m_resolution_multiplier,
				size_t((xoff + w) - 1024) * m_resolution_multiplier, 
				size_t((GLsizei)h) * m_resolution_multiplier);
			glClear(GL_COLOR_BUFFER_BIT);

			glTextureSubImage2D(m_framebuffer.GetMaskTexture(),
				0, xoff, yoff, 1024 - xoff, h, GL_RED, GL_UNSIGNED_BYTE,
				m_blank_image.data());
			glTextureSubImage2D(m_framebuffer.GetMaskTexture(),
				0, 0, yoff, (xoff + w) - 1024, h, GL_RED, GL_UNSIGNED_BYTE,
				m_blank_image.data());
		}
		else if (yoff + h > 512) {
			gl_ctx->SetScissor(xoff * m_resolution_multiplier, yoff * m_resolution_multiplier,
				size_t((GLsizei)w) * m_resolution_multiplier, 
				size_t(512 - (GLsizei)yoff) * m_resolution_multiplier);
			glClear(GL_COLOR_BUFFER_BIT);
			gl_ctx->SetScissor(xoff * m_resolution_multiplier, 0,
				size_t((GLsizei)w) * m_resolution_multiplier, 
				size_t((yoff + h) - 512) * m_resolution_multiplier);
			glClear(GL_COLOR_BUFFER_BIT);

			glTextureSubImage2D(m_framebuffer.GetMaskTexture(),
				0, xoff, yoff, w, 512 - yoff, GL_RED, GL_UNSIGNED_BYTE,
				m_blank_image.data());
			glTextureSubImage2D(m_framebuffer.GetMaskTexture(),
				0, xoff, 0, w, (yoff + h) - 512, GL_RED, GL_UNSIGNED_BYTE,
				m_blank_image.data());
		}
		else {
			gl_ctx->SetScissor(xoff * m_resolution_multiplier, yoff * m_resolution_multiplier,
				size_t((GLsizei)w) * m_resolution_multiplier, 
				size_t((GLsizei)h) * m_resolution_multiplier);
			glClear(GL_COLOR_BUFFER_BIT);

			glTextureSubImage2D(m_framebuffer.GetMaskTexture(),
				0, xoff, yoff, w, h, GL_RED, GL_UNSIGNED_BYTE,
				m_blank_image.data());
		}
	}

	void Renderer::SetResolutionMultiplier(u32 mult) {
		FlushCommands();
		SyncTextures();
		m_framebuffer.RebuildUpscaledFbo(mult);
		m_resolution_multiplier = mult;
	}

	Renderer::~Renderer() {
		m_uniform_buf.Unbind();
	}
}