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

		CreateBuffer();
		MapBuffer();

		auto errors = GlGetErrors();

		if (!errors.empty()) {
			for (auto const& err : errors)
				fmt::println("[OPENGL] Error : {}",
					(const char*)glewGetErrorString(err));
			throw std::runtime_error("OpenGL errors!");
		}

		for (uint32_t index = 0; index < VRAM_SIZE_BYTES; index++)
			m_buffer_ptr[index] = 0x00;

		Upload();
	}

	u8* Vram::Get() const {
		return m_buffer_ptr;
	}

	Vram::~Vram() {
		if (!m_buffer_ptr)
			return;

		UnmapBuffer();
		DestroyBuffer();
	}

	void Vram::Upload() {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_buffer_id);
		glBindTexture(GL_TEXTURE_2D, m_texture_id);

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 512,
			GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, nullptr);

		glBindTexture(GL_TEXTURE_2D, 0);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}

	void Vram::CreateBuffer() {
		glGenBuffers(1, reinterpret_cast<GLuint*>(&m_buffer_id));
		glGenTextures(1, reinterpret_cast<GLuint*>(&m_texture_id));

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_buffer_id);
		glBufferStorage(GL_PIXEL_UNPACK_BUFFER, VRAM_SIZE_BYTES, nullptr,
			GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | 
			GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);

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

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
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
		m_buffer_ptr = nullptr;
	}

	void Vram::DestroyBuffer() {
		glDeleteBuffers(1, reinterpret_cast<GLuint*>(&m_buffer_id));
		glDeleteTextures(1, reinterpret_cast<GLuint*>(&m_texture_id));
		m_buffer_ptr = nullptr;
	}
}