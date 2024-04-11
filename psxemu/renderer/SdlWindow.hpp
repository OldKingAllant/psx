#pragma once

#include "Shader.hpp"
#include "VertexBuffer.hpp"

namespace psx::video {
	struct Rect {
		size_t w, h;
	};

	class SdlWindow {
	public :
		SdlWindow(std::string name, Rect size, std::string blit_loc, std::string blit_name, bool reuse_ctx, bool resize);

		void Blit(uint32_t m_texture_id);

		bool EventLoop();

		~SdlWindow();

		bool CloseRequest() const {
			return m_close;
		}

	private :
		void* m_win;
		void* m_gl_ctx;
		Shader* m_blit;
		bool m_close;
		VertexBuffer<HostVertex2D>* m_vert_buf;
		u32 m_tex_id;
	};
}