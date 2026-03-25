#include "GLContext.hpp"

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <SDL2/SDL.h>
#include <GL/glew.h>

namespace psx::video {
	GLContext GLContext::Create(void* window) {
		auto new_ctx = GLContext{};
		new_ctx.m_handle = (void*)SDL_GL_CreateContext((SDL_Window*)window);
		return new_ctx;
	}

	GLContext::GLContext() :
		m_handle{},
		m_viewport{},
		m_texture_slot{},
		m_texture_bindings{},
		m_program_id{},
		m_vao{},
		m_buffer_bindings{},
		m_scissor{},
		m_enable_scissor{},
		m_enable_blend{},
		m_image_bindings{}
	{
	}

	void GLContext::SetCurrent(void* win_handle) {
		SDL_GL_MakeCurrent((SDL_Window*)win_handle, (SDL_GLContext)m_handle);
		SetCurrentGLContext(this);
	}

	void GLContext::SetViewport(i32 x, i32 y, size_t w, size_t h) {
		if (m_viewport.x == x && m_viewport.y == y && m_viewport.w == w && m_viewport.h == h)
			return;

		m_viewport.x = x;
		m_viewport.y = y;
		m_viewport.w = w;
		m_viewport.h = h;

		glViewport((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h);
	}

	void GLContext::SetScissor(i32 x, i32 y, size_t w, size_t h) {
		if (m_scissor.x == x && m_scissor.y == y && m_scissor.w == w && m_scissor.h == h)
			return;

		m_scissor.x = x;
		m_scissor.y = y;
		m_scissor.w = w;
		m_scissor.h = h;

		glScissor((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h);
	}

	void GLContext::SetTextureSlot(u32 slot) {
		if (m_texture_slot == slot) {
			return;
		}
		m_texture_slot = slot;
		glActiveTexture((GLenum)slot);
	}

	void GLContext::BindTexture(GLTextureBinding binding) {
		if (!m_texture_bindings.contains(m_texture_slot)) {
			m_texture_bindings[m_texture_slot] = binding;
			glBindTexture((GLenum)binding.type, (GLuint)binding.handle);
			return;
		}

		auto const& old_binding = m_texture_bindings[m_texture_slot];
		if (old_binding.type == binding.type && old_binding.handle == binding.handle) {
			return;
		}

		m_texture_bindings[m_texture_slot] = binding;
		glBindTexture((GLenum)binding.type, (GLuint)binding.handle);
	}

	void GLContext::BindProgram(u32 program_id) {
		if (m_program_id == program_id) {
			return;
		}
		m_program_id = program_id;
		glUseProgram((GLuint)program_id);
	}
	
	void GLContext::BindVao(u32 vao_id) {
		if (m_vao == vao_id) {
			return;
		}
		m_vao = vao_id;
		glBindVertexArray((GLuint)m_vao);
	}

	void GLContext::BindBuffer(u32 bind_point, u32 handle) {
		if (!m_buffer_bindings.contains(bind_point)) {
			m_buffer_bindings[bind_point] = handle;
			glBindBuffer((GLenum)bind_point, (GLuint)handle);
			return;
		}

		auto const& old_binding = m_buffer_bindings[bind_point];
		if (old_binding == handle) {
			return;
		}

		m_buffer_bindings[bind_point] = handle;
		glBindBuffer((GLenum)bind_point, (GLuint)handle);
	}

	void GLContext::ScissorEnable() {
		if (m_enable_scissor) {
			return;
		}
		m_enable_scissor = true;
		glEnable(GL_SCISSOR_TEST);
	}

	void GLContext::ScissorDisable() {
		if (!m_enable_scissor) {
			return;
		}
		m_enable_scissor = false;
		glDisable(GL_SCISSOR_TEST);
	}

	void GLContext::BlendEnable() {
		if (m_enable_blend) {
			return;
		}
		m_enable_blend = true;
		glEnable(GL_BLEND);
	}

	void GLContext::BlendDisable() {
		if (!m_enable_blend) {
			return;
		}
		m_enable_blend = false;
		glDisable(GL_BLEND);
	}

	void GLContext::BindImage(u32 slot, GLImageBinding binding) {
		if (!m_image_bindings.contains(slot)) {
			m_image_bindings[slot] = binding;
			glBindImageTexture((GLuint)slot, (GLuint)binding.texture,
				(GLint)binding.level, (GLboolean)binding.layered,
				(GLint)binding.layer, (GLenum)binding.access,
				(GLenum)binding.format);
			return;
		}

		auto const& old_binding = m_image_bindings[slot];
		if (old_binding == binding) {
			return;
		}

		m_image_bindings[slot] = binding;
		glBindImageTexture((GLuint)slot, (GLuint)binding.texture,
			(GLint)binding.level, (GLboolean)binding.layered,
			(GLint)binding.layer, (GLenum)binding.access,
			(GLenum)binding.format);
	}

	void GLContext::BindFramebuffer(u32 target, u32 fbo) {
		if (!m_framebuffer_bindings.contains(target)) {
			m_framebuffer_bindings[target] = fbo;
			glBindFramebuffer((GLenum)target, (GLuint)fbo);
			return;
		}

		auto const& old_binding = m_framebuffer_bindings[target];
		if (old_binding == fbo) {
			return;
		}

		m_framebuffer_bindings[target] = fbo;
		glBindFramebuffer((GLenum)target, (GLuint)fbo);
	}

	GLContext::~GLContext() {
		SDL_GL_DeleteContext((SDL_GLContext)m_handle);
	}

	thread_local GLContext* g_curr_context = nullptr;

	void SetCurrentGLContext(GLContext* ctx) {
		g_curr_context = ctx;
	}

	GLContext* GetCurrentGLContext() {
		return g_curr_context;
	}
}