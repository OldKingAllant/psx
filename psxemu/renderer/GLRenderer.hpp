#pragma once

#include "Vram.hpp"
#include "FrameBuffer.hpp"
#include "Pipeline.hpp"
#include "VertexBuffer.hpp"
#include "Shader.hpp"
#include "UniformBuffer.hpp"
#include "Renderdoc.hpp"

#include <list>

namespace psx::video {
	struct UntexturedOpaqueFlatVertex {
#pragma pack(push, 1)
		int32_t x, y;
		u32 r, g, b;
#pragma pack(pop)

		std::vector<VertexAttribute> attributes() const {
			return {
				VertexAttribute{ VertexAttributeType::INT, (u8*)&x - (u8*)this },
				VertexAttribute{ VertexAttributeType::INT, (u8*)&y - (u8*)this },
				VertexAttribute{ VertexAttributeType::UVEC3, (u8*)&r - (u8*)this }
			};
		}
	};

	struct BasicGouraudVertex {
#pragma pack(push, 1)
		int32_t x, y;
		uint32_t color;
#pragma pack(pop)

		std::vector<VertexAttribute> attributes() const {
			return {
				VertexAttribute{ VertexAttributeType::INT, (u8*)&x - (u8*)this },
				VertexAttribute{ VertexAttributeType::INT, (u8*)&y - (u8*)this },
				VertexAttribute{ VertexAttributeType::UNSIGNED_INT, (u8*)&color - (u8*)this },
			};
		}
	};

	enum TexturedVertexFlags : uint32_t {
		GOURAUD = 1,
		SEMI_TRANSPARENT = 2,
		RAW_TEXTURE = 4
	};

#pragma pack(push, 1)
	struct TexturedVertex {
		int32_t x, y;
		uint32_t color;
		uint32_t clut_page;
		uint32_t flags;
		uint32_t uv;
#pragma pack(pop)

		std::vector<VertexAttribute> attributes() const {
			return {
				VertexAttribute{ VertexAttributeType::INT, (u8*)&x - (u8*)this },
				VertexAttribute{ VertexAttributeType::INT, (u8*)&y - (u8*)this },
				VertexAttribute{ VertexAttributeType::UNSIGNED_INT, (u8*)&color - (u8*)this },
				VertexAttribute{ VertexAttributeType::UNSIGNED_INT, (u8*)&clut_page - (u8*)this },
				VertexAttribute{ VertexAttributeType::UNSIGNED_INT, (u8*)&flags - (u8*)this },
				VertexAttribute{ VertexAttributeType::UNSIGNED_INT, (u8*)&uv - (u8*)this }
			};
		}
	};


	struct UntexturedOpaqueFlatTriangle {
		UntexturedOpaqueFlatVertex v0;
		UntexturedOpaqueFlatVertex v1;
		UntexturedOpaqueFlatVertex v2;
	};

	struct BasicGouraudTriangle {
		BasicGouraudVertex v0;
		BasicGouraudVertex v1;
		BasicGouraudVertex v2;
	};

	struct TexturedTriangle {
		TexturedVertex v0;
		TexturedVertex v1;
		TexturedVertex v2;
	};

	struct BlitVertex {
#pragma pack(push, 1)
		u32 x, y;
#pragma pack(pop)

		std::vector<VertexAttribute> attributes() const {
			return {
				VertexAttribute{ VertexAttributeType::UVEC2, (u8*)&x - (u8*)this }
			};
		}
	};

	enum class PipelineType {
		UNTEXTURED_OPAQUE_FLAT_TRIANGLE,
		BASIC_GOURAUD_TRIANGLE,
		TEXTURED_TRIANGLE,
		ENUM_MAX //Use this to get the number
				//of used elements in the enum
	};

	struct DrawCommand {
		PipelineType type;
		u32 vertex_count;
	};

	struct Color {
		u32 r, g, b;
	};

	struct NullData {};

	struct GlobalUniforms {
#pragma pack(push, 1)
		uint32_t use_dither;
		uint32_t tex_window_mask_x;
		uint32_t tex_window_mask_y;
		uint32_t tex_window_off_x;
		uint32_t tex_window_off_y;
		int32_t draw_x_off;
		int32_t draw_y_off;
		uint32_t set_mask;
		uint32_t check_mask;
#pragma pack(pop)
	};

	struct ScissorBox {
		uint32_t top_x;
		uint32_t top_y;
		uint32_t bottom_x;
		uint32_t bottom_y;
	};

	class Renderer {
	public :
		Renderer();

		void VBlank();

		void VramCpuBlit(u32 xoff, u32 yoff, u32 w, u32 h);
		void BeginCpuVramBlit();
		void PrepareBlit(bool mask_enable);
		void CpuVramBlit(u32 xoff, u32 yoff, u32 w, u32 h);
		void EndBlit();

		void Fill(i32 xoff, i32 yoff, u32 w, u32 h, u32 color);

		void FlushCommands();
		void SyncTextures();
		void UpdateUbo();

		u8* GetVramPtr() const {
			return m_vram.Get();
		}

		Vram& GetVram() {
			return m_vram;
		}

		FrameBuffer& GetFramebuffer() {
			return m_framebuffer;
		}

		void DrawFlatUntexturedOpaque(UntexturedOpaqueFlatTriangle triangle);
		void DrawBasicGouraud(BasicGouraudTriangle triangle);
		void DrawTexturedTriangle(TexturedTriangle triangle);

		void DrawBatch();

		void AppendCommand(DrawCommand cmd);

		void CommandFenceSync();

		GlobalUniforms& GetUniformBuffer() {
			return m_uniform_buf.buffer;
		}

		void RequestUniformBufferUpdate() {
			m_update_ubo = true;
		}

		void SetScissorTop(uint32_t x, uint32_t y) {
			m_scissor.top_x = x;
			m_scissor.top_y = y;
			m_update_scissor = true;
		}

		void SetScissorBottom(uint32_t x, uint32_t y) {
			m_scissor.bottom_x = x;
			m_scissor.bottom_y = y;
			m_update_scissor = true;
		}

		void SetRenderdocAPI(Renderdoc* renderdoc) {
			m_renderdoc = renderdoc;
		}

		FORCE_INLINE Renderdoc* GetRenderDoc() const {
			return m_renderdoc;
		}

		~Renderer();

	private :
		Vram m_vram;
		FrameBuffer m_framebuffer;
		bool m_need_gpu_to_host_copy;
		bool m_need_host_to_gpu_copy;
		bool m_processing_cmd;
		bool m_update_ubo;
		bool m_update_scissor;
		Pipeline<Primitive::TRIANGLES,
			UntexturedOpaqueFlatVertex, NullData> m_untextured_opaque_flat_pipeline;
		Pipeline<Primitive::TRIANGLES,
			BasicGouraudVertex, NullData> m_basic_gouraud_pipeline;
		Pipeline<Primitive::TRIANGLES,
			TexturedVertex, NullData> m_textured_pipeline;
		VertexBuffer<BlitVertex> m_blit_vertex_buf;
		Shader m_blit_shader;
		UniformBuffer<GlobalUniforms> m_uniform_buf;
		ScissorBox m_scissor;
		std::list<DrawCommand> m_commands;
		Renderdoc* m_renderdoc;
	};
}