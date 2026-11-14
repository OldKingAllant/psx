#include "VertexBuffer.hpp"
#include "GLContext.hpp"

#include <GL/glew.h>

#include <common/Errors.hpp>

#include <typeinfo>
#include <string>

namespace psx::video {
	namespace detail {
		std::expected<uint32_t, GLError> CreateVertexArrayObject() {
			uint32_t vao{};
			glGenVertexArrays(1, &vao);

			if (0 == vao) {
				auto maybe_err = ConvertGLErrorToEnum(glGetError());
				return std::unexpected{ maybe_err };
			}

			return vao;
		}

		void DestroyVertexArrayObject(uint32_t _vao) {
			glDeleteVertexArrays(1, &_vao);
		}

		void SetCurrentArrayBuffer(uint32_t _buf) {
			auto ctx = GetCurrentGLContext();
			ctx->BindBuffer(GL_ARRAY_BUFFER, _buf);
		}

		void SetVertexBufferLabel(uint32_t _vao, std::string const& _label) {
			glObjectLabel(GL_VERTEX_ARRAY, _vao, _label.size(), _label.c_str());
		}

		/// <summary>
		/// Convert our vertex attribute type representation 
		/// to OpenGL
		/// </summary>
		/// <param name="_attr">Attribute type</param>
		/// <returns>OpenGL type</returns>
		static uint32_t VertexAttributeToOpenGL(VertexAttributeType _attr) {
			switch (_attr)
			{
			case VertexAttributeType::BYTE:
				return GL_BYTE;
			case VertexAttributeType::UNSIGNED_BYTE:
				return GL_UNSIGNED_BYTE;
			case VertexAttributeType::SHORT:
				return GL_SHORT;
			case VertexAttributeType::UNSIGNED_SHORT:
				return GL_UNSIGNED_SHORT;
			case VertexAttributeType::INT:
				return GL_INT;
			case VertexAttributeType::UNSIGNED_INT:
				return GL_UNSIGNED_INT;
			case VertexAttributeType::FLOAT:
				return GL_FLOAT;
			case VertexAttributeType::DOUBLE:
				return GL_DOUBLE;
			case VertexAttributeType::VEC2:
			case VertexAttributeType::VEC3:
			case VertexAttributeType::VEC4:
				return GL_FLOAT;
			case VertexAttributeType::UVEC2:
			case VertexAttributeType::UVEC3:
			case VertexAttributeType::UVEC4:
				return GL_UNSIGNED_INT;
			case VertexAttributeType::IVEC2:
			case VertexAttributeType::IVEC3:
			case VertexAttributeType::IVEC4:
				return GL_INT;
			default:
				error::Unreachable();
				break;
			}

			error::Unreachable();
			return GL_FLOAT;
		}

		uint32_t SetVertexAttributes(std::vector<VertexAttribute> const& _attributes,

			uint32_t _instance_step_count,
			uint32_t _vert_size_bytes,
			uint32_t _base_index) {
			uint32_t index{ _base_index };

			for (auto const& attribute : _attributes) {
				auto type = VertexAttributeToOpenGL(attribute.type);
				auto offset = (const void*)attribute.offset;
				auto size = VERTEX_ATTRIB_NUM_ELEMENTS[(uint32_t)attribute.type];

				switch (type)
				{
				case GL_FLOAT:
					glVertexAttribPointer(index, size, type, GL_FALSE,
						_vert_size_bytes, offset);
					break;
				case GL_DOUBLE:
					glVertexAttribLPointer(index, size, type,
						_vert_size_bytes, offset);
					break;
				default:
					glVertexAttribIPointer(index, size, type,
						_vert_size_bytes, offset);
					break;
				}

				if (0 != _instance_step_count) {
					glVertexAttribDivisor(index, _instance_step_count);
				}

				glEnableVertexAttribArray(index);
				index++;
			}

			return index;
		}
	}

	VertexBuffer::VertexBuffer(VertexBuffer&& _prev) noexcept :
		m_buf_sizes{ _prev.m_buf_sizes },
		m_vao{ _prev.m_vao },
		m_buffers{ std::move(_prev.m_buffers) },
		m_attrib_types{ std::move(_prev.m_attrib_types) },
		m_attrib_sizes{ std::move(_prev.m_attrib_sizes) },
		m_curr_attrib_index{ _prev.m_curr_attrib_index },
		m_attrib_divisors{ std::move(_prev.m_attrib_divisors) },
		m_buf_attribs{ std::move(_prev.m_buf_attribs) } {
		_prev.m_vao = 0;
		_prev.m_buffers.clear();
		_prev.m_buf_sizes = {};
		_prev.m_attrib_types = {};
		_prev.m_curr_attrib_index = 0;
		_prev.m_attrib_sizes = {};
		_prev.m_attrib_divisors = {};
		_prev.m_buf_attribs = {};
	}

	VertexBuffer& VertexBuffer::operator=(VertexBuffer&& _other) noexcept {
		this->~VertexBuffer();
		m_buffers.clear();

		m_vao = _other.m_vao;
		m_buffers = std::move(_other.m_buffers);
		m_buf_sizes = std::move(_other.m_buf_sizes);
		m_attrib_types = std::move(_other.m_attrib_types);
		m_curr_attrib_index = _other.m_curr_attrib_index;
		m_attrib_sizes = std::move(_other.m_attrib_sizes);
		m_attrib_divisors = std::move(_other.m_attrib_divisors);
		m_buf_attribs = std::move(_other.m_buf_attribs);

		_other.m_vao = 0;
		_other.m_buffers.clear();
		_other.m_buf_sizes = {};
		_other.m_attrib_types = {};
		_other.m_curr_attrib_index = 0;
		_other.m_attrib_sizes = {};
		_other.m_attrib_divisors = {};
		_other.m_buf_attribs = {};

		return *this;
	}

	VertexBuffer::VertexBuffer() :
		m_buf_sizes{},
		m_vao{},
		m_buffers{},
		m_attrib_types{},
		m_attrib_sizes{},
		m_curr_attrib_index{},
		m_attrib_divisors{},
		m_buf_attribs{} {
	}

	std::expected<VertexBuffer, GLError> VertexBuffer::CreateVertexBuffer() {
		auto maybe_vao = detail::CreateVertexArrayObject();
		if (!maybe_vao.has_value()) {
			return std::unexpected{ maybe_vao.error() };
		}

		auto vao = maybe_vao.value();
		auto vert_buf = VertexBuffer();
		vert_buf.m_vao = vao;
		return vert_buf;
	}

	std::optional<GLError> VertexBuffer::AddBufferImpl(
		std::type_info const* _attrib_type,
		size_t _attrib_size,
		std::vector<VertexAttribute> _attribs,
		uint32_t _attrib_divisor,
		size_t _init_size) {
		// First create underlying buffer
		auto buf_temp = GpuBuffer::CreateBuffer(_init_size * _attrib_size,
			BufferMapping::WRITE |
			BufferMapping::PERSISTENT |
			BufferMapping::COHERENT |
			BufferMapping::DYNAMIC
		);
		if (!buf_temp.has_value()) {
			return buf_temp.error();
		}
		// Assign to temporary, do not push to m_buffers
		auto buf = std::move(buf_temp.value());

		// Attempt to host map the new buffer
		// If mapping fails, the vertex buffer
		// state has not been changed in any
		// way, all class invariants are respected
		auto maybe_err = buf.MapBuffer(0, buf.GetBufferSize(),
			BufferMapping::WRITE | BufferMapping::PERSISTENT | BufferMapping::COHERENT);
		if (maybe_err.has_value()) {
			return maybe_err.value();
		}

		auto ctx = GetCurrentGLContext();
		Bind();
		detail::SetCurrentArrayBuffer(buf.GetBufferId());

		m_curr_attrib_index = detail::SetVertexAttributes(_attribs, _attrib_divisor, (uint32_t)_attrib_size,
			m_curr_attrib_index);

		detail::SetCurrentArrayBuffer(0);
		ctx->BindVao(0);

		// We can now modify the vertex buffer state
		m_buffers.push_back(std::move(buf));
		m_buf_sizes.push_back(0);
		m_attrib_types.push_back(_attrib_type);
		m_attrib_sizes.push_back(_attrib_size);
		m_attrib_divisors.push_back(_attrib_divisor);
		m_buf_attribs.push_back(_attribs);

		return std::nullopt;
	}

	std::optional<GLError> VertexBuffer::PushVertexDataTypedImpl(
		size_t _buf_index,
		std::type_info const* _type,
		const void* _data,
		size_t _size_bytes
	) {
		if (_buf_index > m_buffers.size()) {
			return GLError::PUSH_FAILED_INVALID_BUFFER;
		}
		auto stored_type = m_attrib_types[_buf_index];
		auto stored_type_size = m_attrib_sizes[_buf_index];

		if (*stored_type != *_type) {
			return GLError::PUSH_FAILED_TYPE_MISMATCH;
		}

		if (0 != _size_bytes % stored_type_size) {
			return GLError::PUSH_FAILED_SIZE_MISMATCH;
		}

		return PushVertexDataUnsafe(_buf_index, _data, _size_bytes);
	}

	std::optional<GLError> VertexBuffer::PushVertexDataUnsafe(size_t _buf_index, const void* _data, size_t _size_bytes) {
		auto& buf = m_buffers[_buf_index];
		auto type_size = m_attrib_sizes[_buf_index];
		auto curr_byte_offset = m_buf_sizes[_buf_index] * type_size;
		auto last_byte_offset = curr_byte_offset + _size_bytes;
		if (buf.GetBufferSize() < last_byte_offset) {
			return GLError::PUSH_FAILED_BUFFER_OVERFLOW;
		}
		auto maybe_ptr = buf.GetHostBuffer();
		auto buf_ptr = maybe_ptr.value();
		std::copy_n((const uint8_t*)_data, _size_bytes, (uint8_t*)(buf_ptr)+curr_byte_offset);
		m_buf_sizes[_buf_index] += _size_bytes / type_size;
		return std::nullopt;
	}

	std::expected<VertexBuffer, GLError> VertexBuffer::CreateResizedFrom(VertexBuffer const& _other, std::vector<size_t> const& _new_sizes) {
		if (_other.m_buffers.size() != _new_sizes.size()) {
			return std::unexpected{ GLError::RESIZE_FAILED_MISSING_BUFFERS };
		}

		auto maybe_vertex_buf = CreateVertexBuffer();
		if (!maybe_vertex_buf.has_value()) {
			return std::unexpected{ maybe_vertex_buf.error() };
		}

		auto new_vbuf = std::move(maybe_vertex_buf.value());

		for (size_t curr_buf = 0; curr_buf < _new_sizes.size(); curr_buf++) {
			auto maybe_err = new_vbuf.AddBufferImpl(
				_other.m_attrib_types[curr_buf],
				_other.m_attrib_sizes[curr_buf],
				_other.m_buf_attribs[curr_buf],
				_other.m_attrib_divisors[curr_buf],
				_new_sizes[curr_buf]);
			if (maybe_err.has_value()) {
				return std::unexpected{ maybe_err.value() };
			}
			auto curr_buf_size = _other.m_buf_sizes[curr_buf] * _other.m_attrib_sizes[curr_buf];
			if (curr_buf_size == 0) {
				continue;
			}
			auto copy_size = std::min(curr_buf_size, _new_sizes[curr_buf] * _other.m_attrib_sizes[curr_buf]);
			maybe_err = new_vbuf.m_buffers.back().CopyFromBufferRange(_other.m_buffers[curr_buf], 0, 0, copy_size);
			if (maybe_err.has_value()) {
				return std::unexpected{ maybe_err.value() };
			}
			new_vbuf.m_buf_sizes[curr_buf] = _other.m_buf_sizes[curr_buf];
		}

		return new_vbuf;
	}

	std::expected<VertexBuffer, GLError> VertexBuffer::Clone() const {
		std::vector<size_t> sizes{};
		for (size_t buf_index = 0; buf_index < m_buffers.size(); buf_index++) {
			sizes.push_back(GetBufferCapacity(buf_index).value());
		}
		return CreateResizedFrom(*this, sizes);
	}

	void VertexBuffer::Bind() const {
		auto ctx = GetCurrentGLContext();
		ctx->BindVao(m_vao);
	}

	void VertexBuffer::Unbind() const {
		auto ctx = GetCurrentGLContext();
		ctx->BindVao(0);
	}

	VertexBuffer::~VertexBuffer() {
		if (0 == m_vao) {
			return;
		}
		detail::DestroyVertexArrayObject(m_vao);
		m_vao = 0;
	}

	void VertexBuffer::SetLabel(std::string const& _label) const {
		glObjectLabel(GL_VERTEX_ARRAY, m_vao, (GLsizei)_label.size(), _label.c_str());
		for (size_t curr_buf = 0; auto const& buf : m_buffers) {
			buf.SetLabel(_label + "_buf_" + std::to_string(curr_buf));
			curr_buf++;
		}
	}

	void VertexBuffer::SetLabel(std::string_view _label) const {
		glObjectLabel(GL_VERTEX_ARRAY, m_vao, (GLsizei)_label.size(), _label.data());
		for (size_t curr_buf = 0; auto const& buf : m_buffers) {
			buf.SetLabel(std::string{ _label } + "_buf_" + std::to_string(curr_buf));
			curr_buf++;
		}
	}
}