#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include <string_view>

namespace psx::video {
	class FrameBuffer {
	public :
		FrameBuffer();

		void UpdateInternalTexture(u32 blit_tex);
		void CopyToTexture(u32 dest);

		void UpdatePartial(u32 blit_tex, u32 xoff, u32 yoff, u32 w, u32 h);
		void DownloadSubImage(u8* dest_buf, u32 xoff, u32 yoff, u32 w, u32 h);

		FORCE_INLINE u32 GetFbo() const {
			return m_fbo;
		}

		FORCE_INLINE u32 GetInternalTexture() const {
			return m_fbo_tex;
		}

		FORCE_INLINE u32 GetMaskTexture() const {
			return m_mask_texture;
		}

		void Bind();
		void Unbind();

		void SetLabel(std::string_view label);

		~FrameBuffer();

	private :
		u32 m_fbo;
		u32 m_fbo_tex;
		u32 m_mask_texture;
	};
}