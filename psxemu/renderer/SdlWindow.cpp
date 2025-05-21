#include "SdlWindow.hpp"
#include "GlLoad.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include <fmt/format.h>
#include <string>

namespace psx::video {

	void __stdcall DebugCallback(GLenum source,
		GLenum type,
		GLuint id,
		GLenum severity,
		GLsizei length,
		const GLchar* message,
		const void* userParam) {
		//if (type == GL_DEBUG_TYPE_ERROR) {
		//	error::DebugBreak();
		//}
			
		std::string msg{ message, (size_t)length };
		fmt::println("[OPENGL] {}", msg);
	}

	SdlWindow::SdlWindow(std::string name, Rect size, bool reuse_ctx, bool resize, bool enable_debug)
		: m_win{ nullptr }, m_gl_ctx{ nullptr }, m_blit{ nullptr },
		m_close{}, m_vert_buf{ nullptr }, m_tex_id{}, m_ev_callbacks{},
		m_forward_ev_handler{}, m_size{ size } {
		if (reuse_ctx)
			SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
		else
			SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);

			SDL_GL_LoadLibrary(nullptr);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
				SDL_GL_CONTEXT_PROFILE_CORE);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
			SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	

		auto flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;

		if (resize)
			flags |= SDL_WINDOW_RESIZABLE;

		m_win = SDL_CreateWindow(name.c_str(),
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			(int)size.w, (int)size.h, flags);

		m_gl_ctx = SDL_GL_CreateContext((SDL_Window*)m_win);

		if (!GlIsInit()) {
			if (!m_gl_ctx) {
				fmt::println("[RENDERER] OpenGL context creation failed : {}",
					SDL_GetError());
				throw std::runtime_error("SDL_GL_CreateContext failed");
			}

			if (!GlInit())
				throw std::runtime_error("GlInit() failed");

			int32_t major = {};
			int32_t minor = {};

			glGetIntegerv(GL_MAJOR_VERSION, &major);
			glGetIntegerv(GL_MINOR_VERSION, &minor);

			int32_t mask = {};

			//glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &mask);

			fmt::println("[RENDERER] Using OpenGL {}.{}", major, minor);

			//if (mask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT)
				//fmt::println("[RENDERER] Compatibility profile!");
		}

		if (enable_debug) {
			GlEnableDebugOutput();
			GlSetDebugMessageCallback(DebugCallback, nullptr);
		}

		auto errors = GlGetErrors();

		if (!errors.empty()) {
			for (auto const& err : errors)
				fmt::println("[OPENGL] Error : {}",
					(const char*)glewGetErrorString(err));
		}
	}

	SdlWindow::SdlWindow(std::string name, Rect size, std::string blit_loc, std::string blit_name, bool reuse_ctx, bool resize, bool enable_debug)
		: SdlWindow(name, size, reuse_ctx, resize, enable_debug) {
		m_blit = new Shader(blit_loc, blit_name);
		m_blit->SetLabel(fmt::format("window_{}_blit_shader", name));

		m_vert_buf = new VertexBuffer<HostVertex2D>(6);

		m_vert_buf->PushVertex(HostVertex2D{ -1.0, 1.0,  0.0, 0.0 });
		m_vert_buf->PushVertex(HostVertex2D{ 1.0, -1.0,  1.0, 1.0 });
		m_vert_buf->PushVertex(HostVertex2D{ -1.0, -1.0, 0.0, 1.0 });

		m_vert_buf->PushVertex(HostVertex2D{ -1.0, 1.0, 0.0, 0.0 });
		m_vert_buf->PushVertex(HostVertex2D{ 1.0, 1.0,  1.0, 0.0 });
		m_vert_buf->PushVertex(HostVertex2D{ 1.0, -1.0, 1.0, 1.0 });

		m_vert_buf->SetLabel(fmt::format("static_window_{}_vertex_buf", name));
	}

	void SdlWindow::Clear() {
		SDL_GL_MakeCurrent((SDL_Window*)m_win, m_gl_ctx);

		int curr_viewport[4] = {};
		glGetIntegerv(GL_VIEWPORT, curr_viewport);

		glViewport(0, 0, (GLsizei)m_size.w, (GLsizei)m_size.h);

		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glViewport(curr_viewport[0], curr_viewport[1], curr_viewport[2], curr_viewport[3]);
	}

	void SdlWindow::Present() {
		auto errors = GlGetErrors();

		if (!errors.empty()) {
			for (auto const& err : errors)
				fmt::println("[OPENGL] Error : {}",
					(const char*)glewGetErrorString(err));
		}

		SDL_GL_MakeCurrent((SDL_Window*)m_win, m_gl_ctx);

		SDL_GL_SwapWindow((SDL_Window*)m_win);
	}

	void SdlWindow::Blit(uint32_t m_texture_id) {
		SDL_GL_MakeCurrent((SDL_Window*)m_win, m_gl_ctx);

		if (!m_blit || !m_vert_buf)
			throw std::runtime_error("Window is not ready for blit ops");

		int curr_viewport[4] = {};
		glGetIntegerv(GL_VIEWPORT, curr_viewport);

		glViewport(0, 0, (GLsizei)m_size.w, (GLsizei)m_size.h);

		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		m_blit->BindProgram();
		m_vert_buf->Bind();

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_texture_id);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		m_vert_buf->Unbind();

		glViewport(curr_viewport[0], curr_viewport[1], curr_viewport[2], curr_viewport[3]);
	}

	bool SdlWindow::HandleEvent(SDL_Event* ev) {
		if (m_forward_ev_handler) m_forward_ev_handler(ev);

			switch (ev->type)
			{
			case SDL_WINDOWEVENT:
				if (ev->window.event == SDL_WINDOWEVENT_CLOSE)
					m_close = true;
				else if (ev->window.event == SDL_WINDOWEVENT_RESIZED ||
					ev->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
					int h{}, w{};
					SDL_GetWindowSize((SDL_Window*)m_win, &w, &h);
					m_size.w = (size_t)w;
					m_size.h = (size_t)h;
				}
				break;
			case SDL_QUIT:
				m_close = true;
				break;
			case SDL_KEYDOWN:
				if (!HasInputFocus()) return false;
				DispatchEvent(SdlEvent::KeyPressed, std::any{ std::string_view{ SDL_GetKeyName(ev->key.keysym.sym) }});
				break;
			case SDL_KEYUP:
				if (!HasInputFocus()) return false;
				DispatchEvent(SdlEvent::KeyReleased, std::any{ std::string_view{ SDL_GetKeyName(ev->key.keysym.sym) } });
				break;
			default:
				return false;
				break;
			}

			return true;
	}

	SdlWindow::~SdlWindow() {
		if(m_blit) delete m_blit;
		if(m_vert_buf) delete m_vert_buf;
		SDL_GL_DeleteContext(m_gl_ctx);
		SDL_DestroyWindow((SDL_Window*)m_win);
	}

	void SdlWindow::Listen(SdlEvent ev, EventCallback callback) {
		m_ev_callbacks.insert(std::pair{ ev, std::move(callback) });
	}

	void SdlWindow::DispatchEvent(SdlEvent ev, std::any data) {
		auto range = m_ev_callbacks.equal_range(ev);
		
		for (auto& entry = range.first; entry != range.second; entry++)
			entry->second(ev, data);
	}

	void* SdlWindow::GetNativeWindowHandle() const {
		SDL_SysWMinfo wminfo{};
		SDL_VERSION(&wminfo.version);
		SDL_GetWindowWMInfo((SDL_Window*)m_win, &wminfo);
		return (void*)wminfo.info.win.window;
	}

	void SdlWindow::SetTextureWindow(u32 start_x, u32 start_y, Rect window_size, Rect texture_size) {
		m_vert_buf->Clear();

		float w = float(window_size.w) / texture_size.w;
		float h = float(window_size.h) / texture_size.h;

		float xoff = float(start_x) / texture_size.w;
		float yoff = float(start_y) / texture_size.h;
		float endx = xoff + w;
		float endy = yoff + h;

		m_vert_buf->PushVertex(HostVertex2D{ -1.0, 1.0,  xoff, yoff });
		m_vert_buf->PushVertex(HostVertex2D{ 1.0, -1.0,  endx, endy });
		m_vert_buf->PushVertex(HostVertex2D{ -1.0, -1.0, xoff, endy });

		m_vert_buf->PushVertex(HostVertex2D{ -1.0, 1.0, xoff, yoff });
		m_vert_buf->PushVertex(HostVertex2D{ 1.0, 1.0,  endx, yoff });
		m_vert_buf->PushVertex(HostVertex2D{ 1.0, -1.0, endx, endy });
	}

	uint32_t SdlWindow::GetWindowID() const {
		return SDL_GetWindowID((SDL_Window*)m_win);
	}

	bool SdlWindow::HasMouseFocus() const {
		auto flags = SDL_GetWindowFlags((SDL_Window*)m_win);

		return (bool)(flags & SDL_WINDOW_MOUSE_FOCUS);
	}

	bool SdlWindow::HasInputFocus() const {
		auto flags = SDL_GetWindowFlags((SDL_Window*)m_win);

		return (bool)(flags & SDL_WINDOW_INPUT_FOCUS);
	}

	void SdlWindow::SetSize(Rect sz) {
		m_size = sz;
		SDL_SetWindowSize((SDL_Window*)m_win,
			(int)sz.w, (int)sz.h);
	}

	void SdlWindow::ForwardEventHandler(std::function<void(SDL_Event*)> handler) {
		m_forward_ev_handler = handler;
	}


	void SdlWindow::MakeContextCurrent() const {
		SDL_GL_MakeCurrent((SDL_Window*)m_win, m_gl_ctx);
	}
}