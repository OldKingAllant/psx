#include "UniformBuffer.hpp"
#include "GLContext.hpp"

#include <GL/glew.h>

namespace psx::video {
	uint32_t CreateUniformBuffer(uint32_t size) {
		if (size & 0xF) {
			size += 16;
			size &= ~0xF;
		}
		uint32_t id{ 0 };
		glGenBuffers(1, &id);

		auto gl_ctx = GetCurrentGLContext();
		gl_ctx->BindBuffer(GL_UNIFORM_BUFFER, id);
		
		glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
		
		return id;
	}

	void DeleteUniformBuffer(uint32_t ubo) {
		glDeleteBuffers(1, &ubo);
	}

	void UnbindUniformBuffer(uint32_t ubo) {
		(void)ubo;
		auto gl_ctx = GetCurrentGLContext();
		gl_ctx->BindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void BindUniformBuffer(uint32_t ubo) {
		auto gl_ctx = GetCurrentGLContext();
		gl_ctx->BindBuffer(GL_UNIFORM_BUFFER, ubo);
	}

	void BindUniformBufferRange(uint32_t ubo, uint32_t bind_point, uint32_t off, uint32_t size) {
		glBindBufferRange(GL_UNIFORM_BUFFER, bind_point, ubo, off, size);
	}

	void UploadUniformBufferData(uint32_t off, uint32_t size, void* data) {
		if (size & 0xF) {
			size += 16;
			size &= ~0xF;
		}
		glBufferSubData(GL_UNIFORM_BUFFER, off, size, data);
	}

	void UniformBufferLabel(std::string_view label, uint32_t buffer) {
		glObjectLabel(GL_BUFFER, buffer, (GLsizei)label.size(),
			label.data());
	}
}