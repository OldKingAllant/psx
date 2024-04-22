#include "DebugView.hpp"

#include <SDL2/SDL.h>

#include <thirdparty/imgui/imgui.h>
#include <thirdparty/imgui/backends/imgui_impl_sdl2.h>
#include <thirdparty/imgui/backends/imgui_impl_opengl3.h>

DebugView::DebugView(std::shared_ptr<psx::video::SdlWindow> win, psx::System* sys) 
	: m_win{ win }, m_psx{ sys }, m_gl_ctx{ nullptr } {
	m_gl_ctx = m_win->GetGlContext();

	ImGui::CreateContext();
	ImGui_ImplSDL2_InitForOpenGL((SDL_Window*)m_win->GetWindowHandle(), 
		m_gl_ctx);
	ImGui_ImplOpenGL3_Init("#version 430");
}

DebugView::~DebugView() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

void DebugView::Update() {
	m_win->Clear();



	m_win->Present();
}