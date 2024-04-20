#include "GLRenderer.hpp"

#include <common/Errors.hpp>

#include <fmt/format.h>
#include <array>
#include <thread>
#include <chrono>

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
		m_commands{}
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
	}

	void Renderer::VBlank() {
		FlushCommands();
	}

	void Renderer::AppendCommand(DrawCommand cmd) {
		if (m_commands.empty()) {
			m_commands.push_back(cmd);
		}
		else {
			auto& back = m_commands.back();

			if (back.type == cmd.type)
				back.vertex_count += cmd.vertex_count;
			else
				m_commands.push_back(cmd);
		}
	}

	void Renderer::DrawFlatUntexturedOpaque(UntexturedOpaqueFlatTriangle triangle) {
		m_untextured_opaque_flat_pipeline.PushVertex(triangle.v0);
		m_untextured_opaque_flat_pipeline.PushVertex(triangle.v1);
		m_untextured_opaque_flat_pipeline.PushVertex(triangle.v2);

		m_untextured_opaque_flat_pipeline.AddPrimitiveData({});

		DrawCommand cmd = {};

		cmd.vertex_count = 3;
		cmd.type = PipelineType::UNTEXTURED_OPAQUE_FLAT_TRIANGLE;

		AppendCommand(cmd);
	}

	void Renderer::DrawBasicGouraud(BasicGouraudTriangle triangle) {
		m_basic_gouraud_pipeline.PushVertex(triangle.v0);
		m_basic_gouraud_pipeline.PushVertex(triangle.v1);
		m_basic_gouraud_pipeline.PushVertex(triangle.v2);

		m_basic_gouraud_pipeline.AddPrimitiveData({});

		DrawCommand cmd = {};

		cmd.vertex_count = 3;
		cmd.type = PipelineType::BASIC_GOURAUD_TRIANGLE;

		AppendCommand(cmd);
	}

	void Renderer::DrawTexturedTriangle(TexturedTriangle triangle) {
		m_textured_pipeline.PushVertex(triangle.v0);
		m_textured_pipeline.PushVertex(triangle.v1);
		m_textured_pipeline.PushVertex(triangle.v2);

		m_textured_pipeline.AddPrimitiveData({});

		DrawCommand cmd = {};

		cmd.vertex_count = 3;
		cmd.type = PipelineType::TEXTURED_TRIANGLE;

		AppendCommand(cmd);
	}

	void Renderer::DrawBatch() {
		if (m_commands.empty())
			return;

		UpdateUbo();

		glViewport(0, 0, 1024, 512);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_vram.GetTextureHandle());

		if (m_update_scissor) {
			m_update_scissor = false;
			glScissor(m_scissor.top_x, m_scissor.top_y,
				m_scissor.bottom_x - m_scissor.top_x,
				m_scissor.bottom_y - m_scissor.top_y);
		}

		glEnable(GL_SCISSOR_TEST);

		constexpr u32 PIPELINE_COUNT = (u32)PipelineType::ENUM_MAX;

		u32 pipeline_offsets[PIPELINE_COUNT] = {};
		u32 curr_primitive_index[PIPELINE_COUNT] = {};

		m_framebuffer.Bind();

		while (!m_commands.empty()) {
			auto const& cmd = m_commands.front();

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
			case psx::video::PipelineType::ENUM_MAX:
				error::DebugBreak();
				break;
			default:
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

		glDisable(GL_SCISSOR_TEST);

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

		glViewport(0, 0, 1024, 512);

		m_framebuffer.Bind();
		m_blit_vertex_buf.Bind();
		m_blit_shader.BindProgram();

		m_blit_shader.UpdateUniform("mask_enable", mask_enable);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_vram.GetTextureHandle());
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m_vram.GetBlitTextureHandle());
		glActiveTexture(GL_TEXTURE0);
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

		glDrawArrays(GL_TRIANGLES, 0, 6);

		m_processing_cmd = true;

		CommandFenceSync();
	}

	void Renderer::EndBlit() {
		m_blit_vertex_buf.Unbind();
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

	Renderer::~Renderer() {
		m_uniform_buf.Unbind();
	}
}