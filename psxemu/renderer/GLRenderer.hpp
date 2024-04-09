#pragma once

#include "Vram.hpp"

namespace psx::video {
	class Renderer {
	public :
		Renderer();

		void BlitEnd();
		void VBlank();

		u8* GetVramPtr() const {
			return m_vram.Get();
		}

		~Renderer();

	private :
		Vram m_vram;
	};
}