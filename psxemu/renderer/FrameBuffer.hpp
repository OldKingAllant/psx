#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

namespace psx::video {
	class FrameBuffer {
	public :
		FrameBuffer();

		void UpdateInternalTexture(u32 blit_tex);
		void CopyToTexture(u32 dest);

		FORCE_INLINE u32 GetFbo() const {
			return m_fbo;
		}

		FORCE_INLINE u32 GetInternalTexture() const {
			return m_fbo_tex;
		}

		void Bind();
		void Unbind();

		~FrameBuffer();

	private :
		u32 m_fbo;
		u32 m_fbo_tex;
	};
}