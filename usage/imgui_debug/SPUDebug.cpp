#include "DebugView.hpp"

#include <thirdparty/imgui/imgui.h>

void DebugView::SpuWindow() {
	if (!m_is_main_window_open.contains("SPU Window")) {
		m_is_main_window_open["SPU Window"] = false;
	}
	if (!m_is_main_window_open["SPU Window"]) {
		return;
	}
	ImGui::Begin("SPU Window", &m_is_main_window_open["SPU Window"]);



	ImGui::End();
}