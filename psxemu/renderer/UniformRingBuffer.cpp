#include "UniformRingBuffer.hpp"
#include "GLContext.hpp"

#include <GL/glew.h>

namespace psx::video {
	std::expected<UniformRingBuffer, GLError> UniformRingBuffer::CreateRingBuffer(size_t _slot_size, size_t _num_slots) {
		auto ringbuf = UniformRingBuffer{};
		auto maybe_buf = GpuBuffer::CreateBuffer(_slot_size * _num_slots,
			BufferMapping::DYNAMIC | BufferMapping::WRITE | BufferMapping::PERSISTENT);

		if (!maybe_buf.has_value()) {
			return std::unexpected{ maybe_buf.error() };
		}

		auto buf = std::move(maybe_buf.value());
		auto maybe_err = buf.MapBuffer(0, buf.GetBufferSize(),
			BufferMapping::WRITE | BufferMapping::PERSISTENT | BufferMapping::FLUSH_EXPLICIT);

		if (maybe_err.has_value()) {
			return std::unexpected{ maybe_err.value() };
		}

		ringbuf.m_buf = std::move(buf);
		ringbuf.m_slot_size = _slot_size;
		ringbuf.m_num_slots = _num_slots;

		return ringbuf;
	}

	UniformRingBuffer::UniformRingBuffer() :
		m_buf{},
		m_slot_size{},
		m_num_slots{},
		m_bound_to{std::nullopt} { }

	UniformRingBuffer::UniformRingBuffer(UniformRingBuffer&& _other) noexcept :
		m_buf{std::move(_other.m_buf)},
		m_slot_size{_other.m_slot_size},
		m_num_slots{_other.m_num_slots},
		m_bound_to{_other.m_bound_to}
	{
		_other.m_bound_to = std::nullopt;
	}

	UniformRingBuffer& UniformRingBuffer::operator=(UniformRingBuffer&& _other) noexcept {
		m_buf = std::move(_other.m_buf);
		m_slot_size = _other.m_slot_size;
		m_num_slots = _other.m_num_slots;
		m_bound_to = _other.m_bound_to;
		_other.m_bound_to = std::nullopt;
		return *this;
	}

	UniformRingBuffer::~UniformRingBuffer() {
		m_bound_to = std::nullopt;
	}

	std::optional<GLError> UniformRingBuffer::WriteSlotImpl(size_t _slot_id, const void* _data) {
		if (_slot_id > m_num_slots) {
			return GLError::INVALID_RINGBUFFER_SLOT;
		}
		auto buf_ptr = m_buf.GetHostBuffer().value();
		std::copy_n(std::bit_cast<u8*>(_data), m_slot_size, std::bit_cast<u8*>(buf_ptr) + (_slot_id * m_slot_size));
		return std::nullopt;
	}

	std::optional<GLError> UniformRingBuffer::FlushSlotWrites(size_t _slot_id) {
		if (_slot_id > m_num_slots) {
			return GLError::INVALID_RINGBUFFER_SLOT;
		}
		glFlushMappedNamedBufferRange(m_buf.GetBufferId(), _slot_id * m_slot_size, 
			m_slot_size);
		auto maybe_error = ConvertGLErrorToEnum(glGetError());
		if (GLError::OK != maybe_error) {
			return maybe_error;
		}
		return std::nullopt;
	}

	void UniformRingBuffer::BindBuffer() const {
		auto ctx = GetCurrentGLContext();
		ctx->BindBuffer(GL_UNIFORM_BUFFER, m_buf.GetBufferId());
	}

	void UniformRingBuffer::UnbindBuffer() const {
		auto ctx = GetCurrentGLContext();
		ctx->BindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void UniformRingBuffer::BindTo(uint32_t _bind_point) {
		auto ctx = GetCurrentGLContext();
		ctx->BindBufferRange(BufferRangeBinding{
			.target = GL_UNIFORM_BUFFER,
			.index = _bind_point,
			.buffer = m_buf.GetBufferId(),
			.offset = 0,
			.extent = m_buf.GetBufferSize()
		});
		m_bound_to = _bind_point;
	}

	void UniformRingBuffer::SetLabel(std::string_view _label) {
		m_buf.SetLabel(_label);
	}
}