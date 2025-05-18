#include "GLRenderer.hpp"

#include <common/Errors.hpp>

#include <fmt/format.h>
#include <array>

namespace psx::video {
	Renderer::Renderer() :
		m_vram{}, m_framebuffer{},
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
		m_shaded_line_pipeline(std::string("../shaders"), std::string("basic_gouraud"))
	{
		m_framebuffer.SetLabel("output_vram_fb");
		m_blit_shader.SetLabel("vram_blit_shader");
		m_untextured_opaque_flat_pipeline.SetLabel("untextured_opaque_flat_pipeline");
		m_basic_gouraud_pipeline.SetLabel("basic_gouraud_pipeline");
		m_textured_pipeline.SetLabel("textured_pipeline");
		m_blit_vertex_buf.SetLabel("vram_blit_vertex_buf");
		m_uniform_buf.SetLabel("global_uniform_buffer");
		m_uniform_buf.Bind();
		m_uniform_buf.Upload();
		m_uniform_buf.BindRange(2);
		m_vram_blit_vertex_buf.SetLabel("vram_vram_blit_vertex_buf");
		m_vram_blit_shader.SetLabel("vram_vram_blit_shader");
		m_mono_line_pipeline.SetLabel("mono_line_pipeline");
		m_shaded_line_pipeline.SetLabel("shaded_line_pipeline");
	}

	void Renderer::SyncTextures() {
		if (m_need_host_to_gpu_copy && m_need_gpu_to_host_copy) {
			fmt::println("[RENDERER] Texture are out of sync!");
			error::DebugBreak();
		}

		if (m_need_gpu_to_host_copy) {
			//Draw call happened:
			//1. Copy output VRAM to input VRAM
			//2. Download input VRAM to host mapped buffer
			m_framebuffer.CopyToTexture(m_vram.GetTextureHandle());
			m_need_gpu_to_host_copy = false;
		}

		if (m_need_host_to_gpu_copy) {
			m_framebuffer.UpdateInternalTexture(m_vram.GetTextureHandle());
			m_need_host_to_gpu_copy = false;
		}

		glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
	}

	void Renderer::VBlank() {
		FlushCommands();
		SyncTextures();
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

	void Renderer::DrawFlatUntexturedOpaque(UntexturedOpaqueFlatTriangle triangle) {
		if (m_untextured_opaque_flat_pipeline.VertexCount() >= MAX_VERTEX_COUNT)
			FlushCommands();
		
		m_untextured_opaque_flat_pipeline.PushVertex(triangle.v0);
		m_untextured_opaque_flat_pipeline.PushVertex(triangle.v1);
		m_untextured_opaque_flat_pipeline.PushVertex(triangle.v2);

		m_untextured_opaque_flat_pipeline.AddPrimitiveData({});

		DrawCommand cmd = {};

		cmd.vertex_count = 3;
		cmd.type = PipelineType::UNTEXTURED_OPAQUE_FLAT_TRIANGLE;
		cmd.semi_transparent = false;

		AppendCommand(cmd);
	}

	void Renderer::DrawBasicGouraud(BasicGouraudTriangle triangle) {
		if (m_basic_gouraud_pipeline.VertexCount() >= MAX_VERTEX_COUNT)
			FlushCommands();

		m_basic_gouraud_pipeline.PushVertex(triangle.v0);
		m_basic_gouraud_pipeline.PushVertex(triangle.v1);
		m_basic_gouraud_pipeline.PushVertex(triangle.v2);

		m_basic_gouraud_pipeline.AddPrimitiveData({});

		DrawCommand cmd = {};

		cmd.vertex_count = 3;
		cmd.type = PipelineType::BASIC_GOURAUD_TRIANGLE;
		cmd.semi_transparent = false;

		AppendCommand(cmd);
	}

	void Renderer::DrawTexturedTriangle(TexturedTriangle triangle) {
		if (m_textured_pipeline.VertexCount() >= MAX_VERTEX_COUNT)
			FlushCommands();

		m_textured_pipeline.PushVertex(triangle.v0);
		m_textured_pipeline.PushVertex(triangle.v1);
		m_textured_pipeline.PushVertex(triangle.v2);

		m_textured_pipeline.AddPrimitiveData({});

		DrawCommand cmd = {};

		cmd.vertex_count = 3;
		cmd.type = PipelineType::TEXTURED_TRIANGLE;
		cmd.semi_transparent = (triangle.v0.flags & TexturedVertexFlags::SEMI_TRANSPARENT) != 0;

		u16 page = u16(triangle.v0.clut_page);
		u8 semi_transparency = u8((page >> 5) & 0x3);
		cmd.semi_transparency_type = semi_transparency;

		AppendCommand(cmd);
	}

	void Renderer::DrawTransparentUntexturedTriangle(UntexturedOpaqueFlatTriangle triangle, u8 transparency_type) {
		if (m_untextured_opaque_flat_pipeline.VertexCount() >= MAX_VERTEX_COUNT)
			FlushCommands();

		m_untextured_opaque_flat_pipeline.PushVertex(triangle.v0);
		m_untextured_opaque_flat_pipeline.PushVertex(triangle.v1);
		m_untextured_opaque_flat_pipeline.PushVertex(triangle.v2);

		m_untextured_opaque_flat_pipeline.AddPrimitiveData({});

		DrawCommand cmd = {};

		cmd.vertex_count = 3;
		cmd.type = PipelineType::UNTEXTURED_OPAQUE_FLAT_TRIANGLE;
		cmd.semi_transparent = true;
		cmd.semi_transparency_type = transparency_type;

		AppendCommand(cmd);
	}

	void Renderer::DrawMonoLine(MonoLine line) {
		if (m_mono_line_pipeline.VertexCount() >= MAX_VERTEX_COUNT)
			FlushCommands();

		m_mono_line_pipeline.PushVertex(line.v0);
		m_mono_line_pipeline.PushVertex(line.v1);

		m_mono_line_pipeline.AddPrimitiveData({});

		DrawCommand cmd = {};

		cmd.vertex_count = 2;
		cmd.type = PipelineType::MONO_LINE;
		cmd.semi_transparent = false;

		AppendCommand(cmd);
	}

	void Renderer::DrawMonoTransparentLine(MonoLine line, u8 transparency_type) {
		if (m_mono_line_pipeline.VertexCount() >= MAX_VERTEX_COUNT)
			FlushCommands();

		m_mono_line_pipeline.PushVertex(line.v0);
		m_mono_line_pipeline.PushVertex(line.v1);

		m_mono_line_pipeline.AddPrimitiveData({});

		DrawCommand cmd = {};

		cmd.vertex_count = 2;
		cmd.type = PipelineType::MONO_LINE;
		cmd.semi_transparent = true;
		cmd.semi_transparency_type = transparency_type;

		AppendCommand(cmd);
	}

	void Renderer::DrawShadedLine(ShadedLine line) {
		if (m_shaded_line_pipeline.VertexCount() >= MAX_VERTEX_COUNT)
			FlushCommands();

		m_shaded_line_pipeline.PushVertex(line.v0);
		m_shaded_line_pipeline.PushVertex(line.v1);

		m_shaded_line_pipeline.AddPrimitiveData({});

		DrawCommand cmd = {};

		cmd.vertex_count = 2;
		cmd.type = PipelineType::SHADED_LINE;
		cmd.semi_transparent = false;

		AppendCommand(cmd);
	}

	void Renderer::DrawShadedTransparentLine(ShadedLine line, u8 transparency_type) {
		if (m_shaded_line_pipeline.VertexCount() >= MAX_VERTEX_COUNT)
			FlushCommands();

		m_shaded_line_pipeline.PushVertex(line.v0);
		m_shaded_line_pipeline.PushVertex(line.v1);

		m_shaded_line_pipeline.AddPrimitiveData({});

		DrawCommand cmd = {};

		cmd.vertex_count = 2;
		cmd.type = PipelineType::SHADED_LINE;
		cmd.semi_transparent = true;
		cmd.semi_transparency_type = transparency_type;

		AppendCommand(cmd);
	}

	void Renderer::DrawBatch() {
		if (m_commands.empty())
			return;

		UpdateUbo();

		glViewport(0, 0, 1024, 512);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_vram.GetTextureHandle());

		glScissor(m_scissor.top_x, m_scissor.top_y,
			m_scissor.bottom_x - m_scissor.top_x + 1,
			m_scissor.bottom_y - m_scissor.top_y + 1);

		glEnable(GL_SCISSOR_TEST);

		constexpr u32 PIPELINE_COUNT = (u32)PipelineType::ENUM_MAX;

		u32 pipeline_offsets[PIPELINE_COUNT] = {};
		u32 curr_primitive_index[PIPELINE_COUNT] = {};

		m_framebuffer.Bind();

		bool blend_enabled = glIsEnabled(GL_BLEND);

		while (!m_commands.empty()) {
			auto const& cmd = m_commands.front();

			if (cmd.semi_transparent) {
				if (!blend_enabled)
					glEnable(GL_BLEND);

				blend_enabled = true;

				if(cmd.semi_transparency_type == 2)
					glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);
				else 
					glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

				glBlendColor(0.25f, 0.25f, 0.25f, 0.5f);

				switch (cmd.semi_transparency_type)
				{
				case 0:
					glBlendFuncSeparate(GL_CONSTANT_ALPHA, GL_CONSTANT_ALPHA, GL_ONE, GL_ZERO);
					break;
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
			else {
				if (blend_enabled)
					glDisable(GL_BLEND);

				blend_enabled = false;
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
		}

		m_framebuffer.Unbind();

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

		glDisable(GL_SCISSOR_TEST);

		if (blend_enabled)
			glDisable(GL_BLEND);

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

	void Renderer::PrepareBlit(bool mask_enable) {
		CommandFenceSync();
		SyncTextures();

		glDisable(GL_SCISSOR_TEST);
		glViewport(0, 0, 1024, 512);

		m_framebuffer.Bind();
		m_blit_vertex_buf.Bind();
		m_blit_shader.BindProgram();

		m_blit_shader.UpdateUniform("mask_enable", mask_enable);
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

		glBindTexture(GL_TEXTURE_2D, m_vram.GetTextureHandle());
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m_vram.GetBlitTextureHandle());
		glActiveTexture(GL_TEXTURE0);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		m_processing_cmd = true;

		CommandFenceSync();
	}

	void Renderer::EndBlit() {
		m_blit_vertex_buf.Unbind();
		m_framebuffer.Unbind();
		SyncTextures();
	}

	void Renderer::VramVramBlit(u32 srcx, u32 srcy, u32 dstx, u32 dsty, u32 w, u32 h, bool mask_enable) {
		FlushCommands();
		SyncTextures();

		glDisable(GL_SCISSOR_TEST);
		glViewport(0, 0, 1024, 512);

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

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_vram.GetTextureHandle());

		glDrawArrays(GL_TRIANGLES, 0, 6);

		m_processing_cmd = true;
		CommandFenceSync();

		m_vram_blit_vertex_buf.Unbind();
		m_framebuffer.Unbind();
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

		glViewport(0, 0, 1024, 512);
		glEnable(GL_SCISSOR_TEST);
		glClearColor(r, g, b, 0.0);

		if (xoff + w > 1024 && yoff + h > 512) {
			glScissor(xoff, yoff, 1024 - (GLsizei)xoff, 512 - (GLsizei)yoff);
			glClear(GL_COLOR_BUFFER_BIT);
			glScissor(0, 0, (xoff + w) - 1024, (yoff + h) - 512);
			glClear(GL_COLOR_BUFFER_BIT);
			glScissor(0, yoff, (xoff + w) - 1024, 512 - (GLsizei)yoff);
			glClear(GL_COLOR_BUFFER_BIT);
			glScissor(xoff, 0, 1024 - (GLsizei)xoff, (yoff + h) - 512);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		else if (xoff + w > 1024) {
			glScissor(xoff, yoff, 1024 - (GLsizei)xoff, (GLsizei)h);
			glClear(GL_COLOR_BUFFER_BIT);
			glScissor(0, yoff, (xoff + w) - 1024, (GLsizei)h);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		else if (yoff + h > 512) {
			glScissor(xoff, yoff, (GLsizei)w, 512 - (GLsizei)yoff);
			glClear(GL_COLOR_BUFFER_BIT);
			glScissor(xoff, 0, (GLsizei)w, (yoff + h) - 512);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		else {
			glScissor(xoff, yoff, (GLsizei)w, (GLsizei)h);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		
		glDisable(GL_SCISSOR_TEST);

		m_framebuffer.Unbind();
	}

	Renderer::~Renderer() {
		m_uniform_buf.Unbind();
	}
}