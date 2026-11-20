#pragma once

#include "Vram.hpp"
#include "FrameBuffer.hpp"
#include "Pipeline.hpp"
#include "VertexBuffer.hpp"
#include "Shader.hpp"
#include "UniformBuffer.hpp"
#include "UniformRingBuffer.hpp"
#include "Renderdoc.hpp"
#include "GLContext.hpp"

#include <list>
#include <optional>

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
		RAW_TEXTURE = 4,
		TEXTURED = 8
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

	struct MonoLine {
		UntexturedOpaqueFlatVertex v0;
		UntexturedOpaqueFlatVertex v1;
	};

	struct BasicGouraudTriangle {
		BasicGouraudVertex v0;
		BasicGouraudVertex v1;
		BasicGouraudVertex v2;
	};

	struct ShadedLine {
		BasicGouraudVertex v0;
		BasicGouraudVertex v1;
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

	struct VramBlitVertex {
#pragma pack(push, 1)
		u32 dstx, dsty;
		u32 srcx, srcy;
#pragma pack(pop)

		std::vector<VertexAttribute> attributes() const {
			return {
				VertexAttribute{ VertexAttributeType::UVEC2, (u8*)&dstx - (u8*)this },
				VertexAttribute{ VertexAttributeType::UVEC2, (u8*)&srcx - (u8*)this }
			};
		}
	};

	enum class PipelineType {
		UNTEXTURED_OPAQUE_FLAT_TRIANGLE,
		BASIC_GOURAUD_TRIANGLE,
		TEXTURED_TRIANGLE,
		MONO_LINE,
		SHADED_LINE,
		ENUM_MAX //Use this to get the number
				//of used elements in the enum
	};

	struct GenericVertex {
		int32_t x, y;
		uint32_t color;
		uint32_t clut_page;
		uint32_t flags;
		uint32_t uv;
	};

	struct GenericPrimitive {
		GenericVertex vertices[3];
		PipelineType type;
		bool semi_transparent;
		u8 semi_transparency_type;
		u8 vertex_count;
	};

	struct DrawCommand {
		PipelineType type;
		u32          vertex_count;
		bool         semi_transparent;
		u8           semi_transparency_type;
		u64          uniform_buffer_index;
	};

	struct Color {
		u32 r, g, b;
	};

	struct PerPrimitiveData {
#pragma pack(push, 1)
		u32 uniform_buffer_index;
#pragma pack(pop)

		std::vector<VertexAttribute> attributes() const {
			return {
				VertexAttribute{ VertexAttributeType::UNSIGNED_INT, (u8*)&uniform_buffer_index - (u8*)this }
			};
		}
	};

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
		uint32_t resolution_multiplier;
		uint32_t _unused1;
		uint32_t _unused2;
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
		void PrepareBlit(bool mask_enable, bool set_mask);
		void CpuVramBlit(u32 xoff, u32 yoff, u32 w, u32 h);
		void EndBlit();

		void VramVramBlit(u32 srcx, u32 srcy, u32 dstx, u32 dsty,
			u32 w, u32 h, bool mask_enable);

		void Fill(i32 xoff, i32 yoff, u32 w, u32 h, u32 color);

		void FlushCommands();
		void SyncTextures();

		void CopyUboToSlotAndVerifyPreemption();
		void CheckUboSlot(uint64_t _buf_id);

		u8* GetVramPtr() const {
			return m_vram.Get();
		}

		Vram& GetVram() {
			return m_vram;
		}

		FrameBuffer& GetFramebuffer() {
			return m_framebuffer;
		}

		void DrawBatch();

		void DrawPrimitive(GenericPrimitive const& primitive);
		void AppendCommand(DrawCommand cmd);

		void CommandFenceSync();

		/////////////////////////////////////////////////////

		static constexpr size_t NUM_RINGBUFFER_SLOTS = 64;

		void SetDitherEnable(bool _enable_dither) {
			if (m_temp_uniforms.use_dither == (u32)_enable_dither) {
				return;
			}
			m_temp_uniforms.use_dither = _enable_dither;
			CopyUboToSlotAndVerifyPreemption();
			m_update_ubo = true;
		}

		void SetMaskEnable(bool _enable_set_mask) {
			if (m_temp_uniforms.set_mask == (u32)_enable_set_mask) {
				return;
			}
			m_temp_uniforms.set_mask = _enable_set_mask;
			CopyUboToSlotAndVerifyPreemption();
			m_update_ubo = true;
		}

		void SetCheckMaskEnable(bool _enable_check_mask) {
			if (m_temp_uniforms.check_mask == (u32)_enable_check_mask) {
				return;
			}
			m_temp_uniforms.check_mask = _enable_check_mask;
			m_draw_over_mask_disable = _enable_check_mask;
			CopyUboToSlotAndVerifyPreemption();
			m_update_ubo = true;
		}

		void SetTextureWindowMaskX(uint32_t _tex_window_mask_x) {
			if (m_temp_uniforms.tex_window_mask_x == _tex_window_mask_x) {
				return;
			}
			m_temp_uniforms.tex_window_mask_x = _tex_window_mask_x;
			CopyUboToSlotAndVerifyPreemption();
			m_update_ubo = true;
		}

		void SetTextureWindowMaskY(uint32_t _tex_window_mask_y) {
			if (m_temp_uniforms.tex_window_mask_y == _tex_window_mask_y) {
				return;
			}
			m_temp_uniforms.tex_window_mask_y = _tex_window_mask_y;
			CopyUboToSlotAndVerifyPreemption();
			m_update_ubo = true;
		}

		void SetTextureWindowOffX(uint32_t _tex_window_off_x) {
			if (m_temp_uniforms.tex_window_off_x == _tex_window_off_x) {
				return;
			}
			m_temp_uniforms.tex_window_off_x = _tex_window_off_x;
			CopyUboToSlotAndVerifyPreemption();
			m_update_ubo = true;
		}

		void SetTextureWindowOffY(uint32_t _tex_window_off_y) {
			if (m_temp_uniforms.tex_window_off_y == _tex_window_off_y) {
				return;
			}
			m_temp_uniforms.tex_window_off_y = _tex_window_off_y;
			CopyUboToSlotAndVerifyPreemption();
			m_update_ubo = true;
		}

		void SetDrawOffX(uint32_t _draw_x_off) {
			if (m_temp_uniforms.draw_x_off == _draw_x_off) {
				return;
			}
			m_temp_uniforms.draw_x_off = _draw_x_off;
			CopyUboToSlotAndVerifyPreemption();
			m_update_ubo = true;
		}

		void SetDrawOffY(uint32_t _draw_y_off) {
			if (m_temp_uniforms.draw_y_off == _draw_y_off) {
				return;
			}
			m_temp_uniforms.draw_y_off = _draw_y_off;
			CopyUboToSlotAndVerifyPreemption();
			m_update_ubo = true;
		}

		////////////////////////////////////////////////////

		void SetScissorTop(uint32_t x, uint32_t y) {
			m_scissor.top_x = x;
			m_scissor.top_y = y;
			m_update_scissor = true;
			DrawBatch();
		}

		void SetScissorBottom(uint32_t x, uint32_t y) {
			m_scissor.bottom_x = x;
			m_scissor.bottom_y = y;
			m_update_scissor = true;
			DrawBatch();
		}

		void SetRenderdocAPI(Renderdoc* renderdoc) {
			m_renderdoc = renderdoc;
		}

		FORCE_INLINE Renderdoc* GetRenderDoc() const {
			return m_renderdoc;
		}

		void SetResolutionMultiplier(u32 mult);

		GLContext* GetContext() const {
			return m_gl_ctx;
		}

		~Renderer();

	private :
		u32                                                              m_resolution_multiplier;
		Vram                                                             m_vram;
		FrameBuffer                                                      m_framebuffer;
		bool                                                             m_need_gpu_to_host_copy;
		bool                                                             m_need_host_to_gpu_copy;
		bool                                                             m_processing_cmd;
		bool                                                             m_update_scissor;
		Pipeline<Primitive::TRIANGLES, TexturedVertex, PerPrimitiveData> m_textured_pipeline;
		VertexBuffer                                                     m_blit_vertex_buf;
		Shader                                                           m_blit_shader;
		UniformRingBuffer                                                m_uniform_buf;
		GlobalUniforms                                                   m_temp_uniforms;
		uint64_t                                                         m_curr_buffer_id;
		bool                                                             m_update_ubo;
		bool                                                             m_pushed_new_commands;
		uint64_t                                                         m_last_synced_id;
		uint64_t                                                         m_last_flushed_buf_id;
		ScissorBox                                                       m_scissor;
		std::list<DrawCommand>                                           m_commands;
		Renderdoc*                                                       m_renderdoc;
		VertexBuffer                                                     m_vram_blit_vertex_buf;
		Shader                                                           m_vram_blit_shader;
		Pipeline<Primitive::LINES, UntexturedOpaqueFlatVertex, PerPrimitiveData> m_mono_line_pipeline;
		Pipeline<Primitive::LINES, BasicGouraudVertex, PerPrimitiveData>         m_shaded_line_pipeline;
		bool                                                             m_draw_over_mask_disable;
		std::vector<u8>                                                  m_blank_image;
		GLContext*                                                       m_gl_ctx;
	};
}