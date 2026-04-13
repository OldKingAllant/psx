#include "DisplayWindow.hpp"

#include <psxemu/renderer/GLContext.hpp>
#include <GL/glew.h>

#include <fmt/format.h>
#include <SDL2/SDL.h>

#include <thirdparty/imgui/imgui.h>
#include <thirdparty/imgui/imgui_internal.h>
#include <thirdparty/imgui/misc/cpp/imgui_stdlib.h>
#include <thirdparty/imgui/backends/imgui_impl_sdl2.h>
#include <thirdparty/imgui/backends/imgui_impl_opengl3.h>

#include <psxemu/include/psxemu/System.hpp>
#include <psxemu/include/psxemu/Kernel.hpp>

#include <vector>

//Expect that only ONE display window exists
static ImGuiContext* g_imgui_ctx{ nullptr };

static bool g_is_popup_open{ false };
static std::vector<std::pair<std::string, std::string>> g_popups{};

DisplayWindow::DisplayWindow(std::string name, psx::video::Rect size, std::string blit_loc,
	std::string blit16_name, std::string blit24_name, bool reuse_ctx, bool resize,
	bool enable_debug) : SdlWindow(name, size, blit_loc, blit16_name, 
		reuse_ctx, resize, enable_debug), m_sys{ nullptr },
		m_gdb_server{} {
	m_blit24_shader = new psx::video::Shader(blit_loc, blit24_name);
	m_blit24_shader->SetLabel(fmt::format("window_{}_blit24_shader", name));

	m_temp_buf.resize(1024ULL * 512);

	////////////////////////

	glGenBuffers(1, &m_ssbo_buf);

	m_gl_ctx.BindBuffer(GL_PIXEL_PACK_BUFFER, m_ssbo_buf);
	glBufferData(GL_PIXEL_PACK_BUFFER, 1024ULL * 512 * 2,
		nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_ssbo_buf);

	m_gl_ctx.BindBuffer(GL_PIXEL_PACK_BUFFER, 0);


	{
		GLint unpack_row_len{}, pack_row_len{};
		GLint pack_align{}, unpack_align{};

		glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpack_row_len);
		glGetIntegerv(GL_PACK_ROW_LENGTH, &pack_row_len);
		glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_align);
		glGetIntegerv(GL_PACK_ALIGNMENT, &pack_align);

		g_imgui_ctx = ImGui::CreateContext();
		ImGui::SetCurrentContext(g_imgui_ctx);

		ImGui_ImplSDL2_InitForOpenGL((SDL_Window*)m_win, m_gl_ctx.GetHandle());
		ImGui_ImplOpenGL3_Init("#version 460");

		ForwardEventHandler([this](SDL_Event* ev) {
			ImGui::SetCurrentContext(g_imgui_ctx);
			ImGui_ImplSDL2_ProcessEvent(ev);
		});

		glPixelStorei(GL_UNPACK_ROW_LENGTH, unpack_row_len);
		glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_align);
		glPixelStorei(GL_PACK_ROW_LENGTH, pack_row_len);
		glPixelStorei(GL_PACK_ALIGNMENT, pack_align);
	}
}

DisplayWindow::~DisplayWindow() {
	m_gl_ctx.SetCurrent(m_win);
	delete m_blit24_shader;
	glDeleteBuffers(1, &m_ssbo_buf);

	ImGui::SetCurrentContext(g_imgui_ctx);
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext(g_imgui_ctx);
}

void DisplayWindow::SetTextureWindow24(u32 start_x, u32 start_y, Rect window_size, Rect texture_size) {
	m_vert_buf->Clear();

	float w = float(window_size.w) / texture_size.w;
	float h = float(window_size.h) / texture_size.h;

	float xoff = float(start_x) / texture_size.w;
	float yoff = float(start_y) / texture_size.h;
	float endx = xoff + w;
	float endy = yoff + h;

	using psx::video::HostVertex2D;

	m_vert_buf->PushVertex(HostVertex2D{ xoff, yoff, 0.0, 0.0  });
	m_vert_buf->PushVertex(HostVertex2D{ xoff, yoff, 1.0, 1.0  });
	m_vert_buf->PushVertex(HostVertex2D{ xoff, yoff, 0.0, 1.0 });
	
	m_vert_buf->PushVertex(HostVertex2D{ xoff, yoff, 0.0, 0.0 });
	m_vert_buf->PushVertex(HostVertex2D{ xoff, yoff, 1.0, 0.0 });
	m_vert_buf->PushVertex(HostVertex2D{ xoff, yoff, 1.0, 1.0 });

	m_blit24_shader->BindProgram();
	m_blit24_shader->UpdateUniform("resolution_x", float(window_size.w));
	m_blit24_shader->UpdateUniform("resolution_y", float(window_size.h));
}

void DisplayWindow::Blit24(uint32_t texture_id) {
	m_gl_ctx.SetCurrent(m_win);
	m_gl_ctx.ScissorDisable();
	m_gl_ctx.BlendDisable();
	m_gl_ctx.BindFramebuffer(GL_FRAMEBUFFER, 0);

	if (!m_blit || !m_vert_buf)
		throw std::runtime_error("Window is not ready for blit ops");

	m_gl_ctx.SetViewport(0, 0, m_size.w, m_size.h);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	m_blit24_shader->BindProgram();
	m_vert_buf->Bind();

	m_gl_ctx.SetTextureSlot(GL_TEXTURE0);
	m_gl_ctx.BindTexture({ .type = GL_TEXTURE_2D, .handle = texture_id });

	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA,
		GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT,
		std::bit_cast<void*>(m_temp_buf.data()));
	
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 1024ULL * 512 * 2,
		std::bit_cast<void*>(m_temp_buf.data()));
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);


	////////////////////////////////////////////////

	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void DisplayWindow::DrawGui() {
	GLint unpack_row_len{}, pack_row_len{};
	GLint pack_align{}, unpack_align{};

	glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpack_row_len);
	glGetIntegerv(GL_PACK_ROW_LENGTH, &pack_row_len);
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_align);
	glGetIntegerv(GL_PACK_ALIGNMENT, &pack_align);

	ImGui::SetCurrentContext(g_imgui_ctx);
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	static bool s_was_gdb_connected{};
	bool is_gdb_connected = m_gdb_server->IsConnected();
	if (!s_was_gdb_connected && is_gdb_connected) {
		g_popups.push_back({ "GDB Server", "GDB attached" });
	}
	else if (s_was_gdb_connected && !is_gdb_connected) {
		g_popups.push_back({ "GDB Server", "GDB detached" });
	}
	s_was_gdb_connected = is_gdb_connected;
	
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Emulation")) {
			bool stop = m_sys->Stopped();
			if (m_gdb_server->IsConnected()) ImGui::BeginDisabled();
			ImGui::Checkbox("Stop", &stop);
			if (m_gdb_server->IsConnected()) ImGui::EndDisabled();
			m_sys->SetStopped(stop);
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && m_gdb_server->IsConnected()) {
				ImGui::BeginTooltip();
				ImGui::Text("Connected to GDB, cannot free-run");
				ImGui::EndTooltip();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	
	if (!g_is_popup_open && !g_popups.empty()) {
		ImGui::OpenPopup(g_popups[0].first.c_str());
		g_is_popup_open = true;
	}

	if (!g_popups.empty()) {
		auto viewport = ImGui::GetWindowViewport();
		ImGui::SetNextWindowPos(viewport->GetWorkCenter(), 0, ImVec2(.5f, .5f));
		ImGui::SetNextWindowSize(ImVec2(150.f, 80.f));
		if (ImGui::BeginPopupModal(g_popups[0].first.c_str(), nullptr, ImGuiWindowFlags_NoResize)) {
			auto window_width = ImGui::GetWindowSize().x;
			auto text_size = ImGui::CalcTextSize(g_popups[0].second.c_str());
			ImGui::SetCursorPosX((window_width - text_size.x) * 0.5);
			ImGui::Text(g_popups[0].second.c_str());
			
			if (ImGui::Button("OK")) {
				ImGui::CloseCurrentPopup();
				g_is_popup_open = false;
				g_popups.erase(g_popups.begin());
			}
			ImGui::EndPopup();
		}
	}
	
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	glPixelStorei(GL_UNPACK_ROW_LENGTH, unpack_row_len);
	glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_align);
	glPixelStorei(GL_PACK_ROW_LENGTH, pack_row_len);
	glPixelStorei(GL_PACK_ALIGNMENT, pack_align);
}

void DisplayWindow::SetSystem(psx::System* sys) {
	m_sys = sys;
	psx::kernel::Kernel& kernel = sys->GetKernel();
	kernel
		.InsertEnterHook(std::string("SystemError"),
			[sys](psx::u32 pc, psx::u32 id) {
				auto const& regs = sys->GetCPU().GetRegs();
				auto type = char(regs.a0);
				auto errcode = regs.a1;

				g_popups.push_back({ "SystemError", fmt::format("SystemError() called, type: {}, code: {:#x}",
					type, errcode) });
				sys->SetStopped(true);
			});
}

void DisplayWindow::SetGdbServer(std::shared_ptr<psx::gdbstub::Server> server) {
	m_gdb_server = server;
}
