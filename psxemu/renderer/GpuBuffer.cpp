#include "GpuBuffer.hpp"
#include "GLContext.hpp"
#include <common/Errors.hpp>

#include <GL/glew.h>

namespace psx::video {
	static GLbitfield GetOpenGLAccessFlagsFromEnum(BufferMapping _prots) {
		GLbitfield flags{};

		// DYNAMIC = 1,
		// READ = 2,
		// WRITE = 4,
		// PERSISTENT = 8,
		// COHERENT = 16

		if (to_bool(_prots & BufferMapping::DYNAMIC)) {
			flags |= GL_DYNAMIC_STORAGE_BIT;
		}

		if (to_bool(_prots & BufferMapping::READ)) {
			flags |= GL_MAP_READ_BIT;
		}

		if (to_bool(_prots & BufferMapping::WRITE)) {
			flags |= GL_MAP_WRITE_BIT;
		}

		if (to_bool(_prots & BufferMapping::PERSISTENT)) {
			flags |= GL_MAP_PERSISTENT_BIT;
		}

		if (to_bool(_prots & BufferMapping::COHERENT)) {
			flags |= GL_MAP_COHERENT_BIT;
		}

		return flags;
	}

	static GLenum GetOpenGLBufferUsageFromEnum(BufferUsage _usage) {
		switch (_usage)
		{
		case BufferUsage::STREAM_DRAW:
			return GL_STREAM_DRAW;
		case BufferUsage::STREAM_READ:
			return GL_STREAM_READ;
		case BufferUsage::STREAM_COPY:
			return GL_STREAM_COPY;
		case BufferUsage::STATIC_DRAW:
			return GL_STATIC_DRAW;
		case BufferUsage::STATIC_READ:
			return GL_STATIC_READ;
		case BufferUsage::STATIC_COPY:
			return GL_STATIC_COPY;
		case BufferUsage::DYNAMIC_DRAW:
			return GL_DYNAMIC_DRAW;
		case BufferUsage::DYNAMIC_READ:
			return GL_DYNAMIC_READ;
		case BufferUsage::DYNAMIC_COPY:
			return GL_DYNAMIC_COPY;
		default:
			error::Unreachable();
			break;
		}

		error::Unreachable();
		return GLenum(-1);
	}

	GpuBuffer::GpuBuffer() :
		m_buf_prots{},
		m_buf_id{},
		m_mapped_ptr{},
		m_is_mapped{},
		m_curr_prots{},
		m_curr_size{},
		m_map_offset{},
		m_map_size{}
	{
	}

	GpuBuffer::GpuBuffer(GpuBuffer&& _prev) noexcept :
		m_buf_prots{ _prev.m_buf_prots },
		m_buf_id{ _prev.m_buf_id },
		m_mapped_ptr{ _prev.m_mapped_ptr },
		m_is_mapped{ _prev.m_is_mapped },
		m_curr_prots{ _prev.m_curr_prots },
		m_curr_size{ _prev.m_curr_size },
		m_map_offset{ _prev.m_map_offset },
		m_map_size{ _prev.m_map_size } {
		_prev.m_buf_id = 0;
		_prev.m_mapped_ptr = nullptr;
		_prev.m_is_mapped = false;
		_prev.m_curr_size = 0;
		_prev.m_map_offset = 0;
		_prev.m_map_size = 0;
	}

	GpuBuffer::~GpuBuffer() noexcept {
		if (0 == m_buf_id) {
			return;
		}
		if (m_is_mapped) {
			UnmapBuffer();
		}
		glDeleteBuffers(1, &m_buf_id);
		m_buf_id = 0;
		m_curr_size = 0;
	}

	GpuBuffer& GpuBuffer::operator=(GpuBuffer&& _other) noexcept {
		/// Explicitly call the destructor to
		/// free the previous resources (if any)
		this->~GpuBuffer();

		m_buf_prots = _other.m_buf_prots;
		m_buf_id = _other.m_buf_id;
		m_mapped_ptr = _other.m_mapped_ptr;
		m_is_mapped = _other.m_is_mapped;
		m_curr_prots = _other.m_curr_prots;
		m_curr_size = _other.m_curr_size;
		m_map_offset = _other.m_map_offset;
		m_map_size = _other.m_map_size;

		_other.m_buf_id = 0;
		_other.m_mapped_ptr = nullptr;
		_other.m_is_mapped = false;
		_other.m_curr_size = 0;
		_other.m_map_offset = 0;
		_other.m_map_size = 0;
		return *this;
	}

	std::expected<GpuBuffer, GLError> GpuBuffer::CreateBuffer(size_t _size_bytes, BufferMapping _prots) {
		auto buf = GpuBuffer();
		buf.m_buf_prots = _prots;

		if (auto maybe_err = buf.ResizeBuffer(_size_bytes, _prots)) {
			return std::unexpected{ maybe_err.value() };
		}

		return buf;
	}

	void GpuBuffer::BindBuffer(uint32_t _target) const {
		auto ctx = GetCurrentGLContext();
		ctx->BindBuffer(_target, m_buf_id);
	}

	std::optional<GLError> GpuBuffer::ResizeBuffer(size_t _size_bytes, BufferMapping _prots) {
		if (_size_bytes == m_curr_size) {
			return std::nullopt;
		}

		if (0 != m_buf_id && m_is_mapped) {
			return GLError::RESIZE_FAILED_BUFFER_MAPPED;
		}

		uint32_t new_buf_id{};
		glCreateBuffers(1, &new_buf_id);

		if (0 == new_buf_id) {
			return ConvertGLErrorToEnum(glGetError());
		}

		auto prots = GetOpenGLAccessFlagsFromEnum(m_buf_prots);
		glNamedBufferStorage(new_buf_id, _size_bytes, nullptr, prots);

		auto maybe_error = ConvertGLErrorToEnum(glGetError());
		if (maybe_error != GLError::OK) {
			glDeleteBuffers(1, &new_buf_id);
			return maybe_error;
		}

		if (0 != m_buf_id) {
			// A previous buffer exists, copy data
			auto copy_size = std::min(_size_bytes, m_curr_size);
			glCopyNamedBufferSubData(m_buf_id, new_buf_id, 0, 0, copy_size);

			auto maybe_error = ConvertGLErrorToEnum(glGetError());
			if (maybe_error != GLError::OK) {
				glDeleteBuffers(1, &new_buf_id);
				return maybe_error;
			}

			// Delete the old buffer storage
			glDeleteBuffers(1, &m_buf_id);
		}

		m_buf_id = new_buf_id;
		m_curr_size = _size_bytes;

		return std::nullopt;
	}

	std::optional<GLError> GpuBuffer::WriteBufferSubData(const void* _data, size_t _offset_bytes, size_t _len_bytes) const {
		if (m_is_mapped && !to_bool(m_curr_prots & BufferMapping::PERSISTENT)) {
			return GLError::WRITE_FAILED_BUFFER_MAPPED;
		}

		if (_offset_bytes + _len_bytes > m_curr_size) {
			return GLError::WRITE_FAILED_BUFFER_OVERFLOW;
		}

		glNamedBufferSubData(m_buf_id, _offset_bytes, _len_bytes, _data);

		auto maybe_error = ConvertGLErrorToEnum(glGetError());
		if (maybe_error != GLError::OK) {
			return maybe_error;
		}
		return std::nullopt;
	}

	std::optional<GLError> GpuBuffer::ReadBufferSubData(void* _data, size_t _offset_bytes, size_t _len_bytes) const {
		if (m_is_mapped && !to_bool(m_curr_prots & BufferMapping::PERSISTENT)) {
			return GLError::READ_FAILED_BUFFER_MAPPED;
		}

		if (_offset_bytes + _len_bytes > m_curr_size) {
			return GLError::READ_FAILED_BUFFER_OVERFLOW;
		}

		glGetNamedBufferSubData(m_buf_id, _offset_bytes, _len_bytes, _data);

		auto maybe_error = ConvertGLErrorToEnum(glGetError());
		if (maybe_error != GLError::OK) {
			return maybe_error;
		}
		return std::nullopt;
	}

	std::optional<GLError> GpuBuffer::MapBuffer(size_t _offset_bytes, size_t _len_bytes, BufferMapping _prots) {
		if (m_is_mapped) {
			if (m_map_offset == _offset_bytes && m_map_size == _len_bytes &&
				m_curr_prots == _prots) {
				return std::nullopt;
			}
			UnmapBuffer();
		}

		if (_offset_bytes + _len_bytes > m_curr_size) {
			return GLError::MAP_FAILED_BUFFER_OVERFLOW;
		}

		auto flags = GetOpenGLAccessFlagsFromEnum(_prots);
		if (0 == flags) {
			return GLError::MAP_FAILED_INVALID_PROTS;
		}

		auto buf_ptr = glMapNamedBufferRange(m_buf_id, (GLintptr)_offset_bytes, (GLsizeiptr)_len_bytes, flags);
		auto maybe_error = ConvertGLErrorToEnum(glGetError());
		if (maybe_error != GLError::OK) {
			return maybe_error;
		}

		m_is_mapped = true;
		m_map_offset = _offset_bytes;
		m_map_size = _len_bytes;
		m_mapped_ptr = buf_ptr;
		m_curr_prots = _prots;
		return std::nullopt;
	}

	void GpuBuffer::UnmapBuffer() {
		if (!m_is_mapped) {
			return;
		}
		// Assume unmapping succeeds
		glUnmapNamedBuffer(m_buf_id);
		m_is_mapped = false;
		m_map_offset = 0;
		m_map_size = 0;
		m_mapped_ptr = nullptr;
	}

	void GpuBuffer::FlushBuffer() const {
		if (!m_is_mapped) {
			return;
		}
		if (to_bool(m_curr_prots & BufferMapping::COHERENT)) {
			return; // Mapping is already flushed by the driver
		}
		glFlushMappedNamedBufferRange(m_buf_id, m_map_offset, m_map_size);
	}

	std::optional<GLError> GpuBuffer::CopyFromBufferRange(GpuBuffer const& _other,
		size_t _src_offset_bytes,
		size_t _dst_offset_bytes,
		size_t _len_bytes) const {
		if (m_is_mapped || _other.m_is_mapped) {
			auto should_return{ false };

			if (!m_is_mapped || !_other.m_is_mapped) {
				should_return = (m_is_mapped && !to_bool(m_curr_prots & BufferMapping::PERSISTENT)) ||
					(_other.m_is_mapped && !to_bool(_other.m_curr_prots & BufferMapping::PERSISTENT));
			}
			else {
				should_return = (!to_bool(m_curr_prots & BufferMapping::PERSISTENT) ||
					!to_bool(_other.m_curr_prots & BufferMapping::PERSISTENT));
			}

			if (should_return) {
				return GLError::COPY_FAILED_BUFFER_MAPPED;
			}
		}

		if (_src_offset_bytes + _len_bytes > _other.m_curr_size) {
			return GLError::READ_FAILED_BUFFER_OVERFLOW;
		}

		if (_dst_offset_bytes + _len_bytes > m_curr_size) {
			return GLError::WRITE_FAILED_BUFFER_OVERFLOW;
		}

		glCopyNamedBufferSubData(_other.m_buf_id, m_buf_id, _src_offset_bytes, _dst_offset_bytes, _len_bytes);

		auto maybe_error = ConvertGLErrorToEnum(glGetError());
		if (maybe_error != GLError::OK) {
			return maybe_error;
		}

		return std::nullopt;
	}

	std::expected<GpuBuffer, GLError> GpuBuffer::Clone() const {
		if (m_is_mapped && !to_bool(m_curr_prots & BufferMapping::PERSISTENT)) {
			return std::unexpected{ GLError::COPY_FAILED_BUFFER_MAPPED };
		}

		auto maybe_new_buf = CreateBuffer(GetBufferSize(), GetBufferProts());
		if (!maybe_new_buf.has_value()) {
			return std::unexpected{ maybe_new_buf.error() };
		}
		auto new_buf = std::move(maybe_new_buf.value());

		auto maybe_err = new_buf.CopyFromBufferRange(*this, 0, 0, GetBufferSize());
		if (maybe_err.has_value()) {
			return std::unexpected{ maybe_err.value() };
		}

		return new_buf;
	}

	void GpuBuffer::SetLabel(std::string const& _label) const {
		glObjectLabel(GL_BUFFER, m_buf_id, (GLsizei)_label.size(), _label.c_str());
	}

	void GpuBuffer::SetLabel(std::string_view _label) const {
		glObjectLabel(GL_BUFFER, m_buf_id, (GLsizei)_label.size(), _label.data());
	}
}