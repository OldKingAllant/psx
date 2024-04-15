#include "GLRenderer.hpp"

#include <common/Errors.hpp>

#include <fmt/format.h>
#include <array>

namespace psx::video {
	Renderer::Renderer() :
		m_vram{}, m_framebuffer{},
		m_need_gpu_to_host_copy{false},
		m_need_host_to_gpu_copy{false}, 
		m_untextured_opaque_flat_pipeline(std::string("../shaders"), std::string("flat_untextured_opaque_triangle")),
		m_basic_gouraud_pipeline(std::string("../shaders"), std::string("basic_gouraud")),
		m_commands{}
	{}

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

		Color triangle_color = {};

		triangle_color.r = triangle.r;
		triangle_color.g = triangle.g;
		triangle_color.b = triangle.b;

		m_untextured_opaque_flat_pipeline.AddPrimitiveData(triangle_color);

		DrawCommand cmd = {};

		cmd.vertex_count = 3;
		cmd.type = PipelineType::UNTEXTURE_OPAQUE_FLAT_TRIANGLE;

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

	void Renderer::DrawBatch() {
		if (m_commands.empty())
			return;

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
			case psx::video::PipelineType::UNTEXTURE_OPAQUE_FLAT_TRIANGLE: {
				u32 rem = count;

				while (rem) {
					Color color = m_untextured_opaque_flat_pipeline.GetPrimitiveData(
						curr_primitive
					);

					m_untextured_opaque_flat_pipeline
						.Draw(curr_offset, 3, 
							std::pair{ "in_color", std::array<unsigned int, 3>{
								color.r, color.g, color.b
							}}
					);

					rem -= 3;
					curr_primitive++;
					curr_offset += 3;
				}

				curr_primitive_index[(u32)pipeline_id] += count / 3;
			}
				break;
			case psx::video::PipelineType::BASIC_GOURAUD_TRIANGLE: {
				m_basic_gouraud_pipeline.Draw(curr_offset, count);
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

		m_need_gpu_to_host_copy = true;

		CommandFenceSync();
	}

	void Renderer::CommandFenceSync() {
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

	}

	void Renderer::BlitBegin(u32 xoff, u32 yoff, u32 w, u32 h) {
		//Flush all commands since:
		//- CPU-VRAM blits will modify behaviour in most cases
		//- VRAM-CPU blits require updated VRAM 
		FlushCommands();
	}

	void Renderer::BlitEnd(u32 xoff, u32 yoff, u32 w, u32 h) {
		m_vram.UploadSubImage(xoff, yoff, w, h);
		m_framebuffer.UpdatePartial(
			m_vram.GetTextureHandle(),
			xoff, yoff, w, h
		);
	}

	void Renderer::FlushCommands() {
		SyncTextures();
		DrawBatch();
		SyncTextures();
	}

	Renderer::~Renderer() {
		//
	}
}