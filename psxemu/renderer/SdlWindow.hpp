#pragma once

#include "Shader.hpp"
#include "VertexBuffer.hpp"

#include <any>
#include <functional>
#include <map>

union SDL_Event;

namespace psx::video {
	struct Rect {
		size_t w, h;
	};

	enum class SdlEvent {
		KeyPressed,
		KeyReleased,
		WindowClose,
		WindowResized,
		WindowSizeChanged,
		Quit
	};

	using EventCallback = std::function<void(SdlEvent, std::any)>;

	class SdlWindow {
	public :
		SdlWindow(std::string name, Rect size, std::string blit_loc, std::string blit_name, bool reuse_ctx, bool resize, bool enable_debug = false);
		SdlWindow(std::string name, Rect size, bool reuse_ctx, bool resize, bool enable_debug = false);

		void SetTextureWindow(u32 start_x, u32 start_y, Rect window_size, Rect texture_size);
		void Blit(uint32_t m_texture_id);
		void Clear();
		void Present();

		bool HandleEvent(SDL_Event* ev);

		~SdlWindow();

		bool CloseRequest() const {
			return m_close;
		}

		void Listen(SdlEvent ev, EventCallback callback);
		void DispatchEvent(SdlEvent ev, std::any data);
		void ForwardEventHandler(std::function<void(SDL_Event*)> handler);

		void* GetGlContext() const {
			return m_gl_ctx;
		}

		void* GetWindowHandle() const {
			return m_win;
		}

		void* GetNativeWindowHandle() const;
		uint32_t GetWindowID() const;

		bool HasMouseFocus() const;
		bool HasInputFocus() const;

		void SetSize(Rect sz);

		void MakeContextCurrent() const;

	protected :
		void* m_win;
		void* m_gl_ctx;
		Shader* m_blit;
		bool m_close;
		VertexBuffer<HostVertex2D>* m_vert_buf;
		u32 m_tex_id;
		std::multimap<SdlEvent, EventCallback> m_ev_callbacks;
		std::function<void(SDL_Event*)> m_forward_ev_handler;
		Rect m_size;
	};
}