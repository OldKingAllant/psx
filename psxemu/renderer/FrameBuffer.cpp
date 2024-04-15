#include "FrameBuffer.hpp"
#include "GlLoad.hpp"

#include <GL/glew.h>

#include <fmt/format.h>
#include <stdexcept>

namespace psx::video {
	FrameBuffer::FrameBuffer() :
		m_fbo{}, m_fbo_tex{} {
		glGenFramebuffers(1, &m_fbo);
		glGenTextures(1, &m_fbo_tex);

		glBindTexture(GL_TEXTURE_2D, m_fbo_tex);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 1024, 512,
			0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, nullptr);

		glBindTexture(GL_TEXTURE_2D, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, m_fbo_tex, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			fmt::println("[RENDERER] Framebuffer creation failed!");
			throw std::runtime_error("Framebuffer error");
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void FrameBuffer::Bind() {
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	}

	void FrameBuffer::Unbind() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void FrameBuffer::UpdateInternalTexture(u32 blit_tex) {
		glCopyImageSubData(blit_tex, GL_TEXTURE_2D,
			0, 0, 0, 0, m_fbo_tex, GL_TEXTURE_2D,
			0, 0, 0, 0, 1024, 512, 1);
	}

	void FrameBuffer::CopyToTexture(u32 dest) {
		glCopyImageSubData(m_fbo_tex, GL_TEXTURE_2D, 
			0, 0, 0, 0, dest, GL_TEXTURE_2D, 
			0, 0, 0, 0, 1024, 512, 1);
	}

	void FrameBuffer::UpdatePartial(u32 blit_tex, u32 xoff, u32 yoff, u32 w, u32 h) {
		glCopyImageSubData(blit_tex, GL_TEXTURE_2D, 0, xoff, yoff, 0,
			m_fbo_tex, GL_TEXTURE_2D, 0, xoff, yoff, 0,
			w, h, 1);
	}

	FrameBuffer::~FrameBuffer() {
		glDeleteTextures(1, &m_fbo_tex);
		glDeleteFramebuffers(1, &m_fbo);
	}
}