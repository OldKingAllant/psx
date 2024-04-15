#include "Vram.hpp"
#include "GlLoad.hpp"

#include <GL/glew.h>

#include <stdexcept>

#include <fmt/format.h>

namespace psx::video {
	Vram::Vram() :
		m_texture_id{}, m_buffer_id{},
		m_buffer_ptr{ nullptr } {
		if (!GlIsInit())
			throw std::runtime_error("OpeGL context missing");

		m_buffer_ptr = new u8[VRAM_SIZE_BYTES];

		std::memset(m_buffer_ptr, 0x00, VRAM_SIZE_BYTES);

		CreateBuffer();
		Upload();

		auto errors = GlGetErrors();

		if (!errors.empty()) {
			for (auto const& err : errors)
				fmt::println("[OPENGL] Error : {}",
					(const char*)glewGetErrorString(err));
			throw std::runtime_error("OpenGL errors!");
		}
	}

	u8* Vram::Get() const {
		return m_buffer_ptr;
	}

	Vram::~Vram() {
		if (!m_buffer_ptr)
			return;

		delete[] m_buffer_ptr;
		m_buffer_ptr = nullptr;

		UnmapBuffer();
		DestroyBuffer();
	}

	void Vram::Upload() {
		glBindTexture(GL_TEXTURE_2D, m_texture_id);

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 512,
			GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, m_buffer_ptr);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void Vram::CreateBuffer() {
		glGenBuffers(1, reinterpret_cast<GLuint*>(&m_buffer_id));
		glGenTextures(1, reinterpret_cast<GLuint*>(&m_texture_id));

		glBindTexture(GL_TEXTURE_2D, m_texture_id);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		float border[] = { 1.0, 0.0, 0.0, 1.0 };

		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

		glPixelStorei(GL_UNPACK_ROW_LENGTH, 1024);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 1024, 512, 0,
			GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, nullptr);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void Vram::MapBuffer() {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_buffer_id);
		m_buffer_ptr = (u8*)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, (GLintptr)0, VRAM_SIZE_BYTES,
			GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT |
			GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}

	void Vram::UnmapBuffer() {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_buffer_id);
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}

	void Vram::DestroyBuffer() {
		glDeleteBuffers(1, reinterpret_cast<GLuint*>(&m_buffer_id));
		glDeleteTextures(1, reinterpret_cast<GLuint*>(&m_texture_id));
	}

	void Vram::Download() {
		glBindTexture(GL_TEXTURE_2D, m_texture_id);

		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, 
			GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, m_buffer_ptr);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void Vram::UploadSubImage(u32 xoff, u32 yoff, u32 w, u32 h) {
		u32 pointer_offset = ((yoff * 1024) + xoff) * 2;
		glBindTexture(GL_TEXTURE_2D, m_texture_id);
		glTexSubImage2D(GL_TEXTURE_2D, 0, xoff, yoff,
			w, h, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT,
			m_buffer_ptr + pointer_offset);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void Vram::DownloadSubImage(u32 xoff, u32 yoff, u32 w, u32 h) {
		u32 pointer_offset = ((yoff * 1024) + xoff) * 2;
		glBindTexture(GL_TEXTURE_2D, m_texture_id);
		glGetTextureSubImage(m_texture_id, 0, xoff, yoff, 0,
			w, h, 1, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT,
			VRAM_SIZE_BYTES, m_buffer_ptr + pointer_offset);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}