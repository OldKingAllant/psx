#include "FrameBuffer.hpp"
#include "GlLoad.hpp"

#include <GL/glew.h>

#include <fmt/format.h>
#include <stdexcept>
#include <string_view>
#include <common/Defs.hpp>

namespace psx::video {
	FrameBuffer::FrameBuffer(u32 vram_x_size, u32 vram_y_size) :
		m_x_size{vram_x_size}, m_y_size{vram_y_size},
		m_base_label{}, m_fbo{}, m_fbo_tex{},
		m_mask_texture{}, m_resolution_multiplier{ 1 },
		m_upscaled_tex{}, m_upscaled_fbo{} {
		glGenFramebuffers(1, &m_fbo);
		glGenTextures(1, &m_fbo_tex);

		///////////////////////////////////////////////////////
		glBindTexture(GL_TEXTURE_2D, m_fbo_tex);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, m_x_size, m_y_size,
			0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, nullptr);

		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_x_size);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
		glPixelStorei(GL_PACK_ROW_LENGTH, m_x_size);
		glPixelStorei(GL_PACK_ALIGNMENT, 2);

		glBindTexture(GL_TEXTURE_2D, 0);
		///////////////////////////////////////////////////////

		glGenTextures(1, &m_mask_texture);
		glBindTexture(GL_TEXTURE_2D, m_mask_texture);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, m_x_size, m_y_size);

		glBindTexture(GL_TEXTURE_2D, 0);

		///////////////////////////////////////////////////////

		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, m_fbo_tex, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			fmt::println("[RENDERER] Framebuffer creation failed!");
			throw std::runtime_error("Framebuffer error");
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		////////////////////////////////////////////////////
	}

	void FrameBuffer::RebuildUpscaledFbo(u32 resolution_multiplier) {
		if (resolution_multiplier == 0) {
			fmt::print("[RENDERER] CANNOT CREATE FRAMEBUFFER WITH ZERO RESOLUTION");
			throw std::runtime_error("Framebuffer error");
		}

		if (m_resolution_multiplier == resolution_multiplier) {
			return;
		}

		if (resolution_multiplier != 1) {
			//Non-native
			u32 upscaled_tex{}, upscaled_fbo{};

			glGenFramebuffers(1, &upscaled_fbo);
			glGenTextures(1, &upscaled_tex);

			glBindTexture(GL_TEXTURE_2D, upscaled_tex);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1,
				m_x_size * resolution_multiplier,
				m_y_size * resolution_multiplier,
				0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, nullptr);

			glBindTexture(GL_TEXTURE_2D, 0);

			glBindFramebuffer(GL_FRAMEBUFFER, upscaled_fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, upscaled_tex, 0);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				fmt::println("[RENDERER] Upscaled framebuffer creation failed!");
				throw std::runtime_error("Framebuffer error");
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			if (m_resolution_multiplier > 1) {
				glBlitNamedFramebuffer(m_upscaled_fbo.value(), upscaled_fbo,
					0, 0, m_x_size * m_resolution_multiplier, m_y_size * m_resolution_multiplier,
					0, 0, m_x_size * resolution_multiplier, m_y_size * resolution_multiplier,
					GL_COLOR_BUFFER_BIT, GL_NEAREST);
			}
			else {
				glBlitNamedFramebuffer(m_fbo, upscaled_fbo,
					0, 0, m_x_size, m_y_size,
					0, 0, m_x_size * resolution_multiplier, m_y_size * resolution_multiplier,
					GL_COLOR_BUFFER_BIT, GL_NEAREST);
			}

			if (m_resolution_multiplier > 1) {
				glDeleteFramebuffers(1, &m_upscaled_fbo.value());
				glDeleteTextures(1, &m_upscaled_tex.value());
			}

			m_upscaled_tex = upscaled_tex;
			m_upscaled_fbo = upscaled_fbo;
			m_resolution_multiplier = resolution_multiplier;

			SetUpscaledFboLabel();
		}
		else {
			//Return to native
			glBlitNamedFramebuffer(m_upscaled_fbo.value(), m_fbo,
				0, 0, m_x_size * m_resolution_multiplier, m_y_size * m_resolution_multiplier,
				0, 0, m_x_size, m_y_size,
				GL_COLOR_BUFFER_BIT, GL_NEAREST);

			glDeleteFramebuffers(1, &m_upscaled_fbo.value());
			glDeleteTextures(1, &m_upscaled_tex.value());

			m_upscaled_tex = std::nullopt;
			m_upscaled_fbo = std::nullopt;
			m_resolution_multiplier = 1;
		}
	}

	void FrameBuffer::Bind() {
		if (m_resolution_multiplier > 1) {
			glBindFramebuffer(GL_FRAMEBUFFER, m_upscaled_fbo.value());
		}
		else {
			glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
		}
	}

	void FrameBuffer::Unbind() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void FrameBuffer::UpdateInternalTexture(u32 blit_tex) {
		glCopyImageSubData(blit_tex, GL_TEXTURE_2D,
			0, 0, 0, 0, m_fbo_tex, GL_TEXTURE_2D,
			0, 0, 0, 0, m_x_size, m_y_size, 1);

		if (m_resolution_multiplier > 1) {
			glBlitNamedFramebuffer(m_fbo, m_upscaled_fbo.value(),
				0, 0, m_x_size, m_y_size,
				0, 0, m_x_size * m_resolution_multiplier, m_y_size * m_resolution_multiplier,
				GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
	}

	void FrameBuffer::CopyToTexture(u32 dest) {
		if (m_resolution_multiplier > 1) {
			glBlitNamedFramebuffer(m_upscaled_fbo.value(), m_fbo,
				0, 0, m_x_size * m_resolution_multiplier, m_y_size * m_resolution_multiplier,
				0, 0, m_x_size, m_y_size,
				GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}

		glCopyImageSubData(m_fbo_tex, GL_TEXTURE_2D, 
			0, 0, 0, 0, dest, GL_TEXTURE_2D, 
			0, 0, 0, 0, m_x_size, m_y_size, 1);
	}

	void FrameBuffer::UpdatePartial(u32 blit_tex, u32 xoff, u32 yoff, u32 w, u32 h) {
		glCopyImageSubData(blit_tex, GL_TEXTURE_2D, 0, xoff, yoff, 0,
			m_fbo_tex, GL_TEXTURE_2D, 0, xoff, yoff, 0,
			w, h, 1);

		if (m_resolution_multiplier > 1) {
			glBlitNamedFramebuffer(m_fbo, m_upscaled_fbo.value(), 
				xoff, yoff, xoff + w, yoff + h, 
				xoff * m_resolution_multiplier, yoff * m_resolution_multiplier, 
				(xoff + w) * m_resolution_multiplier, (yoff + h) * m_resolution_multiplier, 
				GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
	}

	FrameBuffer::~FrameBuffer() {
		glDeleteTextures(1, &m_fbo_tex);
		glDeleteTextures(1, &m_mask_texture);
		glDeleteFramebuffers(1, &m_fbo);
		if (m_resolution_multiplier > 1) {
			glDeleteTextures(1, &m_upscaled_tex.value());
			glDeleteFramebuffers(1, &m_upscaled_fbo.value());
		}
	}

	void FrameBuffer::DownloadSubImage(u8* dest_buf, u32 xoff, u32 yoff, u32 w, u32 h) {
		if (m_resolution_multiplier > 1) {
			glBlitNamedFramebuffer(m_upscaled_fbo.value(), m_fbo,
				xoff * m_resolution_multiplier, yoff * m_resolution_multiplier,
				(xoff + w) * m_resolution_multiplier, (yoff + h) * m_resolution_multiplier,
				xoff, yoff, xoff + w, yoff + h,
				GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		u32 pointer_offset = ((yoff * m_x_size) + xoff) * 2;
		glGetTextureSubImage(m_fbo_tex, 0, xoff, yoff, 0, w, h, 1,
			GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT,
			(u64)m_x_size * m_y_size * 2, dest_buf + pointer_offset);
	}

	void FrameBuffer::SetLabel(std::string_view label) {
		m_base_label = label;

		glObjectLabel(GL_FRAMEBUFFER, m_fbo, (GLsizei)label.size(),
			label.data());
		std::string tex_label{ std::string(label) + "_texture" };
		std::string depth_label{ std::string(label) + "_mask_texture" };
		glObjectLabel(GL_TEXTURE, m_fbo_tex, (GLsizei)tex_label.size(),
			tex_label.data());
		glObjectLabel(GL_TEXTURE, m_mask_texture, (GLsizei)depth_label.size(),
			depth_label.data());

		SetUpscaledFboLabel();
	}

	void FrameBuffer::SetUpscaledFboLabel() {
		if (m_resolution_multiplier > 1 && !m_base_label.empty()) {
			std::string up_fbo{ m_base_label + "_upscaled_fb" };
			std::string up_tex{ m_base_label + "_upscaled_texture" };
			glObjectLabel(GL_FRAMEBUFFER, m_upscaled_fbo.value(), (GLsizei)up_fbo.size(),
				up_fbo.data());
			glObjectLabel(GL_TEXTURE, m_upscaled_tex.value(), (GLsizei)up_tex.size(),
				up_tex.data());
		}
	}
}