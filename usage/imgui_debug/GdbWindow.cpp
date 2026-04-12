#include "DebugView.hpp"

#include <thirdparty/imgui/imgui.h>
#include <thirdparty/imgui/imgui_internal.h>
#include <thirdparty/imgui/misc/cpp/imgui_stdlib.h>
#include <thirdparty/imgui/backends/imgui_impl_sdl2.h>
#include <thirdparty/imgui/backends/imgui_impl_opengl3.h>

#include <psxemu/include/psxemu/Server.hpp>

void DebugView::GdbWindow() {
	if (!m_is_main_window_open.contains("GDB Server")) {
		m_is_main_window_open["GDB Server"] = true;
	}
	if (!m_is_main_window_open["GDB Server"]) {
		return;
	}
	ImGui::Begin("GDB Server", &m_is_main_window_open["GDB Server"]);

	if (!m_gdb_server) {
		ImGui::Text("GDB Server not created");
		ImGui::End();
		return;
	}

	auto is_thread_running = m_gdb_server->IsThreadRunning();
	auto is_connnected = m_gdb_server->IsConnected();
	
	auto port = m_gdb_server->GetPort();
	ImGui::SetNextItemWidth(100.f);
	ImGui::InputScalar("Port", ImGuiDataType_U16, (void*)&port, nullptr, nullptr, nullptr,
		ImGuiInputTextFlags_ReadOnly);
	if (!is_thread_running) {
		m_gdb_server->SetPort(port);
	}

	if (!is_thread_running) {
		auto start_thread = ImGui::Button("Open");
		if (start_thread) {
			m_gdb_server->StartThread();
		}
	}
	else {
		auto connected_temp = is_connnected;
		ImGui::Checkbox("Is connected?", &connected_temp);

		if (is_connnected) {
			auto address = m_gdb_server->GetClientAddress();
			auto ip_addres = address.host().toString();
			ImGui::Text("Client address:port : %s:%d", ip_addres.c_str(), address.port());
		}

		auto close_conn = ImGui::Button("Close");
		if (close_conn) {
			m_gdb_server->StopThread();
		}
	}

	ImGui::End();
}