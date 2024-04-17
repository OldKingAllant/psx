#include "Buffer.hpp"

#include <GL/glew.h>

namespace psx::video {

	Buffer::Buffer(bool map_persistent, BufferMap prot, u32 size) :
		m_buffer_id{}, m_buffer_ptr{nullptr},
		m_mapped{ false }, m_persistent{map_persistent},
		m_curr_prot{ prot }, m_sz{} {
		CreateBuffer();
		Resize(size);
	}

	void Buffer::Resize(u32 new_sz) {
		glBindBuffer(GL_COPY_WRITE_BUFFER, m_buffer_id);
		
		u32 prots = 0;

		switch (m_curr_prot)
		{
		case psx::video::BufferMap::READ:
			prots = GL_MAP_READ_BIT;
			break;
		case psx::video::BufferMap::WRITE:
			prots = GL_MAP_WRITE_BIT;
			break;
		case psx::video::BufferMap::READWRITE:
			prots = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT;
			break;
		default:
			break;
		}

		if (m_persistent) {
			prots |= GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
		}

		glBufferStorage(GL_COPY_WRITE_BUFFER, new_sz, nullptr,
			(GLbitfield)prots);

		m_sz = new_sz;

		if (m_persistent)
			Map(0, new_sz);

		glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
	}

	void Buffer::BufferSubData(const void* data, u64 off, u32 len) {
		glBindBuffer(GL_COPY_WRITE_BUFFER, m_buffer_id);
		glBufferSubData(GL_COPY_WRITE_BUFFER, (GLintptr)off,
			(GLsizeiptr)len, data);
		glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
	}

	void* Buffer::Map(u32 start, u32 len) {
		if (m_mapped)
			return m_buffer_ptr;

		if (start >= m_sz || start + len > m_sz)
			return nullptr;

		u32 prots = 0;

		switch (m_curr_prot)
		{
		case psx::video::BufferMap::READ:
			prots = GL_MAP_READ_BIT;
			break;
		case psx::video::BufferMap::WRITE:
			prots = GL_MAP_WRITE_BIT;
			break;
		case psx::video::BufferMap::READWRITE:
			prots = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT;
			break;
		default:
			break;
		}

		if (m_persistent) {
			prots |= GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
		}

		glBindBuffer(GL_COPY_WRITE_BUFFER, m_buffer_id);

		auto ptr = glMapBufferRange(GL_COPY_WRITE_BUFFER, start, len,
			(GLbitfield)prots);

		m_buffer_ptr = ptr;
		m_mapped = true;

		glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

		return m_buffer_ptr;
	}

	void Buffer::Umap() {
		if (!m_mapped)
			return;

		glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

		glBindBuffer(GL_COPY_WRITE_BUFFER, m_buffer_id);
		glUnmapBuffer(GL_COPY_WRITE_BUFFER);
		glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

		m_mapped = false;
		m_buffer_ptr = nullptr;
	}

	void Buffer::Flush() {
		glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
	}

	Buffer::~Buffer() {
		glDeleteBuffers(1, &m_buffer_id);
	}

	void Buffer::CreateBuffer() {
		glGenBuffers(1, &m_buffer_id);
	}

	void Buffer::SetLabel(std::string_view label) {
		glObjectLabel(GL_BUFFER, m_buffer_id, (GLsizei)label.size(),
			label.data());
	}
}