#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>
#include <common/Errors.hpp>

#include "GpuBuffer.hpp"

#include <vector>
#include <concepts>
#include <string_view>
#include <string>
#include <typeinfo>
#include <bit>

namespace psx::video {
    /// <summary>
    /// Specifies the type of a vertex attribute,
    /// which is used to determine both the number
    /// of elements and the type of the attribute
    /// </summary>
    enum class VertexAttributeType {
        BYTE, UNSIGNED_BYTE,
        SHORT, UNSIGNED_SHORT,
        INT, UNSIGNED_INT,
        FLOAT, DOUBLE,
        VEC2, VEC3, VEC4,
        UVEC2, UVEC3, UVEC4,
        IVEC2, IVEC3, IVEC4
    };

    /// <summary>
    /// Number of elements for each attribute type
    /// </summary>
    static constexpr uint32_t VERTEX_ATTRIB_NUM_ELEMENTS[] = {
        1, 1, 1, 1, 1, 1, 1, 1,
        2, 3, 4,
        2, 3, 4,
        2, 3, 4,
        9, 16
    };

    /// <summary>
    /// A vertex attribute, given the
    /// type and offset starting from
    /// the first attribute
    /// </summary>
    struct VertexAttribute {
        VertexAttributeType type;
        std::ptrdiff_t      offset;
    };

    namespace detail {
        std::expected<uint32_t, GLError> CreateVertexArrayObject();
        void DestroyVertexArrayObject(uint32_t _vao);

        void SetCurrentArrayBuffer(uint32_t _buf);

        /// <summary>
        /// Create and enable all the vertex attributes in _attributes,
        /// using the vertex size as the stride, using _instance_step_count
        /// as the instance divisor
        /// </summary>
        /// <param name="_attributes"></param>
        /// <param name="_instance_step_count">How many instances before stepping</param>
        /// <param name="_vert_size_bytes"></param>
        /// <returns>The last attribute index</returns>
        uint32_t SetVertexAttributes(std::vector<VertexAttribute> const& _attributes,
            uint32_t _instance_step_count,
            uint32_t _vert_size_bytes,
            uint32_t _base_index);

        /// <summary>
        /// Concept to verify if struct satisfies the properties
        /// of a vertex descriptor
        /// </summary>
        template <typename Vert>
        concept Vertex = requires(Vert v) {
            { v.attributes() } -> std::same_as<std::vector<VertexAttribute>>;
        };

        void SetVertexBufferLabel(uint32_t _vao, std::string const& _label);
    }


    /// <summary>
    /// Vertex buffer class with support for multiple buffers, each
    /// with different usage hints and attribute divisors.
    /// (Initially I did this using
    /// variadic templates for each different buffer,
    /// but that is probably an overkill)
    /// </summary>
    class VertexBuffer {
    public:
        VertexBuffer(VertexBuffer&& _prev) noexcept;

        VertexBuffer& operator=(VertexBuffer&& _other) noexcept;

        /// <summary>
        /// Create shell of the vertex buffer (e.g. no 
        /// associated buffers and attributes)
        /// </summary>
        /// <returns>Vertex buffer, else error</returns>
        static std::expected<VertexBuffer, GLError> CreateVertexBuffer();

        /// <summary>
        /// Append a new buffer to the VAO
        /// </summary>
        /// <typeparam name="Vert">Type of the vertex attributye</typeparam>
        /// <param name="_attrib_divisor">Attribute divisor</param>
        /// <param name="_init_size">Initial buffer size (in number of attributes)</param>
        /// <returns></returns>
        template <detail::Vertex Vert>
        std::optional<GLError> AddBuffer(uint32_t _attrib_divisor, size_t _init_size) {
            return AddBufferImpl(&typeid(Vert), sizeof(Vert), Vert{}.attributes(), _attrib_divisor, _init_size);
        }

        /// <summary>
        /// Append a new buffer to the VAO (with manual size and type
        /// parameters, DO NOT USE DIRECTLY UNLESS NECESSARY)
        /// </summary>
        /// <param name="_attrib_type">Typeinfo of attribute struct</param>
        /// <param name="_attrib_size">Size of attribute struct</param>
        /// <param name="_attribs">The list of attributes</param>
        /// <param name="_attrib_divisor">Attribute step divisor</param>
        /// <param name="_init_size">Initial size of the buffer in number of attributes</param>
        /// <returns>Possible error</returns>
        std::optional<GLError> AddBufferImpl(
            std::type_info const* _attrib_type,
            size_t _attrib_size,
            std::vector<VertexAttribute> _attribs,
            uint32_t _attrib_divisor,
            size_t _init_size
        );

        /// <summary>
        /// Append a single vertex to buffer _buf_index.
        /// The type of the pushed value is compared
        /// against the registered type in the buffer.
        /// If the buffer is overflowed, the function 
        /// will fail
        /// </summary>
        /// <typeparam name="Vert"></typeparam>
        /// <param name="_buf_index">Index of buffer</param>
        /// <param name="_vertex">Vertex value</param>
        /// <returns>Possible error</returns>
        template <detail::Vertex Vert>
        std::optional<GLError> PushVertexDataTyped(size_t _buf_index, Vert const& _vertex) {
            return PushVertexDataTypedImpl(_buf_index, &typeid(Vert),
                std::bit_cast<void*>(&_vertex), sizeof(Vert));
        }

        /// <summary>
        /// Append a list of vertices to buffer _buf_index.
        /// The type of the pushed values is compared
        /// against the registered type in the buffer.
        /// If the buffer is overflowed, the function 
        /// will fail
        /// </summary>
        /// <typeparam name="Vert"></typeparam>
        /// <param name="_buf_index">Index of buffer</param>
        /// <param name="_vertices">List of vertices</param>
        /// <returns>Possible error</returns>
        template <detail::Vertex Vert>
        std::optional<GLError> PushVertexDataTyped(size_t _buf_index, std::vector<Vert> const& _vertices) {
            return PushVertexDataTypedImpl(_buf_index, &typeid(Vert),
                std::bit_cast<void*>(_vertices.data()), sizeof(Vert) * _vertices.size());
        }

        /// <summary>
        /// Append a single vertex to buffer _buf_index,
        /// without type comparison.
        /// If the buffer is overflowed, the function will fail
        /// </summary>
        /// <typeparam name="Vert"></typeparam>
        /// <param name="_buf_index">Index of buffer</param>
        /// <param name="_vertex">Vertex value</param>
        /// <returns>Possible error</returns>
        template <detail::Vertex Vert>
        std::optional<GLError> PushVertexDataUntyped(size_t _buf_index, Vert const& _vertex) {
            return PushVertexDataUnsafe(_buf_index, std::bit_cast<void*>(&_vertex),
                sizeof(Vert));
        }

        /// <summary>
        /// Append a list of vertices to buffer _buf_index, 
        /// without type comparison.
        /// If the buffer is overflowed, the function 
        /// will fail
        /// </summary>
        /// <typeparam name="Vert"></typeparam>
        /// <param name="_buf_index">Index of buffer</param>
        /// <param name="_vertices">List of vertices</param>
        /// <returns>Possible error</returns>
        template <detail::Vertex Vert>
        std::optional<GLError> PushVertexDataUntyped(size_t _buf_index, std::vector<Vert> const& _vertices) {
            return PushVertexDataUnsafe(_buf_index, std::bit_cast<void*>(_vertices.data()), sizeof(Vert) * _vertices.size());
        }

        /// <summary>
        /// Push _size_bytes of data inside buffer
        /// _buf_index, comparing the data type
        /// described by _type with the type
        /// of the buffer (DO NOT USE DIRECTLY UNLESS NEEDED)
        /// </summary>
        /// <param name="_buf_index">Buffer index</param>
        /// <param name="_type">RTTI data type</param>
        /// <param name="_data">Data src buffer</param>
        /// <param name="_size_bytes">Number of bytes to copy</param>
        /// <returns>Possible error</returns>
        std::optional<GLError> PushVertexDataTypedImpl(
            size_t _buf_index,
            std::type_info const* _type,
            const void* _data,
            size_t _size_bytes
        );

        /// <summary>
        /// Unsafe push data to the underlying buffer
        /// _buf_index. Does not check whether _buf_index
        /// is valid and does not check that the type
        /// of data and size are correct w.r.t. the
        /// buffer. If the size of the written data
        /// overflows the buffer size, the function will
        /// fail
        /// </summary>
        /// <param name="_buf_index"></param>
        /// <param name="_data"></param>
        /// <param name="_size_bytes"></param>
        /// <returns></returns>
        std::optional<GLError> PushVertexDataUnsafe(size_t _buf_index, const void* _data, size_t _size_bytes);

        /// <summary>
        /// Get currently used buffer size (in number
        /// of vertices) for buffer _buf_index
        /// </summary>
        /// <param name="_buf_index">Index of the buffer</param>
        /// <returns>Buffer used size</returns>
        FORCE_INLINE std::optional<size_t> GetBufferSize(size_t _buf_index) const {
            if (_buf_index > m_buffers.size()) {
                return std::nullopt;
            }
            return m_buf_sizes[_buf_index];
        }

        /// <summary>
        /// Get buffer capacity (in number of vertices)
        /// for buffer _buf_index
        /// </summary>
        /// <param name="_buf_index">Index of the buffer</param>
        /// <returns>Buffer capacity</returns>
        FORCE_INLINE std::optional<size_t> GetBufferCapacity(size_t _buf_index) const {
            if (_buf_index > m_buffers.size()) {
                return std::nullopt;
            }
            return m_buffers[_buf_index].GetBufferSize() /
                m_attrib_sizes[_buf_index];
        }


        FORCE_INLINE std::optional<GLError> ClearBuffer(size_t _buf_index) {
            if (_buf_index > m_buffers.size()) {
                return GLError::CLEAR_FAILED_INVALID_BUFFER;
            }
            m_buf_sizes[_buf_index] = 0;
            return std::nullopt;
        }

        FORCE_INLINE void ClearAllBuffers() {
            for (auto& buf_size : m_buf_sizes) {
                buf_size = 0;
            }
        }

        /// <summary>
        /// Vertex buffer resizing is explicit, use this function
        /// to create a new vertex buffer, still containing the
        /// data from the provided buffer, but with the new sizes
        /// </summary>
        /// <param name="_new_sizes"></param>
        /// <returns></returns>
        static std::expected<VertexBuffer, GLError> CreateResizedFrom(VertexBuffer const& _other, std::vector<size_t> const& _new_sizes);

        /// <summary>
        /// Clone this vertex buffer
        /// </summary>
        /// <returns></returns>
        std::expected<VertexBuffer, GLError> Clone() const;

        ~VertexBuffer();

        /// <summary>
        /// Bind the VAO
        /// </summary>
        void Bind() const;

        void Unbind() const;

        void SetLabel(std::string const& _label) const;
        void SetLabel(std::string_view   _label) const;

    private:
        VertexBuffer();
        VertexBuffer(VertexBuffer const& _other) = delete;

    private:
        // Current used size for each buffer
        std::vector<size_t>                       m_buf_sizes;
        uint32_t                                  m_vao;
        // Allocated buffers for the vertex attributes
        std::vector<GpuBuffer>                    m_buffers;
        // Type of each attribute type in each buffer
        std::vector<std::type_info const*>        m_attrib_types;
        std::vector<size_t>                       m_attrib_sizes;
        // Attribute index base for the next buffer
        uint32_t                                  m_curr_attrib_index;
        std::vector<uint32_t>                     m_attrib_divisors;
        std::vector<std::vector<VertexAttribute>> m_buf_attribs;
    };
}