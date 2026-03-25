#pragma once

#include <common/Macros.hpp>
#include <common/Defs.hpp>

#include <unordered_map>

namespace psx::video {
	struct GLViewport {
		i32 x;
		i32 y;
		size_t w;
		size_t h;
	};

	struct GLScissor {
		i32 x;
		i32 y;
		size_t w;
		size_t h;
	};

	struct GLTextureBinding {
		u32 type;
		u32 handle;
	};

	struct GLImageBinding {
		/*
		GLuint unit,
 		GLuint texture,
 		GLint level,
 		GLboolean layered,
 		GLint layer,
 		GLenum access,
 		GLenum format
		*/
		u32 texture;
		i32 level;
		bool layered;
		i32 layer;
		i32 access;
		u32 format;
	};

	 static bool operator==(GLImageBinding const& l, GLImageBinding const& r) {
		return l.texture == r.texture && l.level == r.level &&
			l.layered == r.layered && l.layer == r.layer &&
			l.access == r.access && l.format == r.format;
	}

	struct GLContext {
		static GLContext Create(void* window);

		GLContext();

		FORCE_INLINE void* GetHandle() const {
			return m_handle;
		}

		void SetCurrent(void* win_handle);

		void SetViewport(i32 x, i32 y, size_t w, size_t h);
		FORCE_INLINE GLViewport GetViewport() const {
			return m_viewport;
		}

		void SetScissor(i32 x, i32 y, size_t w, size_t h);
		FORCE_INLINE GLScissor GetScissor() const {
			return m_scissor;
		}

		void SetTextureSlot(u32 slot);
		void BindTexture(GLTextureBinding binding);

		void BindProgram(u32 program_id);

		void BindVao(u32 vao_id);

		void BindBuffer(u32 bind_point, u32 handle);

		void ScissorEnable();
		void ScissorDisable();

		void BlendEnable();
		void BlendDisable();

		void BindImage(u32 slot, GLImageBinding binding);

		void BindFramebuffer(u32 target, u32 fbo);

		~GLContext();

		void* m_handle;
		GLViewport m_viewport;
		u32 m_texture_slot;
		std::unordered_map<u32, GLTextureBinding> m_texture_bindings;
		u32 m_program_id;
		u32 m_vao;
		std::unordered_map<u32, u32> m_buffer_bindings;
		GLScissor m_scissor;
		bool m_enable_scissor;
		bool m_enable_blend;
		std::unordered_map<u32, GLImageBinding> m_image_bindings;
		std::unordered_map<u32, u32> m_framebuffer_bindings;
	};

	extern thread_local GLContext* g_curr_context;

	void SetCurrentGLContext(GLContext* ctx);
	GLContext* GetCurrentGLContext();
}