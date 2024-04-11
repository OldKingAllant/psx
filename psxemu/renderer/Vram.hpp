#pragma once

#include <common/Defs.hpp>

namespace psx::video {
	static constexpr u32 VRAM_SIZE_BYTES = 1024 * 1024;

	class Vram {
	public :
		Vram();

		/// <summary>
		/// Get pointer to host mapped 
		/// memory of the VRAM
		/// </summary>
		/// <returns>Pointer</returns>
		u8* Get() const;

		~Vram();

		/// <summary>
		/// Upload buffer data to
		/// texture storage
		/// </summary>
		void Upload();

		/// <summary>
		/// Retrieve OpenGL texture
		/// handle 
		/// </summary>
		/// <returns></returns>
		u32 GetTextureHandle() const {
			return m_texture_id;
		}

	private :
		void CreateBuffer();
		void MapBuffer();
		void UnmapBuffer();
		void DestroyBuffer();

	private :
		u32 m_texture_id;
		u32 m_buffer_id;
		u8* m_buffer_ptr;
	};
}