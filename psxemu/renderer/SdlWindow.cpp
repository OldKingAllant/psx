#include "SdlWindow.hpp"
#include "GlLoad.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include <fmt/format.h>
#include <string>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

namespace psx::video {

	void __stdcall DebugCallback(GLenum source,
		GLenum type,
		GLuint id,
		GLenum severity,
		GLsizei length,
		const GLchar* message,
		const void* userParam) {
		std::string msg{ message, (size_t)length };

		if (!logger::Logger::get().is_running()) {
			fmt::print("[OPENGL] {}\n", msg);
		}
		else {
			switch (type) {
			case GL_DEBUG_SEVERITY_LOW:
				LOG_DEBUG("OPENGL", "[OPENGL] {}\n", msg);
				break;
			case GL_DEBUG_SEVERITY_NOTIFICATION:
				LOG_INFO("OPENGL", "[OPENGL] {}\n", msg);
				break;
			case GL_DEBUG_SEVERITY_MEDIUM:
				LOG_WARN("OPENGL", "[OPENGL] {}\n", msg);
				break;
			case GL_DEBUG_SEVERITY_HIGH:
				LOG_ERROR("OPENGL", "[OPENGL] {}\n", msg);
				break;
			default:
				LOG_DEBUG("OPENGL", "[OPENGL] {}\n", msg);
			}
		}
	}

	SdlWindow::SdlWindow(std::string name, Rect size, bool reuse_ctx, bool resize, bool enable_debug)
		: m_win{ nullptr }, m_gl_ctx{}, m_blit{ nullptr },
		m_close{}, m_vert_buf{ nullptr }, m_tex_id{}, m_ev_callbacks{},
		m_forward_ev_handler{}, m_size{ size }, m_last_title_update_time{},
		m_curr_frame_count{}, m_window_name{ name } {
		if (reuse_ctx)
			SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
		else
			SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
	

		auto flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;

		if (resize)
			flags |= SDL_WINDOW_RESIZABLE;

		m_win = SDL_CreateWindow(name.c_str(),
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			(int)size.w, (int)size.h, flags);

		m_gl_ctx.m_handle = (void*)SDL_GL_CreateContext((SDL_Window*)m_win);
		m_gl_ctx.SetCurrent(m_win);

		if (!GlIsInit()) {
			if (!m_gl_ctx.GetHandle()) {
				fmt::println("[RENDERER] OpenGL context creation failed : {}",
					SDL_GetError());
				throw std::runtime_error("SDL_GL_CreateContext failed");
			}

			if (!GlInit()) {
				throw std::runtime_error("GlInit() failed");
			}

			int32_t major = {};
			int32_t minor = {};

			glGetIntegerv(GL_MAJOR_VERSION, &major);
			glGetIntegerv(GL_MINOR_VERSION, &minor);

			int32_t mask = {};

			fmt::println("[RENDERER] Using OpenGL {}.{}", major, minor);
		}

		if (enable_debug) {
			GlEnableDebugOutput();
			GlSetDebugMessageCallback(DebugCallback, nullptr);
		}

		auto errors = GlGetErrors();

		if (!errors.empty()) {
			for (auto const& err : errors)
				fmt::println("[OPENGL] ERROR : {}",
					(const char*)glewGetErrorString(err));
		}

		m_last_title_update_time = std::chrono::system_clock::now();
		//SDL_GL_SetSwapInterval(0);
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
		m_gl_ctx.SetCurrent(m_win);
		m_gl_ctx.SetViewport(0, 0, m_size.w, m_size.h);
		m_gl_ctx.BindFramebuffer(GL_FRAMEBUFFER, 0);

		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	void SdlWindow::Present() {
		auto errors = GlGetErrors();

		if (!errors.empty()) {
			for (auto const& err : errors)
				fmt::println("[OPENGL] ERROR : {}",
					(const char*)glewGetErrorString(err));
		}

		m_curr_frame_count++;
		auto curr_time = std::chrono::system_clock::now();
		auto diff = std::chrono::duration_cast<std::chrono::seconds>(curr_time - m_last_title_update_time).count();
		
		if (diff >= 1) {
			auto new_title = std::format("{} | {} fps", m_window_name, m_curr_frame_count);
			m_curr_frame_count = 0;
			SDL_SetWindowTitle((SDL_Window*)m_win, new_title.c_str());
			m_last_title_update_time = std::chrono::system_clock::now();
		}

		m_gl_ctx.SetCurrent(m_win);
		SDL_GL_SwapWindow((SDL_Window*)m_win);
	}

	void SdlWindow::Blit(uint32_t texture_id) {
		m_gl_ctx.SetCurrent(m_win);

		if (!m_blit || !m_vert_buf)
			throw std::runtime_error("Window is not ready for blit ops");

		m_gl_ctx.ScissorDisable();
		m_gl_ctx.BlendDisable();
		m_gl_ctx.SetViewport(0, 0, m_size.w, m_size.h);
		m_gl_ctx.BindFramebuffer(GL_FRAMEBUFFER, 0);

		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		m_blit->BindProgram();
		m_vert_buf->Bind();

		m_gl_ctx.SetTextureSlot(GL_TEXTURE0);
		m_gl_ctx.BindTexture({ .type = GL_TEXTURE_2D, .handle = texture_id });

		glDrawArrays(GL_TRIANGLES, 0, 6);

		m_vert_buf->Unbind();
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

	Rect SdlWindow::GetSize() {
		int w{}, h{};
		SDL_GetWindowSize((SDL_Window*)m_win,
			&w, &h);
		return Rect{.w = (size_t)w, .h = (size_t)h};
	}

	void SdlWindow::ForwardEventHandler(std::function<void(SDL_Event*)> handler) {
		m_forward_ev_handler = handler;
	}

	void SdlWindow::MakeContextCurrent() {
		m_gl_ctx.SetCurrent(m_win);
	}
}