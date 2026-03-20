#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include <string_view>
#include <optional>
#include <string>

namespace psx::video {
	class FrameBuffer {
	public :
		FrameBuffer(u32 vram_x_size, u32 vram_y_size);

		void RebuildUpscaledFbo(u32 resolution_multiplier);

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

		FORCE_INLINE u32 GetResolutionMultiplier() const {
			return m_resolution_multiplier;
		}

		FORCE_INLINE std::optional<u32> GetUpscaledFbo() const {
			return m_upscaled_fbo;
		}

		FORCE_INLINE std::optional<u32> GetUpscaledTexture() const {
			return m_upscaled_tex;
		}

		void Bind();
		void Unbind();

		void SetLabel(std::string_view label);
		void SetUpscaledFboLabel();

		~FrameBuffer();

	private :
		u32 m_x_size;
		u32 m_y_size;
		std::string m_base_label;
		u32 m_fbo;
		u32 m_fbo_tex;
		u32 m_mask_texture;
		u32 m_resolution_multiplier;
		std::optional<u32> m_upscaled_tex;
		std::optional<u32> m_upscaled_fbo;
	};
}