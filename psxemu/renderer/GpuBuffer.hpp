#pragma once 

#include <common/Macros.hpp>
#include "GLError.hpp"

#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace psx::video {
	/// <summary>
	/// Pattern of usage of a GPU buffer (ideally
	/// used by the driver to optimize buffer
	/// location and read/write)
	/// 
	/// <para/>
	/// Documenation:
	/// https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBufferData.xhtml
	/// </summary>
	enum class BufferUsage {
		STREAM_DRAW = 1,
		STREAM_READ = 2,
		STREAM_COPY = 4,
		STATIC_DRAW = 8,
		STATIC_READ = 16,
		STATIC_COPY = 32,
		DYNAMIC_DRAW = 64,
		DYNAMIC_READ = 128,
		DYNAMIC_COPY = 256
	};

	/// <summary>
	/// Protection flags for
	/// buffer mappings to
	/// host memory
	/// </summary>
	enum class BufferMapping {
		DYNAMIC = 1,    // Allow the CPU to directly modify the buffer
		READ = 2,       // Allow CPU read
		WRITE = 4,      // Allow CPU write
		PERSISTENT = 8, // Allow GPU read/write while the buffer is CPU mapped
		COHERENT = 16   // "Automatically" flush the buffer when either CPU/GPU write to it
	};

	// Declare logical operators for the enum
	DECLARE_ENUM_LOGICAL_OPS(BufferMapping)

	/// <summary>
	/// This class represent any buffer on the the GPU.
	/// Buffers are used for many things, such as vertex
	/// data, indices, staging, uniforms and things
	/// like SSBOs.
	/// https://wikis.khronos.org/opengl/Buffer_Object
	/// Under normal circumstances, an OpenGL
	/// buffer is not directly accessible by 
	/// the CPU. To be read/written directly,
	/// the buffer must be host mapped, with
	/// the reuqired access flags. A mapped
	/// buffer may need manual coherency
	/// if not mapped as PERSISTENT and
	/// COHERENT
	/// https://wikis.khronos.org/opengl/Memory_Model
	/// </summary>
	class GpuBuffer {
	public:
		GpuBuffer(GpuBuffer&& _prev) noexcept;
		~GpuBuffer() noexcept;

		GpuBuffer& operator=(GpuBuffer&& _other) noexcept;

		/// <summary>
		/// DO NOT USE DIRECTLY UNLESS NECESSARY
		/// </summary>
		GpuBuffer();

		/// <summary>
		/// Create GPU buffer with the given allocated
		/// size and protections. Memory will be uninitialized
		/// </summary>
		/// <param name="_size_bytes">Size in bytes of the buffer</param>
		/// <param name="_prots">Buffer storage protection flags</param>
		/// <returns>Buffer or error</returns>
		static std::expected<GpuBuffer, GLError> CreateBuffer(size_t _size_bytes, BufferMapping _prots);

		void BindBuffer(uint32_t _target) const;

		/// <summary>
		/// Resize the GPU-side buffer. The
		/// function will fail if the buffer
		/// is host mapped. In any case,
		/// if the function fails, the 
		/// internal buffer will still be valid
		/// (unless it was already in an undefined state)
		/// </summary>
		/// <param name="_size_bytes">New size in bytes</param>
		/// <param name="_prots">Buffer storage protection flags</param>
		/// <returns>Error if any</returns>
		std::optional<GLError> ResizeBuffer(size_t _size_bytes, BufferMapping _prots);

		/// <summary>
		/// Write buffer
		/// </summary>
		/// <param name="_data">Data to write</param>
		/// <param name="_offset_bytes">Offset in buffer in bytes</param>
		/// <param name="_len_bytes">Len in bytes</param>
		/// <returns>Error if any</returns>
		std::optional<GLError> WriteBufferSubData(const void* _data, size_t _offset_bytes, size_t _len_bytes) const;

		/// <summary>
		/// Read buffer
		/// </summary>
		/// <param name="_data">Read data into buffer</param>
		/// <param name="_offset_bytes">Offset in buffer in bytes</param>
		/// <param name="_len_bytes">Len in bytes</param>
		/// <returns>Error if any</returns>
		std::optional<GLError> ReadBufferSubData(void* _data, size_t _offset_bytes, size_t _len_bytes) const;

		/// <summary>
		/// Map buffer to host memory. If the mapping fails,
		/// and the buffer was already mapped with a different
		/// configuration, you can assume that the previous
		/// mapping is no longer valid.
		/// Will fail if offset + len > buffer size
		/// </summary>
		/// <param name="_offset_bytes">Offset in the buffer in bytes</param>
		/// <param name="_len_bytes">Size of the mapping in bytes</param>
		/// <param name="_prots">Access flags to the mapping</param>
		/// <returns>Error if any</returns>
		std::optional<GLError> MapBuffer(size_t _offset_bytes, size_t _len_bytes, BufferMapping _prots);

		/// <summary>
		/// Opposite of BufferMap
		/// </summary>
		void UnmapBuffer();

		/// <summary>
		/// Flush writes to the GPU
		/// </summary>
		void FlushBuffer() const;

		/// <summary>
		/// Copy _len_bytes starting from _offset_bytes
		/// from the buffer _other to this buffer.
		/// If any of the two buffers is host mapped,
		/// both must be. And if both are mapped, they
		/// must use the PERSISTENT map flag. 
		/// Failing to meet one of the above requirements
		/// will result in the function failing
		/// </summary>
		/// <param name="_other">The source buffer</param>
		/// <param name="_src_offset_bytes"></param>
		/// <param name="_dst_offset_bytes"></param>
		/// <param name="_len_bytes"></param>
		/// <returns>Error if any</returns>
		std::optional<GLError> CopyFromBufferRange(GpuBuffer const& _other,
			size_t _src_offset_bytes,
			size_t _dst_offset_bytes,
			size_t _len_bytes) const;

		/// <summary>
		/// Return a copy of this buffer. If the buffer
		/// is mapped and not PERSISTENT, the function
		/// will fail. Host mapping is not replicated
		/// (e.g. if this buffer is host mapped in any way,
		/// the clone won't be)
		/// </summary>
		/// <returns>Copy or error</returns>
		std::expected<GpuBuffer, GLError> Clone() const;

		//////////////////// GETTERS FOR BUFFER PROPERTIES //////////////////////////

		FORCE_INLINE uint32_t GetBufferId() const {
			return m_buf_id;
		}

		FORCE_INLINE size_t GetBufferSize() const {
			return m_curr_size;
		}

		FORCE_INLINE BufferMapping GetBufferProts() const {
			return m_buf_prots;
		}

		//////////////////// GETTERS FOR HOST MAPPING //////////////////////////

		FORCE_INLINE bool IsMapped() const {
			return m_is_mapped;
		}

		FORCE_INLINE std::optional<void*> GetHostBuffer() const {
			if (!m_is_mapped) {
				return std::nullopt;
			}
			return m_mapped_ptr;
		}

		FORCE_INLINE std::optional<size_t> GetMapOffset() const {
			if (!m_is_mapped) {
				return std::nullopt;
			}
			return m_map_offset;
		}

		FORCE_INLINE std::optional<size_t> GetMapSize() const {
			if (!m_is_mapped) {
				return std::nullopt;
			}
			return m_map_size;
		}

		FORCE_INLINE std::optional<BufferMapping> GetMapProts() const {
			if (!m_is_mapped) {
				return std::nullopt;
			}
			return m_curr_prots;
		}

		/////////////////////////////////////////////////

		void SetLabel(std::string const& _label) const;
		void SetLabel(std::string_view   _label) const;

	private:
		GpuBuffer(GpuBuffer const& _other) = delete;

	private:
		BufferMapping m_buf_prots;
		uint32_t      m_buf_id;
		// If the buffer is currently mapped,
		// this points to the mapped memory
		void* m_mapped_ptr;
		bool          m_is_mapped;
		BufferMapping m_curr_prots;
		size_t        m_curr_size;
		// Offset in buffer of the mapping
		size_t        m_map_offset;
		// Size in bytes of the mapping
		size_t        m_map_size;
	};
}