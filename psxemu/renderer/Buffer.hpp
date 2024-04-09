#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

namespace psx::video {
	enum class BufferMap {
		READ,
		WRITE,
		READWRITE
	};

	class Buffer {
	public :
		Buffer(bool map_persistent, BufferMap prot, u32 size);

		void Resize(u32 new_sz);

		void BufferSubData(const void* data, u64 off, u32 len);

		void* Map(u32 start, u32 len);
		void Umap();
		void Flush();

		FORCE_INLINE bool IsMapped() const {
			return m_mapped;
		}

		FORCE_INLINE void* Get() {
			return m_buffer_ptr;
		}

		FORCE_INLINE u32 BufID() const {
			return m_buffer_id;
		}

		~Buffer();

	private :
		void CreateBuffer();

	private :
		u32 m_buffer_id;
		void* m_buffer_ptr;
		bool m_mapped;
		bool m_persistent;
		BufferMap m_curr_prot;
		u32 m_sz;
	};
}