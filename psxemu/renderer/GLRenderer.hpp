#pragma once

#include "Vram.hpp"
#include "FrameBuffer.hpp"
#include "Pipeline.hpp"
#include "VertexBuffer.hpp"
#include "Shader.hpp"

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
		UNTEXTURE_OPAQUE_FLAT_TRIANGLE,
		BASIC_GOURAUD_TRIANGLE,
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

	class Renderer {
	public :
		Renderer();

		void VBlank();

		void VramCpuBlit(u32 xoff, u32 yoff, u32 w, u32 h);
		void BeginCpuVramBlit();
		void EndCpuVramBlit(u32 xoff, u32 yoff, u32 w, u32 h, bool mask_enable);

		void FlushCommands();

		void SyncTextures();

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

		void DrawBatch();

		void AppendCommand(DrawCommand cmd);

		void CommandFenceSync();

		~Renderer();

	private :
		Vram m_vram;
		FrameBuffer m_framebuffer;
		bool m_need_gpu_to_host_copy;
		bool m_need_host_to_gpu_copy;
		bool m_processing_cmd;
		Pipeline<Primitive::TRIANGLES,
			UntexturedOpaqueFlatVertex, NullData> m_untextured_opaque_flat_pipeline;
		Pipeline<Primitive::TRIANGLES,
			BasicGouraudVertex, NullData> m_basic_gouraud_pipeline;
		VertexBuffer<BlitVertex> m_blit_vertex_buf;
		Shader m_blit_shader;
		std::list<DrawCommand> m_commands;
	};
}