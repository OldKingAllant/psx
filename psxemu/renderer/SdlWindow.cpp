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
		//if (type == GL_DEBUG_TYPE_PERFORMANCE)
			//return;

		std::string msg{ message, (size_t)length };
		fmt::println("[OPENGL] {}", msg);
	}

	SdlWindow::SdlWindow(std::string name, Rect size, std::string blit_loc, std::string blit_name, bool reuse_ctx, bool resize)
		: m_win{ nullptr }, m_gl_ctx { nullptr }, m_blit{ nullptr }, 
		m_close{}, m_vert_buf{ nullptr }, m_tex_id{}, m_ev_callbacks{} {
		if (reuse_ctx)
			SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
		else
			SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);

		if (!GlIsInit()) {
			SDL_GL_LoadLibrary(nullptr);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
				SDL_GL_CONTEXT_PROFILE_CORE);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
			SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		}

		auto flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;

		if (resize)
			flags |= SDL_WINDOW_RESIZABLE;

		m_win = SDL_CreateWindow(name.c_str(),
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			size.w, size.h, flags);

		if (!GlIsInit()) {
			m_gl_ctx = SDL_GL_CreateContext((SDL_Window*)m_win);

			if (!m_gl_ctx) {
				fmt::println("[RENDERER] OpenGL context creation failed : {}",
					SDL_GetError());
				throw std::runtime_error("SDL_GL_CreateContext failed");
			}

			if(!GlInit())
				throw std::runtime_error("GlInit() failed");

			GlEnableDebugOutput();
			GlSetDebugMessageCallback(DebugCallback, nullptr);

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

		auto errors = GlGetErrors();

		if (!errors.empty()) {
			for (auto const& err : errors)
				fmt::println("[OPENGL] Error : {}",
					(const char*)glewGetErrorString(err));
		}

		m_vert_buf = new VertexBuffer<HostVertex2D>(6);

		m_vert_buf->PushVertex(HostVertex2D{ -1.0, 1.0,  0.0, 0.0 });
		m_vert_buf->PushVertex(HostVertex2D{ 1.0, -1.0,  1.0, 1.0 });
		m_vert_buf->PushVertex(HostVertex2D{ -1.0, -1.0, 0.0, 1.0 });

		m_vert_buf->PushVertex(HostVertex2D{ -1.0, 1.0, 0.0, 0.0 });
		m_vert_buf->PushVertex(HostVertex2D{ 1.0, 1.0,  1.0, 0.0 });
		m_vert_buf->PushVertex(HostVertex2D{ 1.0, -1.0, 1.0, 1.0 });
		
		m_blit = new Shader(blit_loc, blit_name);

		m_tex_id = m_blit->UniformLocation(std::string("vram_tex")).value_or((uint32_t)-1);

		if (m_tex_id == (uint32_t)-1)
			throw std::runtime_error("Cannot find texture uniform");

		m_vert_buf->SetLabel("static_vram_view_blit_vertex_buf");
		m_blit->SetLabel("vram_view_blit_shader");
	}

	void SdlWindow::Blit(uint32_t m_texture_id) {
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		m_blit->BindProgram();
		m_vert_buf->Bind();

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_texture_id);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		m_vert_buf->Unbind();

		SDL_GL_SwapWindow((SDL_Window*)m_win);
	}

	bool SdlWindow::EventLoop() {
		SDL_Event next_ev{};

		while (SDL_PollEvent(&next_ev)) {
			switch (next_ev.type)
			{
			case SDL_WINDOWEVENT:
				if (next_ev.window.type == SDL_WINDOWEVENT_CLOSE)
					m_close = true;
				break;
			case SDL_QUIT:
				m_close = true;
				break;
			case SDL_KEYDOWN:
				DispatchEvent(SdlEvent::KeyPressed, std::any{ std::string_view{ SDL_GetKeyName(next_ev.key.keysym.sym) }});
				break;
			case SDL_KEYUP:
				DispatchEvent(SdlEvent::KeyReleased, std::any{ std::string_view{ SDL_GetKeyName(next_ev.key.keysym.sym) } });
				break;
			default:
				break;
			}
		}

		return !m_close;
	}

	SdlWindow::~SdlWindow() {
		if(m_blit)
			delete m_blit;
		delete m_vert_buf;
		SDL_GL_DeleteContext(m_gl_ctx);
		SDL_DestroyWindow((SDL_Window*)m_win);
	}

	void SdlWindow::Listen(SdlEvent ev, EventCallback callback) {
		m_ev_callbacks.insert(std::pair{ ev, std::move(callback) });
	}

	void SdlWindow::DispatchEvent(SdlEvent ev, std::any data) {
		auto range = m_ev_callbacks.equal_range(ev);
		
		for (auto entry = range.first; entry != range.second; entry++)
			entry->second(ev, data);
	}

	void* SdlWindow::GetNativeWindowHandle() const {
		SDL_SysWMinfo wminfo{};
		SDL_VERSION(&wminfo.version);
		SDL_GetWindowWMInfo((SDL_Window*)m_win, &wminfo);
		return (void*)wminfo.info.win.window;
	}
}