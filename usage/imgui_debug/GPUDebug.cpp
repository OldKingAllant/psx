#include "DebugView.hpp"

#include <thirdparty/imgui/imgui.h>
#include <thirdparty/imgui/imgui_internal.h>
#include <thirdparty/magic_enum/include/magic_enum/magic_enum.hpp>
#include <thirdparty/ImGuiFileDialog/ImGuiFileDialog.h>
#include <thirdparty/stb/stb_image_write.h>
#include <thirdparty/cereal/archives/portable_binary.hpp>

#include <psxemu/include/psxemu/System.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/GPUCommands.hpp>

#include <psxemu/renderer/GLRenderer.hpp>
#include <psxemu/renderer/Shader.hpp>

#include <GL/glew.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <filesystem>
#include <fstream>

#define _USE_MATH_DEFINES

static bool g_show_command_data_window{};
static uint32_t g_command_data_index{};
static uint64_t g_command_data_list_version{};

static bool g_show_command_texture_window{};
static uint32_t g_command_texture_index{};
static uint64_t g_command_texture_list_version{};

static bool g_show_recording_window{ false };
static bool g_show_vram_window{ false };
static bool g_show_dump_vram_window{ false };
static bool g_show_load_dump_window{ false };
static bool g_show_gpustat_window{ false };

static uint32_t g_selected_pixel_x{};
static uint32_t g_selected_pixel_y{};

void DebugView::GpuCommandWindow() {
	if (!g_show_recording_window) {
		return;
	}

	ImGui::Begin("GPU Commands", &g_show_recording_window);

	auto& gpu = m_psx->GetStatus().sysbus->GetGPU();

	bool record_commands{ gpu.GetRecordingCommands() };
	if (ImGui::Checkbox("Record commands", &record_commands)) {
		gpu.SetRecordingCommands(record_commands);
	}

	ImGui::SetNextItemWidth(100.f);
	ImGui::SameLine();
	ImGui::InputInt("Frames to record", (int*)&gpu.m_frames_to_record);

	ImGui::SameLine();
	if (ImGui::Button("Dump")) {
		ImGuiFileDialog::Instance()->OpenDialog("DumpCommands", "Dump GPU commands", ".gpudump");
	}
	if (ImGuiFileDialog::Instance()->Display("DumpCommands")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();

			std::ofstream out_file{};
			if (!std::filesystem::exists(filePathName)) {
				out_file.open(filePathName, std::ios::out);
				out_file.close();
			}

			bool show_error_dialog{ false };

			out_file.open(filePathName, std::ios::out | std::ios::binary);
			if (!out_file.is_open()) {
				show_error_dialog = true;
			}
			else {
				cereal::PortableBinaryOutputArchive out{ out_file };
				gpu.DumpRecordedCommands(out);
			}

			if (!show_error_dialog) {
				ImGuiFileDialog::Instance()->Close();
			}
			else {
				ImGui::OpenPopup("Dump error");
			}
		}
	}

	if (ImGui::BeginPopupModal("Dump error", nullptr, ImGuiWindowFlags_NoResize)) {
		ImGui::Text("Could not create dump file");
		if (ImGui::Button("OK")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	auto cursor_pos = ImGui::GetCursorPos();

	if (record_commands) {
		auto const& commands = gpu.GetRecordedCommands();

		static uint64_t s_last_list_version{};
		static std::vector<std::pair<std::string, uint32_t>> s_last_filtered_items{};
		static size_t s_last_list_len{};

		static uint32_t s_last_selected_pixel_x{};
		static uint32_t s_last_selected_pixel_y{};

		static ImGuiTextFilter s_filter{};
		static bool s_show_gp0{ true };
		static bool s_show_gp1{ true };
		static bool s_filter_by_area{ false };
		static bool s_filter_w{ true };
		static bool s_filter_r{ true };

		bool gp0_changed = ImGui::Checkbox("GP0", &s_show_gp0); ImGui::SameLine();
		bool gp1_changed = ImGui::Checkbox("GP1", &s_show_gp1); ImGui::SameLine();
		bool filter_area_changed = ImGui::Checkbox("Filter by area", &s_filter_by_area);
		if (ImGui::BeginItemTooltip()) {
			ImGui::Text("Filter by commands that access selected pixel");
			ImGui::EndTooltip();
		}
		bool filter_r_changed{ false };
		bool filter_w_changed{ false };
		if (s_filter_by_area) {
			ImGui::SameLine();
			filter_r_changed = ImGui::Checkbox("R", &s_filter_r); 
			if (ImGui::BeginItemTooltip()) {
				ImGui::Text("Filter by read (VRAM-VRAM blit, texture source, clut)");
				ImGui::EndTooltip();
			}
			ImGui::SameLine();
			filter_w_changed = ImGui::Checkbox("W", &s_filter_w);
			if (ImGui::BeginItemTooltip()) {
				ImGui::Text("Filter by write");
				ImGui::EndTooltip();
			}
		}
		bool filter_changed = s_filter.Draw("Filter commands (not by value)", 200.f) ||
			gp0_changed || gp1_changed || filter_area_changed;

		filter_changed = filter_changed || (
			s_filter_by_area &&
			(
			filter_r_changed ||
			filter_w_changed ||
			(s_last_selected_pixel_x != g_selected_pixel_x) ||
			(s_last_selected_pixel_y != g_selected_pixel_y)
			)
		);

		s_last_selected_pixel_x = g_selected_pixel_x;
		s_last_selected_pixel_y = g_selected_pixel_y;

		size_t start_filter_index{s_last_list_len};
		if (s_last_list_version != gpu.m_gp_commands_version || filter_changed) {
			s_last_list_version = gpu.m_gp_commands_version;
			start_filter_index = 0;
			s_last_list_len = 0;
			s_last_filtered_items.clear();
		}

		while (start_filter_index < commands.size()) {
			auto const& cmd = commands[start_filter_index];
			if ((!s_show_gp0 && cmd.reg == psx::CommandRegister::GP0) ||
				(!s_show_gp1 && cmd.reg == psx::CommandRegister::GP1)) {
				start_filter_index++;
				continue;
			}
			auto name = GetGpuCommandName(&cmd);
			if (s_filter.PassFilter(name.c_str())) {
				if (s_filter_by_area) {
					auto accesses_pixel = GetGpuCommandAccessesVramArea(&cmd,
						s_last_selected_pixel_x, s_last_selected_pixel_y, 1, 1);
					if ((s_filter_r && accesses_pixel.first) ||
						(s_filter_w && accesses_pixel.second)) {
						s_last_filtered_items.push_back({ name, (uint32_t)start_filter_index });
					}
				}
				else {
					s_last_filtered_items.push_back({ name, (uint32_t)start_filter_index });
				}
			}
			start_filter_index++;
		}
		s_last_list_len = start_filter_index;

		static uint32_t s_scroll_to{};
		ImGui::SetNextItemWidth(75.f);
		ImGui::InputScalar("##jump_to", ImGuiDataType_U32, (void*)&s_scroll_to); ImGui::SameLine();
		s_scroll_to = std::clamp<uint32_t>(s_scroll_to, 0, s_last_filtered_items.size());
		bool scrolled = ImGui::Button("Scroll");

		ImGui::Separator();
		ImGui::BeginChild(ImGui::GetID("##cmd_list"));

		if (s_last_filtered_items.size() == 0) {
			ImGui::Text("No results");
		}
		else {
			static float s_item_height{};
			if (scrolled && s_item_height > .0f) {
				ImGui::SetScrollY(s_scroll_to * s_item_height);
				s_scroll_to = 0;
			}

			ImGuiListClipper clipper{};
			clipper.Begin((int)s_last_filtered_items.size());
			
			size_t last_loaded_config_index{};

			while (clipper.Step()) {
				for (size_t cmd_index = clipper.DisplayStart; cmd_index < clipper.DisplayEnd; cmd_index++) {
					if (cmd_index >= s_last_filtered_items.size()) {
						break;
					}
					auto current_abs_index = s_last_filtered_items[cmd_index].second;
					while (last_loaded_config_index <= (size_t)current_abs_index) {
						GpuCommandLoadConfig(&commands[last_loaded_config_index]);
						last_loaded_config_index++;
					}
					auto const& cmd = commands[current_abs_index];
					auto has_details = !cmd.from_prev_frame && GetGpuCommandHasDetails(&cmd);
					auto is_open = ShowGpuCommandEntry(current_abs_index, &cmd, has_details);
					auto is_hovered = ImGui::IsItemHovered();

					if (is_open || is_hovered) {
						GpuCommandAppendVramAreas(&cmd, current_abs_index);
					}

					if (is_open && has_details) {
						ShowGpuCommandDetails(&cmd, current_abs_index, false);
						ImGui::Separator();
					}
					else if (is_hovered && has_details) {
						ImGui::BeginTooltip();
						ShowGpuCommandDetails(&cmd, current_abs_index, true);
						ImGui::EndTooltip();
					}
				}
				if (clipper.ItemsHeight > .0f) {
					s_item_height = clipper.ItemsHeight;
				}
			}
		}

		ImGui::EndChild();
	}

	ImGui::End();

	if (record_commands) {
		auto const& commands = gpu.GetRecordedCommands();

		if (g_show_command_data_window && (size_t)g_command_data_index < commands.size()) {
			ShowGpuCmdData(&commands[g_command_data_index], g_command_data_index);
		}
		else {
			g_show_command_data_window = false;
		}

		if (g_show_command_texture_window && (size_t)g_command_texture_index < commands.size()) {
			ShowGpuCmdTexture(&commands[g_command_texture_index], g_command_texture_index);
		}
		else {
			g_show_command_texture_window = false;
		}
	}
}

static void CallbackDisableBlending(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
	glDisable(GL_BLEND);
}

static void CallbackEnableBlending(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
	glEnable(GL_BLEND);
}

static void ColorPicker(uint8_t r, uint8_t g, uint8_t b, std::string const& id) {
	static int64_t s_picker_id{};
	float colorf[3] = {};
	colorf[0] = r / 255.f;
	colorf[1] = g / 255.f;
	colorf[2] = b / 255.f;
	ImGui::PushID(ImGui::GetID(id.c_str()));
	ImGui::ColorEdit3("", colorf,
		ImGuiColorEditFlags_NoInputs |
		ImGuiColorEditFlags_NoPicker);
	ImGui::PopID();
}

void DebugView::GpuVramWindow() {
	if (!g_show_vram_window) {
		return;
	}

	auto vram_handle = m_psx->GetStatus().sysbus->GetGPU().GetRenderer()->GetVram()
		.GetTextureHandle();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

	constexpr float MIN_SCALE = 1.f;
	constexpr float MAX_SCALE = 512.f;

	static float s_scale{ 1.f };
	static float s_vram_offset_x{ 0.f };
	static float s_vram_offset_y{ 0.f };
	static float s_mouse_delta_x{ .0f };
	static float s_mouse_delta_y{ .0f };

	static uint8_t s_picked_r{};
	static uint8_t s_picked_g{};
	static uint8_t s_picked_b{};
	static uint32_t s_picked_x{};
	static uint32_t s_picked_y{};

	static float s_highlight_color[3] = {};
	
	static float s_scale_step = .25f;

	static double s_blink_timer{};

	static bool s_magnifying_glass_on{};
	static float s_magnification{2.f};
	static int s_mag_size{100};

	auto old_item_spacing = ImGui::GetStyle().ItemSpacing;
	auto new_item_spacing = ImVec2(old_item_spacing.x, 50.f);

	const auto Y_SIZE = 512.f + ImGui::GetFrameHeightWithSpacing() + new_item_spacing.y;

	ImGui::SetNextWindowSizeConstraints(ImVec2(1024.f, Y_SIZE), ImVec2(1024.f, Y_SIZE));
	ImGui::SetNextWindowSize(ImVec2(1024.f, Y_SIZE));

	ImGui::Begin("VRAM", &g_show_vram_window, ImGuiWindowFlags_NoScrollbar);

	if (ImGui::Button("Reset view", ImVec2(.0f, 30.f))) {
		s_scale = 1.0f;
		s_vram_offset_x = .0f;
		s_vram_offset_y = .0f;
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.f);
	ImGui::InputFloat("Zoom step", &s_scale_step, .1f, .0f, "%.2f");
	ImGui::SameLine(.0f, 20.f);
	ImGui::Text("Picked color: "); ImGui::SameLine();
	ColorPicker(s_picked_r, s_picked_g, s_picked_b, "#picked_color");
	ImGui::SameLine(.0f, 20.f);
	ImGui::Text("X: %d, Y: %d", s_picked_x, s_picked_y);
	ImGui::SameLine(.0f, 20.f);
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 2.f);
	ImGui::SameLine(.0f, 20.f);
	ImGui::Checkbox("Magnifying glass", &s_magnifying_glass_on);

	if (s_magnifying_glass_on) {
		ImGui::SameLine(.0f, 20.f);
		ImGui::SetNextItemWidth(60.f);
		ImGui::InputFloat("Magnify", &s_magnification, .0f, .0f, "%.2f");
		ImGui::SameLine(.0f, 20.f);
		ImGui::SetNextItemWidth(80.f);
		ImGui::InputInt("Glass size (radius)", &s_mag_size, 10);
		s_mag_size = std::max(s_mag_size, 0);
	}

	auto offset = ImGui::GetCursorScreenPos();

	ImGui::GetWindowDrawList()->AddCallback(CallbackDisableBlending, nullptr);
	const auto begin = ImVec2(offset.x, offset.y + 10.f);
	const auto end = ImVec2(offset.x + 1024.f, offset.y + 512.f + 10.f);
	const auto bounding_box = ImRect(begin, end);
	ImGui::ItemSize(bounding_box);
	ImGui::ItemAdd(bounding_box, 0);
	ImGui::GetWindowDrawList()->AddImage((void*)(intptr_t)vram_handle, begin,
		end, ImVec2(std::floorf(s_vram_offset_x) / 1024.f, std::floorf(s_vram_offset_y) / 512.f),
		ImVec2(std::floorf(s_vram_offset_x) / 1024.f + 1.f / s_scale, std::floorf(s_vram_offset_y) / 512.f + 1.f / s_scale),
		ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 1.f)));
	ImGui::GetWindowDrawList()->AddCallback(CallbackEnableBlending, nullptr);

	ImGui::PopStyleVar();

	//Thanks to Avocado
	s_blink_timer += 0.004 * IM_PI;
	ImColor blink_color_normal = ImColor::HSV((float)s_blink_timer, 1.f, 1.f, 0.75f);
	ImColor blink_color = {};

	auto vram_offset_x_floored = std::floorf(s_vram_offset_x);
	auto vram_offset_y_floored = std::floorf(s_vram_offset_y);

	ImGui::GetWindowDrawList()->PushClipRect(begin, end, true);
	bool current_win_has_focus = ImGui::IsWindowFocused();

	for (auto const& area : m_highlited_areas) {
		blink_color = blink_color_normal;
		auto color = area.color;

		if (color.has_value()) {
			auto color_components = color.value();
			blink_color = ImColor::HSV(color_components[0], std::fmodf((float)s_blink_timer, 100.f) / 100.f,
				1.f, .75f);
		}

		if (area.num_vertices == 4) {
			//0 and 2
			float x_l{ (float)area.x[0] };
			float x_r{ (float)area.x[2] };
			float y_t{ (float)area.y[0] };
			float y_b{ (float)area.y[2] };

			x_l -= vram_offset_x_floored;
			x_r -= vram_offset_x_floored;
			y_t -= vram_offset_y_floored;
			y_b -= vram_offset_y_floored;

			x_l *= s_scale;
			x_r *= s_scale;
			y_t *= s_scale;
			y_b *= s_scale;

			x_l += begin.x;
			x_r += begin.x;

			y_t += begin.y;
			y_b += begin.y;

			x_l = std::max(x_l, begin.x);
			x_r = std::max(x_r, begin.x);

			if (x_l == x_r) {
				continue;
			}

			y_t = std::max(y_t, begin.y);
			y_b = std::max(y_b, begin.y);

			if (y_t == y_b) {
				continue;
			}

			auto prim_begin = ImVec2(x_l, y_t);
			auto prim_end = ImVec2(x_r, y_b);

			if (area.filled) {
				ImGui::GetWindowDrawList()->AddRectFilled(prim_begin, prim_end,
					blink_color);
			}
			else {
				ImGui::GetWindowDrawList()->AddRect(prim_begin, prim_end,
					blink_color);
			}

			if (current_win_has_focus && area.filled && ImGui::IsMouseHoveringRect(prim_begin, prim_end) &&
				!s_magnifying_glass_on) {
				ImGui::BeginTooltip();
				ImGui::Text("Command %d", area.cmd_index);
				ImGui::Text("Description: %s", area.name.c_str());
				ImGui::EndTooltip();
			}
		}
		else if (area.num_vertices == 3) {
			float x_0{ (float)area.x[0] };
			float x_1{ (float)area.x[1] };
			float x_2{ (float)area.x[2] };
			float y_0{ (float)area.y[0] };
			float y_1{ (float)area.y[1] };
			float y_2{ (float)area.y[2] };

			x_0 -= vram_offset_x_floored;
			x_1 -= vram_offset_x_floored;
			x_2 -= vram_offset_x_floored;

			y_0 -= vram_offset_y_floored;
			y_1 -= vram_offset_y_floored;
			y_2 -= vram_offset_y_floored;

			x_0 *= s_scale;
			x_1 *= s_scale;
			x_2 *= s_scale;

			y_0 *= s_scale;
			y_1 *= s_scale;
			y_2 *= s_scale;

			x_0 += begin.x;
			x_1 += begin.x;
			x_2 += begin.x;

			y_0 += begin.y;
			y_1 += begin.y;
			y_2 += begin.y;

			auto v0 = ImVec2(x_0, y_0);
			auto v1 = ImVec2(x_1, y_1);
			auto v2 = ImVec2(x_2, y_2);

			if (area.filled) {
				ImGui::GetWindowDrawList()->AddTriangleFilled(v0, v1, v2, 
					blink_color);
			}
			else {
				ImGui::GetWindowDrawList()->AddTriangle(v0, v1, v2,
					blink_color);
			}

			if (current_win_has_focus && area.filled && ImTriangleContainsPoint(v0, v1, v2, ImGui::GetMousePos()) &&
				!s_magnifying_glass_on) {
				ImGui::BeginTooltip();
				ImGui::Text("Command %d", area.cmd_index);
				ImGui::Text("Description: %s", area.name.c_str());
				ImGui::EndTooltip();
			}
		} 
		else if (area.num_vertices == 2) {
			float x0{ (float)area.x[0] };
			float x1{ (float)area.x[1] };
			float y0{ (float)area.y[0] };
			float y1{ (float)area.y[1] };

			x0 -= vram_offset_x_floored;
			x1 -= vram_offset_x_floored;
			y0 -= vram_offset_y_floored;
			y1 -= vram_offset_y_floored;

			x0 *= s_scale;
			x1 *= s_scale;
			y0 *= s_scale;
			y1 *= s_scale;

			x0 += begin.x;
			x1 += begin.x;

			y0 += begin.y;
			y1 += begin.y;

			x0 = std::max(x0, begin.x);
			x1 = std::max(x1, begin.x);

			y0 = std::max(y0, begin.y);
			y1 = std::max(y1, begin.y);

			auto prim_begin = ImVec2(x0, y0);
			auto prim_end = ImVec2(x1, y1);

			ImGui::GetWindowDrawList()->AddLine(prim_begin, prim_end,
				blink_color, s_scale);
		}
	}

	ImGui::GetWindowDrawList()->PopClipRect();

	if (ImGui::IsItemHovered()) {
		auto mouse_x = ImGui::GetMousePos().x;
		auto mouse_y = ImGui::GetMousePos().y;

		auto pos_x = vram_offset_x_floored + (mouse_x - begin.x) / s_scale;
		auto pos_y = vram_offset_y_floored + (mouse_y - begin.y) / s_scale;

		if (s_magnifying_glass_on) {
			ImGui::GetWindowDrawList()->AddCircleFilled(
				ImVec2(mouse_x, mouse_y),
				(float)s_mag_size + 1.f, IM_COL32(255, 255, 255, 255)
			);

			ImGui::GetWindowDrawList()->AddCallback(CallbackDisableBlending, nullptr);
			ImGui::GetWindowDrawList()->AddImageRounded((ImTextureID)(intptr_t)vram_handle,
				ImVec2(mouse_x - s_mag_size, mouse_y - s_mag_size),
				ImVec2(mouse_x + s_mag_size, mouse_y + s_mag_size),
				ImVec2(
					(s_vram_offset_x + ((mouse_x - begin.x) / s_scale - s_mag_size / (s_magnification * s_scale))) / 1024.f,
					(s_vram_offset_y + ((mouse_y - begin.y) / s_scale - s_mag_size / (s_magnification * s_scale))) / 512.f
					),
				ImVec2(
					(s_vram_offset_x + ((mouse_x - begin.x) / s_scale + s_mag_size / (s_magnification * s_scale))) / 1024.f,
					(s_vram_offset_y + ((mouse_y - begin.y) / s_scale + s_mag_size / (s_magnification * s_scale))) / 512.f
				), IM_COL32(255, 255, 255, 1), (float)s_mag_size
			);
			ImGui::GetWindowDrawList()->AddCallback(CallbackEnableBlending, nullptr);
		}

		if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
			ImGui::GetCurrentWindow()->Flags &= ~ImGuiWindowFlags_NoMove;
		}
		else {
			ImGui::GetCurrentWindow()->Flags |= ImGuiWindowFlags_NoMove;
		}

		if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !s_magnifying_glass_on) {
			bool dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left, .01f);
			if (bounding_box.Contains(ImGui::GetIO().MouseClickedPos[0]) && dragging) {
				auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);

				auto distance_x = std::abs(delta.x - s_mouse_delta_x);
				auto distance_y = std::abs(delta.y - s_mouse_delta_y);

				distance_x /= s_scale;
				distance_y /= s_scale;

				if (s_mouse_delta_x > delta.x) {
					distance_x *= -1;
				}

				if (s_mouse_delta_y > delta.y) {
					distance_y *= -1;
				}

				s_mouse_delta_x = delta.x;
				s_mouse_delta_y = delta.y;
				
				s_vram_offset_x = s_vram_offset_x - distance_x;
				s_vram_offset_y = s_vram_offset_y - distance_y;
			}
			else if(dragging) {
				ImGui::GetCurrentWindow()->Flags &= ~ImGuiWindowFlags_NoMove;
				ImGui::StartMouseMovingWindow(ImGui::GetCurrentWindow());
				ImGui::GetCurrentWindow()->Flags |= ImGuiWindowFlags_NoMove;
				s_mouse_delta_x = s_mouse_delta_y = .0f;
			}
		}
		else {
			s_mouse_delta_x = s_mouse_delta_y = .0f;
		}

		if (ImGui::GetIO().MouseWheel != 0 && !s_magnifying_glass_on) {
			s_scale += ImGui::GetIO().MouseWheel * s_scale_step;
			s_scale = std::clamp(s_scale, MIN_SCALE, MAX_SCALE);

			if (s_scale == 1.0) {
				s_vram_offset_x = .0f;
				s_vram_offset_y = .0f;
			}
			else {
				s_vram_offset_x = pos_x - (512.f / s_scale);
				s_vram_offset_y = pos_y - (256.f / s_scale);
			}
			
		}
		
		if (pos_x >= .0f && pos_y >= .0f && pos_x < 1024.f && pos_y < 512.f) {
			ImGui::BeginTooltip();
			uint32_t pixel{};
			glGetTextureSubImage(vram_handle, 0, 
				(GLint)(pos_x), 
				(GLint)(pos_y),
				0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT,
				4, &pixel);
			ImGui::Text("Pixel X: %d, Y: %d", uint32_t(pos_x),
				uint32_t(pos_y));
			uint8_t r = uint8_t(((pixel & 0x1F) / 31.f) * 255.f);
			uint8_t g = uint8_t((((pixel >> 5) & 0x1F) / 31.f) * 255.f);
			uint8_t b = uint8_t((((pixel >> 10) & 0x1F) / 31.f) * 255.f);
			uint8_t mask = (pixel >> 15) & 1;
			ImGui::Text("Mask/transparent: %d", mask);
			ImGui::Text("Color: "); ImGui::SameLine();
			ColorPicker(r, g, b, "#hovered");
			ImGui::Text("R: %d, G: %d, B: %d", r, g, b);
			if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
				s_picked_r = r;
				s_picked_g = g;
				s_picked_b = b;
				s_picked_x = uint32_t(pos_x);
				s_picked_y = uint32_t(pos_y);

				g_selected_pixel_x = s_picked_x;
				g_selected_pixel_y = s_picked_y;
			}
			ImGui::EndTooltip();
		}
	}
	else {
		s_mouse_delta_x = s_mouse_delta_y = .0f;
	}

	m_highlited_areas.clear();

	ImGui::End();
}

static void DecodeStoreTexture(uint32_t vram_handle,
	uint32_t x, uint32_t y,
	uint32_t w, uint32_t h,
	uint32_t color_depth,
	uint32_t clut_x, uint32_t clut_y,
	std::string const& fpath);

void DebugView::GpuDumpVramWindow() {
	if (!g_show_dump_vram_window) {
		return;
	}

	static uint32_t s_x_off{};
	static uint32_t s_y_off{};
	static uint32_t s_w{};
	static uint32_t s_h{};
	static uint32_t s_clut_x{};
	static uint32_t s_clut_y{};
	static int32_t s_bpp{};

	auto vram_handle = m_psx->GetStatus().sysbus->GetGPU().GetRenderer()->GetVram().GetTextureHandle();

	ImGui::Begin("Dump Vram", &g_show_dump_vram_window);

	ImGui::InputScalar("Position X", ImGuiDataType_U32, (void*)&s_x_off);
	ImGui::InputScalar("Position Y", ImGuiDataType_U32, (void*)&s_y_off);
	ImGui::InputScalar("W         ", ImGuiDataType_U32, (void*)&s_w);
	ImGui::InputScalar("H         ", ImGuiDataType_U32, (void*)&s_h);
	ImGui::InputScalar("Clut X    ", ImGuiDataType_U32, (void*)&s_clut_x);
	ImGui::InputScalar("Clut Y    ", ImGuiDataType_U32, (void*)&s_clut_y);

	constexpr const char* BPP_STRINGS[] = { "4BPP", "8BPP", "15BPP" };
	ImGui::Combo("Color bpp", &s_bpp, BPP_STRINGS, IM_ARRAYSIZE(BPP_STRINGS));

	s_x_off = std::clamp<uint32_t>(s_x_off, 0, 1023);
	s_y_off = std::clamp<uint32_t>(s_y_off, 0, 511);
	s_w = std::clamp<uint32_t>(s_w, 0, 1024);
	s_h = std::clamp<uint32_t>(s_h, 0, 512);
	s_clut_x = std::clamp<uint32_t>(s_clut_x, 0, 1023);
	s_clut_y = std::clamp<uint32_t>(s_clut_y, 0, 511);
	s_bpp = std::clamp<int32_t>(s_bpp, 0, 2);

	//constexpr uint32_t PIXELS_PER_ENTRY[] = { 4, 2, 1, 1 };
	//const auto current_pixels_per_entry = PIXELS_PER_ENTRY[(uint32_t)s_bpp];

	if (ImGui::Button("Dump")) {
		ImGuiFileDialog::Instance()->OpenDialog("DumpVram", "Dump rectangle",
			".bmp,.png");
	}
	if (ImGuiFileDialog::Instance()->Display("DumpVram")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
			std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();

			DecodeStoreTexture(vram_handle,
				s_x_off,
				s_y_off,
				s_w,
				s_h,
				(uint32_t)s_bpp, s_clut_x, s_clut_y,
				filePathName);
		}

		ImGuiFileDialog::Instance()->Close();
	}

	ImGui::End();
}

static size_t GpuCommandGetGPSize(psx::GPUCommand const* cmd, std::vector<psx::Gpu::RegisterCommand> const& gps) {
	if (cmd->from_prev_frame) {
		return 0;
	}
	auto curr_index = cmd->start_index;
	auto reg_index = gps[curr_index].reg_index;
	size_t item_count{ 1 };
	while (curr_index < gps.size()) {
		curr_index++;
		auto const& stored_cmd = gps[curr_index];
		if (stored_cmd.reg_index == reg_index && !stored_cmd.end_marker) {
			item_count++;
		}
		else if (stored_cmd.end_marker &&
			(stored_cmd.reg_index == reg_index || stored_cmd.end_reg_independent)) {
			break;
		}
	}
	return item_count;
}

static size_t GpuCommandGetLastGPIndex(psx::GPUCommand const* cmd, std::vector<psx::Gpu::RegisterCommand> const& gps) {
	if (cmd->from_prev_frame) {
		return UINT64_MAX;
	}
	auto curr_index = cmd->start_index;
	auto reg_index = gps[curr_index].reg_index;
	while (curr_index < gps.size()) {
		curr_index++;
		auto const& stored_cmd = gps[curr_index];
		if (stored_cmd.end_marker &&
			(stored_cmd.reg_index == reg_index || stored_cmd.end_reg_independent)) {
			break;
		}
	}
	return curr_index;
}

void GpuCommandRemoveGPData(size_t index, 
	std::vector<psx::GPUCommand>& cmds, 
	std::vector<psx::Gpu::RegisterCommand>& gps) {
	auto& cmd = cmds[index];
	if (cmd.from_prev_frame) {
		return;
	}

	//Remove interleaved items from GP list
	//and shift references of the following
	//items
	//Item count to remove (include end marker)
	auto item_count = GpuCommandGetGPSize(&cmd, gps) + 1;
	//Index at which we are removing data
	auto curr_index = cmd.start_index;
	//GP0 or GP1
	auto reg_index = gps[curr_index].reg_index;
	//Removed since start of last index + all others accumulated
	auto removed_counts = std::vector<size_t>{};
	//Indices at which we started removing data
	auto removed_indices = std::vector<size_t>{};
	removed_counts.push_back(0); //Start from zero removed items
	removed_indices.push_back(cmd.start_index); //at the current index
	while (item_count && curr_index < gps.size()) {
		auto const& stored_cmd = gps[curr_index];
		if (stored_cmd.reg_index == reg_index) { //Same register
			gps.erase(gps.begin() + curr_index); //Remove entry
			item_count--;
			removed_counts.back()++; //Accumulate removed items up to now
		}
		else {
			while (curr_index < gps.size() && gps[curr_index].reg_index != reg_index) {
				curr_index++; //Skip all items up to a new item with the same register
			}
			if (curr_index == gps.size()) {
				break;
			}
			removed_counts.push_back(removed_counts.back()); //Add accumulated item count
			removed_indices.push_back(cmd.start_index + removed_counts.back()); //Create new entry for index shifting
		}
	}

	size_t curr_cmd_index = index + 1;
	size_t curr_removed_index_reference = {};

	while (curr_cmd_index < cmds.size()) {
		size_t shift_count = {};
		if (curr_removed_index_reference < (removed_indices.size() - 1) &&
			removed_indices[curr_removed_index_reference + 1] < cmds[curr_cmd_index].start_index) {
			curr_removed_index_reference++;
		}
		shift_count = removed_counts[curr_removed_index_reference];
		if (!cmds[curr_cmd_index].from_prev_frame) {
			cmds[curr_cmd_index].start_index -= shift_count;
		}
		curr_cmd_index++;
	}
}

void DebugView::GpuLoadDumpWindow() {
	if (!g_show_load_dump_window) {
		return;
	}

	if (!ImGui::Begin("GPU command replay", &g_show_load_dump_window)) {
		ImGui::End();
		return;
	}

	auto& gpu = m_psx->GetStatus().sysbus->GetGPU();

	static std::vector<psx::GPUCommand>           s_hle_commands{};
	static std::vector<psx::Gpu::RegisterCommand> s_gp_commands{};
	static bool s_single_step{ true };
	static bool s_capture_renderdoc{ false };
	static bool s_force_flush{ true };

	ImGui::Text("Load GPU dump: ");
	ImGui::SameLine();
	if (ImGui::Button("Select dump")) {
		ImGuiFileDialog::Instance()->OpenDialog("LoadDump", "Load GPU dump", ".gpudump");
	}
	if (ImGuiFileDialog::Instance()->Display("LoadDump")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();

			std::ifstream in_file{};

			bool show_error_dialog{ false };

			if (!std::filesystem::exists(filePathName)) {
				show_error_dialog = true;
			}
			else {
				in_file.open(filePathName, std::ios::binary);
				if (!in_file.is_open()) {
					show_error_dialog = true;
				}
				else {
					cereal::PortableBinaryInputArchive in{ in_file };
					auto curr_ctx = psx::video::GetCurrentGLContext();
					//Hope it is ok to bind that OpenGL context to this window
					gpu.GetRenderer()->GetContext()->SetCurrent(m_win->GetWindowHandle());
					show_error_dialog = !gpu.LoadRecordedCommands(in, s_hle_commands, s_gp_commands);
					curr_ctx->SetCurrent(m_win->GetWindowHandle());
				}
			}

			if (!show_error_dialog) {
				ImGuiFileDialog::Instance()->Close();
			}
			else {
				ImGui::OpenPopup("Load error");
			}
		}
		else {
			ImGuiFileDialog::Instance()->Close();
		}
	}

	if (ImGui::BeginPopupModal("Load error", nullptr, ImGuiWindowFlags_NoResize)) {
		ImGui::Text("Could not load dump");
		if (ImGui::Button("OK")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::Button("Clear")) {
		s_hle_commands.clear();
		s_gp_commands.clear();
	}

	ImGui::SameLine();
	ImGui::Checkbox("Single step", &s_single_step);
	ImGui::SameLine();
	ImGui::Checkbox("Renderdoc", &s_capture_renderdoc);
	ImGui::SameLine();
	ImGui::Checkbox("Flush cmds", &s_force_flush);
	if (ImGui::BeginItemTooltip()) {
		ImGui::Text("After stepping:");
		ImGui::BulletText("Force flush commands");
		ImGui::BulletText("Sync front/back textures");
		ImGui::EndTooltip();
	}

	static uint32_t s_cmd_count{ 0 };
	ImGui::SetNextItemWidth(150.f);
	ImGui::InputScalar("Command count", ImGuiDataType_U32, (void*)&s_cmd_count);
	s_cmd_count = std::clamp<uint32_t>(s_cmd_count, 0, (uint32_t)s_hle_commands.size());
	if (ImGui::BeginItemTooltip()) {
		ImGui::Text("When single stepping:");
		ImGui::BulletText("How many commands to execute on step");
		ImGui::BulletText("Leave zero if you want single stepping");
		ImGui::EndTooltip();
	}

	if (!s_hle_commands.empty() && ImGui::Button("Replay")) {
		if (!s_hle_commands[0].from_prev_frame &&
			s_hle_commands[0].start_index != 0) {
			psx::LOG_ERROR("DEBUG", "[DEBUG] Command replay: start index in gp list != 0");
		}
		size_t cmd_count = s_single_step ? (s_cmd_count ? s_cmd_count : 1) : s_hle_commands.size();
		size_t gp_last_index = 0;

		size_t remove_count = cmd_count;
		size_t curr_cmd_index = 0;

		auto renderdoc = gpu.GetRenderer()->GetRenderDoc();
		if (renderdoc && s_capture_renderdoc) {
			renderdoc->PrepareCapture();
			renderdoc->StartCapture();
		}

		auto curr_ctx = psx::video::GetCurrentGLContext();
		gpu.GetRenderer()->GetContext()->SetCurrent(m_win->GetWindowHandle());

		while (cmd_count && curr_cmd_index < s_hle_commands.size()) {
			auto& curr_cmd = s_hle_commands[curr_cmd_index];
			if (curr_cmd.from_prev_frame) {
				gpu.LoadStateConfiguration(curr_cmd);
			}
			else {
				gp_last_index = GpuCommandGetLastGPIndex(&s_hle_commands[curr_cmd_index], s_gp_commands);
				auto curr_gp_index = curr_cmd.start_index;
				auto& start_gp_entry = s_gp_commands[curr_gp_index];
				while (curr_gp_index != gp_last_index) {
					auto& gp_entry = s_gp_commands[curr_gp_index];
					if (gp_entry.reg_index != start_gp_entry.reg_index &&
						(gp_entry.end_marker || gp_entry.end_reg_independent)) {
						remove_count++;
					}
					if (!gp_entry.end_marker && !gp_entry.end_reg_independent) {
						if (gp_entry.reg_index == 0) {
							gpu.WriteGP0(gp_entry.value);
						}
						else if (gp_entry.reg_index == 1) {
							gpu.WriteGP1(gp_entry.value);
						}
					}
					curr_gp_index++;
				}
			}
			cmd_count--;

			if (s_hle_commands[curr_cmd_index].reg == psx::CommandRegister::GP0 &&
				s_hle_commands[curr_cmd_index].gp0.type == psx::GP0CommandType::LINE &&
				s_hle_commands[curr_cmd_index].gp0.line.is_polyline()) {
				while (curr_cmd_index < s_hle_commands.size() &&
					(s_hle_commands[curr_cmd_index].reg != psx::CommandRegister::GP0 ||
					s_hle_commands[curr_cmd_index].gp0.type != psx::GP0CommandType::POLYLINE_END)) {
					curr_cmd_index++;
					remove_count++;
				}
			}

			curr_cmd_index++;
		}

		//Force sync between commands
		if (s_force_flush) {
			gpu.GetRenderer()->FlushCommands();
			gpu.GetRenderer()->SyncTextures();
		}

		curr_ctx->SetCurrent(m_win->GetWindowHandle());

		if (renderdoc && s_capture_renderdoc) {
			renderdoc->EndCapture();
		}

		remove_count = std::min(s_hle_commands.size(), remove_count);
		if (remove_count == s_hle_commands.size()) {
			s_gp_commands.clear();
		}
		else {
			for (size_t remove_index = 0; remove_index < remove_count; remove_index++) {
				GpuCommandRemoveGPData(remove_index, s_hle_commands, s_gp_commands);
				if (s_hle_commands[remove_index].reg == psx::CommandRegister::GP0 &&
					s_hle_commands[remove_index].gp0.type == psx::GP0CommandType::LINE &&
					s_hle_commands[remove_index].gp0.line.is_polyline()) {
					while (remove_index < remove_count &&
						(s_hle_commands[remove_index].gp0.type != psx::GP0CommandType::POLYLINE_END ||
							s_hle_commands[remove_index].reg != psx::CommandRegister::GP0)) {
						remove_index++;
					}
				}
			}
		}
		s_hle_commands.erase(s_hle_commands.begin(), s_hle_commands.begin() + remove_count);
		s_cmd_count = 0;
	}

	ImGui::Separator();
	ImGui::BeginChild("#dump_cmd_list");

	if (s_hle_commands.empty()) {
		ImGui::Text("No commands");
	}
	else {
		ImGuiListClipper clipper{};
		clipper.Begin((int)s_hle_commands.size());

		std::vector<size_t> indexes_to_remove{};

		while (clipper.Step()) {
			for (size_t curr_index = clipper.DisplayStart; curr_index < clipper.DisplayEnd;
				curr_index++) {
				auto id_string = fmt::format("#loaded{}", curr_index);

				auto const& cmd = s_hle_commands[curr_index];

				bool is_gp0 = cmd.reg == psx::CommandRegister::GP0;
				bool is_middle_polyline = is_gp0 && (cmd.gp0.type == psx::GP0CommandType::LINE &&
					cmd.gp0.line.is_polyline() &&
					!cmd.polyline_begin);
				bool is_end_polyline = is_gp0 && cmd.gp0.type == psx::GP0CommandType::POLYLINE_END;

				
				ImGui::PushID(id_string.c_str());
				if ((!is_middle_polyline && !is_end_polyline)) {
					if (ImGui::Button("R")) {
						indexes_to_remove.push_back(curr_index);
					}
				}
				else {
					ImGui::Dummy(ImVec2(ImGui::CalcTextSize("R  ").x, .0f));
				}
				ImGui::PopID();
				ImGui::SameLine();
				
				auto name = GetGpuCommandName(&cmd);
				ImGui::Text("%d (0x%08x) %s", curr_index, cmd.value, name.c_str());
			}
		}

		for (auto index : indexes_to_remove) {
			auto const& cmd = s_hle_commands[index];

			bool is_gp0 = cmd.reg == psx::CommandRegister::GP0;
			bool is_polyline = is_gp0 && 
				cmd.gp0.type == psx::GP0CommandType::LINE &&
				cmd.gp0.line.is_polyline();
			bool is_middle_polyline = (is_polyline && !cmd.polyline_begin);
			bool is_end_polyline = is_gp0 && cmd.gp0.type == psx::GP0CommandType::POLYLINE_END;

			if (is_middle_polyline || is_end_polyline) {
				psx::LOG_ERROR("DEBUG", "[DEBUG] Replay: Attemtping to delete index in the middle of a polyline");
				continue;
			}

			GpuCommandRemoveGPData(index, s_hle_commands, s_gp_commands);
			if (is_gp0 && is_polyline) {
				while (true) {
					auto cmd = s_hle_commands[index];
					s_hle_commands.erase(s_hle_commands.begin() + index);
					if (cmd.reg == psx::CommandRegister::GP0 &&
						cmd.gp0.type == psx::GP0CommandType::POLYLINE_END) {
						break;
					}
				}
			}
			else {
				s_hle_commands.erase(s_hle_commands.begin() + index);
			}
		}
	}

	ImGui::EndChild();

	ImGui::End();
}

void DebugView::GpuMainWindow() {
	/*
	static bool g_show_recording_window{ false };
	static bool g_show_vram_window{ false };
	static bool g_show_dump_vram_window{ false };
	static bool g_show_load_dump_window{ false };
	*/
	if (!m_is_main_window_open.contains("GPU Main")) {
		m_is_main_window_open["GPU Main"] = true;
	}
	if (!m_is_main_window_open["GPU Main"]) {
		return;
	}
	ImGui::Begin("GPU Main", &m_is_main_window_open["GPU Main"]);

	ImGui::Text("Show/hide GPU debug windows");
	ImGui::Spacing();
	ImGui::Checkbox("Show command recording window", &g_show_recording_window);
	ImGui::Checkbox("Show VRAM window",      &g_show_vram_window);
	ImGui::Checkbox("Show dump vram window", &g_show_dump_vram_window);
	ImGui::Checkbox("Show load dump window", &g_show_load_dump_window);
	ImGui::Checkbox("Show stat window", &g_show_gpustat_window);

	ImGui::End();
}

bool DebugView::ShowGpuCommandEntry(size_t index, psx::GPUCommand const* cmd, bool has_details) {
	ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	if (cmd->reg == psx::CommandRegister::GP1) {
		ImVec4 color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
	}

	ImGui::PushStyleColor(ImGuiCol_Text, color);

	std::string line{};
	if (cmd->from_prev_frame) {
		line = fmt::format("{:5d} PREVIOUS FRAME GP{} ({:#010x}) {}",
			index, cmd->reg == psx::CommandRegister::GP0 ? "0" : "1",
			cmd->value, GetGpuCommandName(cmd));
	}
	else {
		line = fmt::format("{:5d} FRAME {} GP{} ({:#010x}) {}",
			index, cmd->frame_of_recording,
			cmd->reg == psx::CommandRegister::GP0 ? "0" : "1",
			cmd->value, GetGpuCommandName(cmd));
	}

	bool open{ false };
	if (has_details) {
		open = ImGui::TreeNodeEx((void*)(intptr_t)index,
			ImGuiTreeNodeFlags_SpanFullWidth |
			ImGuiTreeNodeFlags_NoTreePushOnOpen |
			ImGuiTreeNodeFlags_NoAutoOpenOnLog,
			"%s", line.c_str());
	}
	else {
		ImGui::Selectable(line.c_str());
	}

	ImGui::PopStyleColor();

	return open;
}

static void ColorPicker(psx::ColorAttribute color) {
	float colorf[3] = {};
	colorf[0] = color.r() / 31.f;
	colorf[1] = color.g() / 31.f;
	colorf[2] = color.b() / 31.f;
	ImGui::ColorEdit3("##colorpicker", colorf,
		ImGuiColorEditFlags_NoInputs |
		ImGuiColorEditFlags_NoPicker);
}

void DebugView::ShowGpuCommandDetails(psx::GPUCommand const* cmd, size_t cmd_index, bool is_popup) {
	if (cmd->reg == psx::CommandRegister::GP1 && cmd->gp1.type == psx::GP1CommandType::DISPLAY_MODE) {
		auto disp_mode = cmd->gp1.disp_mode;
		uint32_t hoz_resolution{};
		if (disp_mode.horizontal_resolution_2) {
			hoz_resolution = 368;
		}
		else {
			constexpr uint32_t RESOLUTIONS[] = {256, 320, 512, 640};
			hoz_resolution = RESOLUTIONS[disp_mode.horizontal_resolution_1];
		}

		uint32_t v_resolution{};
		if (disp_mode.vertical_interlace && disp_mode.vertical_resolution) {
			v_resolution = 480;
		}
		else {
			v_resolution = 240;
		}
		ImGui::Text("Horizontal resolution: %d", hoz_resolution);
		ImGui::Text("Vertical resolution  : %d", v_resolution);
		ImGui::Text("Video mode           : %s", disp_mode.video_mode ? "PAL" : "NTSC");
		ImGui::Text("Color depth          : %s", disp_mode.color_depth ? "24BPP" : "15BPP");
		ImGui::Text("Interlace            : %s", disp_mode.vertical_interlace ? "Yes" : "No");
		ImGui::Text("Flip screen          : %s", disp_mode.flip_screen_x_axis ? "Yes" : "No");
	}
	else if (cmd->reg == psx::CommandRegister::GP0 && cmd->gp0.type == psx::GP0CommandType::MISC &&
		cmd->gp0.misc.type == psx::MiscCommandType::QUICK_FILL) {
		ImGui::Text("X: %d, Y: %d", cmd->params.quick_fill.x, cmd->params.quick_fill.y);
		ImGui::Text("W: %d, H: %d", cmd->params.quick_fill.w, cmd->params.quick_fill.h);
		ImGui::Text("Color: "); ImGui::SameLine();
		ColorPicker(uint8_t(cmd->gp0.misc.quick_fill.r),
			uint8_t(cmd->gp0.misc.quick_fill.g),
			uint8_t(cmd->gp0.misc.quick_fill.b), fmt::format("#cmd{}", cmd_index));
	}
	else if (cmd->reg == psx::CommandRegister::GP0 && cmd->gp0.type == psx::GP0CommandType::ENV &&
		cmd->gp0.env.type == psx::EnvCommandType::TEXTURE_PAGE) {
		auto texpage = cmd->gp0.env.texpage;
		auto ybase = texpage.y_base_1 | (texpage.y_base_2 << 1);
		ImGui::Text("Texture page X base: %d", texpage.x_base * 64);
		ImGui::Text("Texture page Y base: %d", ybase * 256);
		constexpr const char* SEMI_TRANSPARENCIES[] = { "B/2+F/2", "B+F", "B-F", "B+F/4" };
		ImGui::Text("Semi-transparency  : %s", SEMI_TRANSPARENCIES[texpage.semi_transparency]);
		constexpr const char* TEXPAGE_COLORS[] = { "4BPP", "8BPP", "15BPP", "RESERVED" };
		ImGui::Text("Texpage bit depth  : %s", TEXPAGE_COLORS[texpage.texpage_colors]);
		ImGui::Text("Dither             : %s", texpage.enable_dither ? "On" : "Off");
		ImGui::Text("Draw to display    : %s", texpage.draw_to_display ? "Yes" : "No");
		ImGui::Text("Texture X-Flip     : %s", texpage.texture_x_flip ? "Yes" : "No");
		ImGui::Text("Texture Y-Flip     : %s", texpage.texture_y_flip ? "Yes" : "No");
	}
	else if (cmd->reg == psx::CommandRegister::GP0 && cmd->gp0.type == psx::GP0CommandType::POLYGON) {
		ShowGpuCmdPolygon(cmd, cmd_index);
	}
	else if (cmd->reg == psx::CommandRegister::GP0 && cmd->gp0.type == psx::GP0CommandType::RECTANGLE) {
		ShowGpuCmdRectangle(cmd, cmd_index);
	}
	else if (cmd->reg == psx::CommandRegister::GP0 && cmd->gp0.type == psx::GP0CommandType::LINE) {
		ShowGpuCmdLine(cmd, cmd_index);
	}
	else if(is_popup && !cmd->from_prev_frame) {
		ImGui::Text("Open node for command data");
	}

	if (is_popup || cmd->from_prev_frame) {
		return;
	}

	bool is_shown = g_show_command_data_window && g_command_data_index == cmd_index;
	auto id = ImGui::GetID((int)cmd_index);
	ImGui::PushID(id);
	if (ImGui::Checkbox("Command data", &is_shown)) {
		g_command_data_index = (uint32_t)cmd_index;
		g_show_command_data_window = is_shown;
		g_command_data_list_version = m_psx->GetStatus().sysbus->GetGPU().m_gp_commands_version;
	}
	ImGui::PopID();

	if ((cmd->reg == psx::CommandRegister::GP0 && cmd->gp0.type == psx::GP0CommandType::POLYGON &&
		cmd->gp0.polygon.is_textured()) || 
		(cmd->reg == psx::CommandRegister::GP0 && cmd->gp0.type == psx::GP0CommandType::RECTANGLE &&
		cmd->gp0.rect.is_textured())) {
		bool is_shown = g_show_command_texture_window && g_command_texture_index == cmd_index;
		auto name = fmt::format("#cmd{}_texture", cmd_index);
		auto id = ImGui::GetID(name.c_str());

		ImGui::PushID(id);
		if (ImGui::Checkbox("View texture", &is_shown)) {
			g_command_texture_index = (uint32_t)cmd_index;
			g_show_command_texture_window = is_shown;
			g_command_texture_list_version = m_psx->GetStatus().sysbus->GetGPU().m_gp_commands_version;
		}
		ImGui::PopID();
	}
}

void DebugView::ShowGpuCmdPolygon(psx::GPUCommand const* cmd, size_t cmd_index) {
	auto polygon_cmd = cmd->gp0.polygon;

	auto blending_string = polygon_cmd.is_textured() ? (
		polygon_cmd.is_raw_texture() ? "raw texture" : "texture blended"
		) : "";
	auto final_string = fmt::format("{} {}{} {}",
		polygon_cmd.is_quad() ? "Quad" : "Triangle",
		polygon_cmd.is_textured() ? "textured " : "",
		polygon_cmd.is_semi_transparent() ? "semi-transparent" : "opaque",
		blending_string);
	ImGui::Text("%s", final_string.c_str());

	if (polygon_cmd.is_semi_transparent()) {
		constexpr const char* SEMI_TRANSPARENCIES[] = { "B/2+F/2", "B+F", "B-F", "B+F/4" };
		ImGui::Text("Semi-transparency  : %s", SEMI_TRANSPARENCIES[cmd->params.rendering.transparency_type]);
	}

	uint32_t color_depth = {};
	uint32_t x_base = {};
	uint32_t y_base = {};
	uint32_t clut_x = {};
	uint32_t clut_y = {};
	if (polygon_cmd.is_textured()) {
		auto texpage = cmd->params.rendering.vertices[0].clut_page & 0xFFFF;
		color_depth = (texpage >> 7) & 0x3;
		constexpr const char* TEXPAGE_COLORS[] = { "4BPP", "8BPP", "15BPP", "RESERVED" };
		ImGui::Text("Texpage bit depth  : %s", TEXPAGE_COLORS[color_depth]);
		x_base = (texpage & 0xF) * 64;
		y_base = (((texpage >> 4) & 1) | (((texpage >> 11) & 1) << 1)) * 256;
		auto clut = (cmd->params.rendering.vertices[0].clut_page >> 16) & 0xFFFF;
		clut_x = (clut & 0x3F) * 16;
		clut_y = (clut >> 6) & 0x1FF;
		if (color_depth != 2) {
			ImGui::Text("Clut X: %d, Y: %d", clut_x, clut_y);
		}
	}

	HighlitArea area1{};
	area1.filled = true;
	area1.name = polygon_cmd.is_quad() ? "Quad" : "Triangle";
	area1.num_vertices = 3;
	area1.cmd_index = cmd_index;

	HighlitArea area2{};
	area2 = area1;

	constexpr uint32_t COLOR_DEPTH_DIVISOR[] = { 4, 2, 1, 1 };
	for (size_t vertex_index = 0; vertex_index < (polygon_cmd.is_quad() ? 4 : 3); vertex_index++) {
		auto const& vertex = cmd->params.rendering.vertices[vertex_index];
		ImGui::Text("X%d: %d, Y%d: %d", vertex_index, vertex.x, vertex_index, vertex.y);

		area1.x[vertex_index] = vertex.x;
		area1.y[vertex_index] = vertex.y;

		if (polygon_cmd.is_textured()) {
			uint32_t u = cmd->params.rendering.vertices[vertex_index].u;
			uint32_t v = cmd->params.rendering.vertices[vertex_index].v;
			u = (u / COLOR_DEPTH_DIVISOR[color_depth]) + x_base;
			v += y_base;
			ImGui::Text("U%d: %d, V%d: %d", vertex_index, u,
				vertex_index, v);
		}

		if (polygon_cmd.is_gouraud() && (!polygon_cmd.is_textured() || !polygon_cmd.is_raw_texture())) {
			ImGui::Text("Color: "); ImGui::SameLine();
			ColorPicker(uint8_t(cmd->params.rendering.vertices[vertex_index].color.r()),
				uint8_t(cmd->params.rendering.vertices[vertex_index].color.g()),
				uint8_t(cmd->params.rendering.vertices[vertex_index].color.b()),
				fmt::format("#cmd{}{}", cmd_index, vertex_index));
		}
	}

	/*
	prim2.vertices[0] = vertices[1];
	prim2.vertices[1] = vertices[2];
	prim2.vertices[2] = vertices[3];
	*/
	m_highlited_areas.push_back(area1);

	if (polygon_cmd.is_quad()) {
		area2.x[0] = area1.x[1];
		area2.x[1] = area1.x[2];
		area2.x[2] = area1.x[3];

		area2.y[0] = area1.y[1];
		area2.y[1] = area1.y[2];
		area2.y[2] = area1.y[3];
		m_highlited_areas.push_back(area2);
	}

	GpuCommandAppendClipRect(cmd);

	if ((!polygon_cmd.is_textured() || !polygon_cmd.is_raw_texture()) && !polygon_cmd.is_gouraud()) {
		ImGui::Text("Color: "); ImGui::SameLine();
		ColorPicker(uint8_t(cmd->params.rendering.vertices[0].color.r()),
			uint8_t(cmd->params.rendering.vertices[0].color.g()),
			uint8_t(cmd->params.rendering.vertices[0].color.b()),
			fmt::format("#cmd{}", cmd_index));
	}
}

void DebugView::ShowGpuCmdRectangle(psx::GPUCommand const* cmd, size_t cmd_index) {
	auto rect_cmd = cmd->gp0.rect;

	//Textured
	//Semi-transparent
	//Texture-blending
	auto blending_string = rect_cmd.is_textured() ? (
		rect_cmd.is_raw_texture() ? "raw texture" : "texture blended"
		) : "";
	auto final_string = fmt::format("Rectangle {}{} {}",
		rect_cmd.is_textured() ? "textured " : "",
		rect_cmd.is_semi_transparent() ? "semi-transparent" : "opaque",
		blending_string);
	ImGui::Text("%s", final_string.c_str());

	if (rect_cmd.is_semi_transparent()) {
		constexpr const char* SEMI_TRANSPARENCIES[] = { "B/2+F/2", "B+F", "B-F", "B+F/4" };
		ImGui::Text("Semi-transparency  : %s", SEMI_TRANSPARENCIES[cmd->params.rendering.transparency_type]);
	}

	uint32_t color_depth = {};
	uint32_t x_base = {};
	uint32_t y_base = {};
	uint32_t clut_x = {};
	uint32_t clut_y = {};
	if (rect_cmd.is_textured()) {
		auto texpage = cmd->params.rendering.vertices[0].clut_page & 0xFFFF;
		color_depth = (texpage >> 7) & 0x3;
		constexpr const char* TEXPAGE_COLORS[] = { "4BPP", "8BPP", "15BPP", "RESERVED" };
		ImGui::Text("Texpage bit depth  : %s", TEXPAGE_COLORS[color_depth]);
		x_base = (texpage & 0xF) * 64;
		y_base = (((texpage >> 4) & 1) | (((texpage >> 11) & 1) << 1)) * 256;
		auto clut = (cmd->params.rendering.vertices[0].clut_page >> 16) & 0xFFFF;
		clut_x = (clut & 0x3F) * 16;
		clut_y = (clut >> 6) & 0x1FF;
		if (color_depth != 2) {
			ImGui::Text("Clut X: %d, Y: %d", clut_x, clut_y);
		}
	}

	/*
	texpage |= gpu_stat.texture_page_x_base;
	texpage |= ((u16)gpu_stat.texture_page_y_base << 4);
	texpage |= ((u16)gpu_stat.semi_transparency << 5);
	texpage |= ((u16)gpu_stat.tex_page_colors << 7);
	texpage |= ((u16)gpu_stat.texture_page_y_base2 << 11);

	clut: 0-5 x coord, 6-14 y coord

	tex_and_clut = (clut << 16) | texpage;
	*/
	HighlitArea area{};
	area.filled = true;
	area.name = fmt::format("Rect");
	area.num_vertices = 4;
	area.cmd_index = cmd_index;

	constexpr uint32_t COLOR_DEPTH_DIVISOR[] = { 4, 2, 1, 1 };
	for (size_t vertex_index = 0; vertex_index < 4; vertex_index++) {
		auto const& vertex = cmd->params.rendering.vertices[vertex_index];
		ImGui::Text("X%d: %d, Y%d: %d", vertex_index, vertex.x, vertex_index, vertex.y);

		area.x[vertex_index] = vertex.x;
		area.y[vertex_index] = vertex.y;

		if (rect_cmd.is_textured()) {
			uint32_t u = cmd->params.rendering.vertices[vertex_index].u;
			uint32_t v = cmd->params.rendering.vertices[vertex_index].v;
			u = (u / COLOR_DEPTH_DIVISOR[color_depth]) + x_base;
			v += y_base;
			ImGui::Text("U%d: %d, V%d: %d", vertex_index, u,
				vertex_index, v);
		}
	}

	std::swap(area.y[1], area.y[2]);
	m_highlited_areas.push_back(area);

	if (!rect_cmd.is_textured() || !rect_cmd.is_raw_texture()) {
		ImGui::Text("Color: "); ImGui::SameLine();
		ColorPicker(uint8_t(cmd->params.rendering.vertices[0].color.r()),
			uint8_t(cmd->params.rendering.vertices[0].color.g()),
			uint8_t(cmd->params.rendering.vertices[0].color.b()),
			fmt::format("#cmd{}", cmd_index));
	}

	GpuCommandAppendClipRect(cmd);
}

void DebugView::ShowGpuCmdLine(psx::GPUCommand const* cmd, size_t cmd_index) {
	auto line_cmd = cmd->gp0.line;
	//line_cmd.
	auto final_string = fmt::format("{} shaded {} {}",
		line_cmd.is_gouraud() ? "Gouraud" : "Flat",
		line_cmd.is_semi_transparent() ? "semi-transparent" : "opaque",
		line_cmd.is_polyline() ? "polyline" : "line");
	ImGui::Text("%s", final_string.c_str());

	if (line_cmd.is_semi_transparent()) {
		constexpr const char* SEMI_TRANSPARENCIES[] = { "B/2+F/2", "B+F", "B-F", "B+F/4" };
		ImGui::Text("Semi-transparency  : %s", SEMI_TRANSPARENCIES[cmd->params.rendering.transparency_type]);
	}
	
	for (size_t vertex_index = 0; vertex_index < 2; vertex_index++) {
		auto const& vertex = cmd->params.rendering.vertices[vertex_index];
		ImGui::Text("X%d: %d, Y%d: %d", vertex_index, vertex.x, vertex_index, vertex.y);
		if (line_cmd.is_gouraud()) {
			ImGui::Text("Color %d: ", vertex_index); ImGui::SameLine();
			ColorPicker(vertex.color);
		}
	}

	if (!line_cmd.is_gouraud()) {
		ImGui::Text("Color: "); ImGui::SameLine();
		ColorPicker(cmd->params.rendering.vertices[0].color);
	}

	HighlitArea area{};
	area.name = fmt::format("Command {} line", cmd_index);
	area.num_vertices = 2;
	area.x[0] = cmd->params.rendering.vertices[0].x;
	area.y[0] = cmd->params.rendering.vertices[0].y;
	area.x[1] = cmd->params.rendering.vertices[1].x;
	area.y[1] = cmd->params.rendering.vertices[1].y;
	m_highlited_areas.emplace_back(area);
	GpuCommandAppendClipRect(cmd);
}

void DebugView::ShowGpuCmdData(psx::GPUCommand const* cmd, size_t cmd_index) {
	auto window_name = fmt::format("Command {} data", cmd_index);
	ImGui::Begin(window_name.c_str(), &g_show_command_data_window);

	auto& gpu = m_psx->GetStatus().sysbus->GetGPU();
	if (gpu.m_gp_commands_version == g_command_data_list_version) {
		if (cmd->start_index >= gpu.m_recorded_gp_commands.size() ||
			cmd->start_index == UINT64_MAX) {
			ImGui::Text("Invalid data index");
		}
		else {
			auto curr_index = cmd->start_index;
			auto reg_index = gpu.m_recorded_gp_commands[curr_index].reg_index;
			size_t item_count{1};
			while (curr_index < gpu.m_recorded_gp_commands.size()) {
				curr_index++;
				auto const& stored_cmd = gpu.m_recorded_gp_commands[curr_index];
				if (stored_cmd.reg_index == reg_index && !stored_cmd.end_marker) {
					item_count++;
				} else if (stored_cmd.end_marker &&
					(stored_cmd.reg_index == reg_index || stored_cmd.end_reg_independent)) {
					break;
				}
			}

			ImGui::Text("Total bytes: %u", item_count * 4);
			ImGui::Text("Register index: %d", reg_index);
			ImGui::Separator();

			ImGuiListClipper clipper{};
			clipper.Begin((int)item_count);

			while (clipper.Step()) {
				for (size_t index = clipper.DisplayStart; index < clipper.DisplayEnd; index++) {
					if (gpu.m_recorded_gp_commands[cmd->start_index + index].reg_index != reg_index) {
						continue;
					}
					auto text = fmt::format("{:#010x}", gpu.m_recorded_gp_commands[cmd->start_index + index].value);
					ImGui::Text(text.c_str());
				}
			}
		}
	}
	else {
		g_show_command_data_window = false;
	}

	ImGui::End();
}

struct TextureViewData {
	uint32_t color_depth = {};
	uint32_t x_base = {};
	uint32_t y_base = {};
	uint32_t clut_x = {};
	uint32_t clut_y = {};
	psx::video::Shader* render_shader;
};

static void DownloadPossiblyOverflowingTexture(uint32_t vram_handle,
	uint32_t x, uint32_t y,
	uint32_t w, uint32_t h,
	std::vector<uint16_t>& out_data,
	uint32_t buf_size) {
	if (x + w < 1024 && y + h < 512) {
		glGetTextureSubImage(vram_handle, 0, x, y, 0, w, h, 1, GL_RGBA,
			GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, buf_size,
			(void*)out_data.data());
	}
	else if (x + w >= 1024 && y + h >= 512) {
		uint16_t* out_buf_ptr = out_data.data();
		//Top left
		glGetTextureSubImage(vram_handle, 0, x, y, 0, 1024 - x, 512 - y, 1, GL_RGBA,
			GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, buf_size,
			(void*)out_buf_ptr);
		out_buf_ptr += (1024 - x) * (512 - y);

		//Top right
		glGetTextureSubImage(vram_handle, 0, 0, y, 0, (x + w) & 0x3FF, 512 - y, 1, GL_RGBA,
			GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, buf_size,
			(void*)out_buf_ptr);
		out_buf_ptr += ((x + w) & 0x3FF) * (512 - y);

		//Bottom left
		glGetTextureSubImage(vram_handle, 0, x, 0, 0, 1024 - x, (y + h) & 0x1FF, 1, GL_RGBA,
			GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, buf_size,
			(void*)out_buf_ptr);
		out_buf_ptr += (1024 - x) * ((y + h) & 0x1FF);

		//Bottom right
		glGetTextureSubImage(vram_handle, 0, 0, 0, 0, (x + w) & 0x3FF, (y + h) & 0x1FF, 1, GL_RGBA,
			GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, buf_size,
			(void*)out_buf_ptr);
		out_buf_ptr += ((x + w) & 0x3FF) * ((y + h) & 0x1FF);

		const auto row_stride_pixels = w;
		const auto second_part_offset = (1024 - x) * (512 - y);
		const auto third_part_offset = second_part_offset + ((x + w) & 0x3FF) * (512 - y);
		const auto fourth_part_offset = third_part_offset + (1024 - x) * ((y + h) & 0x1FF);

		const auto left_side_stride_pixels = 1024 - x;
		const auto right_side_stride_pixels = (x + w) & 0x3FF;

		const auto rows_top = 512 - y;
		const auto rows_bottom = (y + h) & 0x1FF;
		
		auto reorder_temp = std::vector<uint16_t>{};
		reorder_temp.resize(out_data.size());
		//Reorder top left and right
		for (uint32_t curr_row = 0; curr_row < rows_top; curr_row++) {
			const auto pos_in_unordered = left_side_stride_pixels * curr_row;
			const auto pos_in_ordered = row_stride_pixels * curr_row;
			std::copy_n(out_data.data() + pos_in_unordered, left_side_stride_pixels,
				reorder_temp.data() + pos_in_ordered);
		}
		for (uint32_t curr_row = 0; curr_row < rows_top; curr_row++) {
			const auto pos_in_unordered = second_part_offset + right_side_stride_pixels * curr_row;
			const auto pos_in_ordered = (row_stride_pixels * curr_row) + left_side_stride_pixels;
			std::copy_n(out_data.data() + pos_in_unordered, right_side_stride_pixels,
				reorder_temp.data() + pos_in_ordered);
		}

		for (uint32_t curr_row = rows_top; curr_row < h; curr_row++) {
			const auto pos_in_unordered = third_part_offset + left_side_stride_pixels * (curr_row - rows_top);
			const auto pos_in_ordered = row_stride_pixels * curr_row;
			std::copy_n(out_data.data() + pos_in_unordered, left_side_stride_pixels,
				reorder_temp.data() + pos_in_ordered);
		}
		for (uint32_t curr_row = rows_top; curr_row < h; curr_row++) {
			const auto pos_in_unordered = fourth_part_offset + right_side_stride_pixels * (curr_row - rows_top);
			const auto pos_in_ordered = (row_stride_pixels * curr_row) + left_side_stride_pixels;
			std::copy_n(out_data.data() + pos_in_unordered, right_side_stride_pixels,
				reorder_temp.data() + pos_in_ordered);
		}
		std::copy_n(reorder_temp.begin(), reorder_temp.size(), out_data.data());
	}
	else if (x + w >= 1024) {
		uint16_t* out_buf_ptr = out_data.data();
		glGetTextureSubImage(vram_handle, 0, x, y, 0, 1024 - x, h, 1, GL_RGBA,
			GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, buf_size,
			(void*)out_buf_ptr);
		out_buf_ptr += (1024 - x) * h;
		glGetTextureSubImage(vram_handle, 0, 0, y, 0, (x + w) & 0x3FF, h, 1, GL_RGBA,
			GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, buf_size,
			(void*)out_buf_ptr);
		//Reordering needed, pixels from the next row will
		//be interpreted as part of the current row.
		//The right part of the texture will end up
		//at the bottom of the image, also with
		//the same behavior as above
		const auto row_stride_pixels = w;
		const auto left_side_stride_pixels = 1024 - x;
		const auto right_side_stride_pixels = (x + w) & 0x3FF;
		const auto second_part_offset = (1024 - x) * h;
		auto reorder_temp = std::vector<uint16_t>{};
		reorder_temp.resize(out_data.size());
		for (uint32_t curr_row = 0; curr_row < h; curr_row++) {
			const auto pos_in_unordered = left_side_stride_pixels * curr_row;
			const auto pos_in_ordered = row_stride_pixels * curr_row;
			std::copy_n(out_data.data() + pos_in_unordered, left_side_stride_pixels,
				reorder_temp.data() + pos_in_ordered);
		}
		for (uint32_t curr_row = 0; curr_row < h; curr_row++) {
			const auto pos_in_unordered = second_part_offset + right_side_stride_pixels * curr_row;
			const auto pos_in_ordered = (row_stride_pixels * curr_row) + left_side_stride_pixels;
			std::copy_n(out_data.data() + pos_in_unordered, right_side_stride_pixels,
				reorder_temp.data() + pos_in_ordered);
		}
		std::copy_n(reorder_temp.begin(), reorder_temp.size(), out_data.data());
	}
	else {
		uint16_t* out_buf_ptr = out_data.data();
		glGetTextureSubImage(vram_handle, 0, x, y, 0, w, 512 - y, 1, GL_RGBA,
			GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, buf_size,
			(void*)out_buf_ptr);
		out_buf_ptr += w * (512 - y);
		glGetTextureSubImage(vram_handle, 0, x, 0, 0, w, (y + h) & 0x3FF, 1, GL_RGBA,
			GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, buf_size,
			(void*)out_buf_ptr);
		//No reordering needed, bottom rectangle is already located
		//in the correct position
	}
}

static void DecodeStoreTexture(uint32_t vram_handle, 
	uint32_t x, uint32_t y, 
	uint32_t w, uint32_t h,
	uint32_t color_depth, 
	uint32_t clut_x, uint32_t clut_y, 
	std::string const& fpath) {
	if (w & 1) {
		w += 1;
	}

	if (h & 1) {
		h += 1;
	}

	auto compressed = std::vector<uint16_t>{};
	const auto compressed_texture_size_pixels = w * h;
	const auto compressed_texture_size_bytes = compressed_texture_size_pixels * 2;
	compressed.resize(compressed_texture_size_bytes);

	DownloadPossiblyOverflowingTexture(vram_handle, x, y, w, h,
		compressed, compressed_texture_size_bytes);

	constexpr uint32_t PIXELS_PER_ENTRY[] = { 4, 2, 1, 1 };

	const auto current_pixels_per_entry = PIXELS_PER_ENTRY[color_depth];
	const auto decompressed_texture_w = current_pixels_per_entry * w;
	const auto decompressed_texture_h = h;
	const auto decompressed_texture_size_pixels = decompressed_texture_w * decompressed_texture_h;
	const auto decompressed_texture_size_bytes = decompressed_texture_size_pixels * 4;
	auto decompressed = std::vector<uint8_t>{};
	decompressed.resize(decompressed_texture_size_bytes);

	auto clut_map = std::unordered_map<uint32_t, uint32_t>{};

	auto get_clut_entry = [&clut_map, vram_handle, clut_x, clut_y](uint32_t index) {
		auto absolute_pos = ((clut_y & 0x1FF) * 1024 + ((clut_x + index) & 0x3FF));
		if (clut_map.contains(absolute_pos)) {
			return clut_map[absolute_pos];
		}

		uint32_t value{};
		glGetTextureSubImage(vram_handle, 0, (clut_x + index) & 0x3FF, clut_y, 0, 1, 1, 1,
			GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, 4, (void*)&value);
		value &= 0xFFFF;

		uint32_t decompressed_value{};
		decompressed_value |= uint32_t(((value & 0x1F) / 31.f) * 255);
		decompressed_value |= uint32_t((((value >> 5) & 0x1F) / 31.f) * 255) << 8;
		decompressed_value |= uint32_t((((value >> 10) & 0x1F) / 31.f) * 255) << 16;
		decompressed_value |= uint32_t(255) << 24;

		clut_map[absolute_pos] = decompressed_value;
		return decompressed_value;
	};

	if (color_depth == 0) {//4 bpp
		for (uint32_t curr_pixel = 0; curr_pixel < compressed_texture_size_pixels;
			curr_pixel++) {
			auto curr_decompressed_pixel = curr_pixel * current_pixels_per_entry;
			auto entry = compressed[curr_pixel];

			auto index_1 = entry & 0xF;
			auto index_2 = (entry >> 4) & 0xF;
			auto index_3 = (entry >> 8) & 0xF;
			auto index_4 = (entry >> 12) & 0xF;

			auto pixel_1 = get_clut_entry(index_1);
			auto pixel_2 = get_clut_entry(index_2);
			auto pixel_3 = get_clut_entry(index_3);
			auto pixel_4 = get_clut_entry(index_4);

			auto write_ptr = std::bit_cast<uint32_t*>(decompressed.data()) + curr_decompressed_pixel;
			write_ptr[0] = pixel_1;
			write_ptr[1] = pixel_2;
			write_ptr[2] = pixel_3;
			write_ptr[3] = pixel_4;
		}
	} 
	else if (color_depth == 1) {//8 bpp
		for (uint32_t curr_pixel = 0; curr_pixel < compressed_texture_size_pixels;
			curr_pixel++) {
			auto curr_decompressed_pixel = curr_pixel * current_pixels_per_entry;
			auto entry = compressed[curr_pixel];

			auto index_1 = entry & 0xFF;
			auto index_2 = (entry >> 8) & 0xFF;

			auto pixel_1 = get_clut_entry(index_1);
			auto pixel_2 = get_clut_entry(index_2);

			auto write_ptr = std::bit_cast<uint32_t*>(decompressed.data()) + curr_decompressed_pixel;
			write_ptr[0] = pixel_1;
			write_ptr[1] = pixel_2;
		}
	}
	else if (color_depth == 2) {//16 bpp
		for (uint32_t curr_pixel = 0; curr_pixel < compressed_texture_size_pixels;
			curr_pixel++) {
			auto curr_decompressed_pixel = curr_pixel;
			auto entry = compressed[curr_pixel];

			uint32_t decompressed_pixel{};
			decompressed_pixel |= uint32_t(((entry & 0x1F) / 31.f) * 255);
			decompressed_pixel |= uint32_t((((entry >> 5) & 0x1F) / 31.f) * 255) << 8;
			decompressed_pixel |= uint32_t((((entry >> 10) & 0x1F) / 31.f) * 255) << 16;
			decompressed_pixel |= uint32_t(255) << 24;

			auto write_ptr = std::bit_cast<uint32_t*>(decompressed.data()) + curr_decompressed_pixel;
			write_ptr[0] = decompressed_pixel;
		}
	}

	auto fs_path = std::filesystem::path(fpath);
	auto extension = fs_path.extension().string();

	if (extension == ".bmp") {
		stbi_write_bmp(fpath.c_str(), decompressed_texture_w, decompressed_texture_h,
			4, (void*)decompressed.data());
	}
	else if (extension == ".png") {
		stbi_write_png(fpath.c_str(), decompressed_texture_w, decompressed_texture_h,
			4, (void*)decompressed.data(), decompressed_texture_w * 4);
	}
}

void DebugView::ShowGpuCmdTexture(psx::GPUCommand const* cmd, size_t cmd_index) {
	auto window_name = fmt::format("Command {} texture", cmd_index);
	ImGui::Begin(window_name.c_str(), &g_show_command_texture_window);

	auto& gpu = m_psx->GetStatus().sysbus->GetGPU();
	if (gpu.m_gp_commands_version == g_command_texture_list_version) {
		if (!m_texture_view_shader) {
			m_texture_view_shader = std::make_unique<psx::video::Shader>("../shaders", "imgui_image_shader");
		}

		static int s_scale{1};
		ImGui::SetNextItemWidth(80.f);
		ImGui::InputInt("Scale texture", &s_scale);
		s_scale = std::max(0, s_scale);

		uint32_t color_depth = {};
		uint32_t x_base = {};
		uint32_t y_base = {};
		uint32_t clut_x = {};
		uint32_t clut_y = {};

		constexpr uint32_t COLOR_DEPTH_DENOMINATOR[] = { 4, 2, 1, 1 };

		auto texpage = cmd->params.rendering.vertices[0].clut_page & 0xFFFF;
		color_depth = (texpage >> 7) & 0x3;
		constexpr const char* TEXPAGE_COLORS[] = { "4BPP", "8BPP", "15BPP", "RESERVED" };
		ImGui::Text("Texpage bit depth  : %s", TEXPAGE_COLORS[color_depth]);
		auto curr_color_depth_denom = COLOR_DEPTH_DENOMINATOR[color_depth];
		x_base = (texpage & 0xF) * 64;
		y_base = (((texpage >> 4) & 1) | (((texpage >> 11) & 1) << 1)) * 256;
		auto clut = (cmd->params.rendering.vertices[0].clut_page >> 16) & 0xFFFF;
		clut_x = (clut & 0x3F) * 16;
		clut_y = (clut >> 6) & 0x1FF;
		if (color_depth != 2) {
			ImGui::Text("Clut X: %d, Y: %d", clut_x, clut_y);
		}

		int32_t x[4] = {};
		int32_t y[4] = {};

		uint32_t view_u[4] = {};
		uint32_t view_v[4] = {};

		size_t num_vertices = cmd->gp0.type == psx::GP0CommandType::RECTANGLE ?
			4 : (cmd->gp0.polygon.is_quad() ? 4 : 3);

		for (size_t vertex_index = 0; vertex_index < num_vertices; vertex_index++) {
			auto const& vertex = cmd->params.rendering.vertices[vertex_index];
			ImGui::Text("X%d: %d, Y%d: %d", vertex_index, vertex.x, vertex_index, vertex.y);

			x[vertex_index] = vertex.x;
			y[vertex_index] = vertex.y;
			
			uint32_t u = cmd->params.rendering.vertices[vertex_index].u;
			uint32_t v = cmd->params.rendering.vertices[vertex_index].v;

			view_u[vertex_index] = u;
			view_v[vertex_index] = v;

			u = (u / COLOR_DEPTH_DENOMINATOR[color_depth]) + x_base;
			v += y_base;
			ImGui::Text("U%d: %d, V%d: %d", vertex_index, u,
				vertex_index, v);
		}

		int32_t min_x{}, max_x{};
		int32_t min_y{}, max_y{};

		uint32_t min_u{}, max_u{};
		uint32_t min_v{}, max_v{};

		if (num_vertices == 4) {
			min_x = std::min({ x[0], x[1], x[2], x[3] });
			max_x = std::max({ x[0], x[1], x[2], x[3] });

			min_y = std::min({ y[0], y[1], y[2], y[3] });
			max_y = std::max({ y[0], y[1], y[2], y[3] });

			min_u = std::min({ view_u[0], view_u[1], view_u[2], view_u[3] });
			max_u = std::max({ view_u[0], view_u[1], view_u[2], view_u[3] });

			min_v = std::min({ view_v[0], view_v[1], view_v[2], view_v[3] });
			max_v = std::max({ view_v[0], view_v[1], view_v[2], view_v[3] });
		}
		else {
			min_x = std::min({ x[0], x[1], x[2] });
			max_x = std::max({ x[0], x[1], x[2] });

			min_y = std::min({ y[0], y[1], y[2] });
			max_y = std::max({ y[0], y[1], y[2] });

			min_u = std::min({ view_u[0], view_u[1], view_u[2] });
			max_u = std::max({ view_u[0], view_u[1], view_u[2] });

			min_v = std::min({ view_v[0], view_v[1], view_v[2] });
			max_v = std::max({ view_v[0], view_v[1], view_v[2] });
		}

		auto size_x = std::abs(max_x - min_x);
		auto size_y = std::abs(max_y - min_y);

		ImGui::Text("Size X: %d, Size Y: %d", size_x, size_y);

		auto vram_handle = m_psx->GetStatus().sysbus->GetGPU().GetRenderer()->GetVram().GetTextureHandle();

		if (ImGui::Button("Save")) {
			ImGuiFileDialog::Instance()->OpenDialog("SaveTexture", "Save texture",
				".bmp,.png");
		}
		if (ImGuiFileDialog::Instance()->Display("SaveTexture")) {
			if (ImGuiFileDialog::Instance()->IsOk()) {
				std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
				std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
				
				DecodeStoreTexture(vram_handle, 
					min_u / curr_color_depth_denom + x_base,
					min_v + y_base, 
					(max_u - min_u) / curr_color_depth_denom,
					(max_v - min_v),
					color_depth, clut_x, clut_y,
					filePathName);
			}

			ImGuiFileDialog::Instance()->Close();
		}

		TextureViewData render_data{
			.color_depth = color_depth,
			.x_base = x_base,
			.y_base = y_base,
			.clut_x = clut_x,
			.clut_y = clut_y,
			.render_shader = m_texture_view_shader.get()
		};

		auto same_viewport_as_main = ImGui::GetMainViewport() == ImGui::GetWindowViewport();

		auto window_pos = same_viewport_as_main ? ImGui::GetMainViewport()->WorkPos : ImGui::GetWindowPos();
		auto begin = ImGui::GetCursorScreenPos();
		begin.x -= window_pos.x;
		begin.y -= window_pos.y;
		begin.y += 20.f;

		auto draw_list = ImGui::GetWindowDrawList();
		
		auto viewport_size = ImGui::GetWindowViewport()->WorkSize;
		
		GpuCommandAppendPossiblyOverflowedAreas(
			min_u / curr_color_depth_denom + x_base, 
			min_v + y_base,
			(max_u - min_u) / curr_color_depth_denom,
			(max_v - min_v), "Texture source", cmd_index);

		HighlitArea texpage_indicator{};
		texpage_indicator.num_vertices = 2;
		texpage_indicator.x[0] = 512;
		texpage_indicator.y[0] = 256;
		texpage_indicator.x[1] = (min_u / curr_color_depth_denom + x_base) & 0x3FF;
		texpage_indicator.y[1] = (min_v + y_base) & 0x1FF;
		m_highlited_areas.push_back(texpage_indicator);

		if (color_depth != 2) {
			HighlitArea clut_indicator{};
			clut_indicator.num_vertices = 2;
			clut_indicator.x[0] = 512;
			clut_indicator.y[0] = 256;
			clut_indicator.x[1] = clut_x;
			clut_indicator.y[1] = clut_y;
			m_highlited_areas.push_back(clut_indicator);

			if (color_depth == 0) {
				GpuCommandAppendPossiblyOverflowedAreas(clut_x, clut_y, 16, 1,
					fmt::format("Command {} clut", cmd_index), cmd_index);
			}
			else if (color_depth == 1) {
				GpuCommandAppendPossiblyOverflowedAreas(clut_x, clut_y, 256, 1,
					fmt::format("Command {} clut", cmd_index), cmd_index);
			}
		}

		draw_list->AddCallback([](const ImDrawList*, const ImDrawCmd* draw_cmd) {
			TextureViewData render_data{};
			std::copy_n((TextureViewData*)draw_cmd->UserCallbackData, 1, &render_data);
			render_data.render_shader->BindProgram();
			render_data.render_shader->UpdateUniform("x_base", render_data.x_base);
			render_data.render_shader->UpdateUniform("y_base", render_data.y_base);
			render_data.render_shader->UpdateUniform("clut_x", render_data.clut_x);
			render_data.render_shader->UpdateUniform("clut_y", render_data.clut_y);
			render_data.render_shader->UpdateUniform("texpage_colors", render_data.color_depth);
			glDisable(GL_BLEND);
		}, (void*)&render_data, sizeof(render_data));
		draw_list->AddImage((ImTextureID)(intptr_t)vram_handle, 
			ImVec2(begin.x / viewport_size.x, begin.y / viewport_size.y),
			ImVec2((begin.x + size_x * s_scale) / viewport_size.x, (begin.y + size_y * s_scale) / viewport_size.y),
			ImVec2(min_u / 1024.f, min_v / 512.f),
			ImVec2(max_u / 1024.f, max_v / 512.f));
		draw_list->AddCallback([](const ImDrawList*, const ImDrawCmd* draw_cmd) {
			auto gl_ctx = psx::video::GetCurrentGLContext();
			gl_ctx->BindProgram(0);
		}, nullptr);
		draw_list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
	}
	else {
		g_show_command_texture_window = false;
	}

	ImGui::End();
}

std::string DebugView::GetGpuCommandName(psx::GPUCommand const* cmd) {
	switch (cmd->reg)
	{
	case psx::CommandRegister::GP0:
		using GP0Command = psx::GP0CommandType;
		switch (cmd->gp0.type)
		{
		case GP0Command::MISC: {
			switch (cmd->gp0.misc.type)
			{
			case psx::MiscCommandType::NOP:
			case psx::MiscCommandType::NOP_FIFO:
				return "NOP";
			case psx::MiscCommandType::CLEAR_CACHE:
				return "CLEAR TEXTURE CACHE";
			case psx::MiscCommandType::QUICK_FILL:
				return fmt::format("RECTANGLE FILL (R: {}, G: {}, B: {})",
					cmd->gp0.misc.quick_fill.r, cmd->gp0.misc.quick_fill.g,
					cmd->gp0.misc.quick_fill.b);
			}
		} break;
		case GP0Command::POLYGON:
			return fmt::format("POLYGON (QUAD: {}, TEXTURED: {}, GOURAUD: {}, TRANSPARENT: {}, RAW MAPPING: {})", 
				cmd->gp0.polygon.is_quad() ? "Y" : "N",
				cmd->gp0.polygon.is_textured() ? "Y" : "N",
				cmd->gp0.polygon.is_gouraud() ? "Y" : "N",
				cmd->gp0.polygon.is_semi_transparent() ? "Y" : "N",
				cmd->gp0.polygon.is_raw_texture() ? "Y" : "N");
		case GP0Command::LINE:
			return fmt::format("LINE (GORAUD: {}, TRANSPARENT: {}, POLYLINE: {})", 
				cmd->gp0.line.is_gouraud() ? "Y" : "N", 
				cmd->gp0.line.is_semi_transparent() ? "Y" : "N",
				cmd->gp0.line.is_polyline() ? "Y" : "N");
		case GP0Command::RECTANGLE:
			return fmt::format("RECTANGLE (TEXTURED: {}, TRANSPARENT: {}, RAW MAPPING: {})", 
				cmd->gp0.rect.is_textured() ? "Y" : "N", 
				cmd->gp0.rect.is_semi_transparent() ? "Y" : "N",
				cmd->gp0.rect.is_raw_texture() ? "Y" : "N");
		case GP0Command::POLYLINE_END:
			return "POLYLINE END MARKER";
		case GP0Command::VRAM_BLIT:
			return fmt::format("VRAM-VRAM BLIT (SRC. X: {}, SRC. Y {}, DST. X: {}, DST. Y: {}, W: {}, H: {})",
				cmd->params.vram_vram_blit.src_x, cmd->params.vram_vram_blit.src_y,
				cmd->params.vram_vram_blit.dst_x, cmd->params.vram_vram_blit.dst_y,
				cmd->params.vram_vram_blit.w, cmd->params.vram_vram_blit.h);
		case GP0Command::CPU_VRAM_BLIT:
			return fmt::format("CPU-VRAM BLIT (DST. X: {}, DST. Y: {}, W: {}, H: {})",
				cmd->params.cpu_vram_blit.dst_x, cmd->params.cpu_vram_blit.dst_y,
				cmd->params.cpu_vram_blit.w, cmd->params.cpu_vram_blit.h);
		case GP0Command::VRAM_CPU_BLIT:
			return fmt::format("VRAM-CPU BLIT (SRC. X: {}, SRC. Y: {}, W: {}, H: {})",
				cmd->params.vram_cpu_blit.src_x, cmd->params.vram_cpu_blit.src_y,
				cmd->params.vram_cpu_blit.w, cmd->params.vram_cpu_blit.h);
		case GP0Command::ENV: {
			switch (cmd->gp0.env.type)
			{
			case psx::EnvCommandType::TEXTURE_PAGE:
				return "TEXPAGE";
			case psx::EnvCommandType::TEXTURE_WINDOW:
				return fmt::format("TEXWINDOW (MASK X: {}, MASK Y: {}, OFF. X: {}, OFF. Y: {})",
					uint32_t(cmd->gp0.env.texwindow.mask_x),
					uint32_t(cmd->gp0.env.texwindow.mask_y),
					uint32_t(cmd->gp0.env.texwindow.offset_x),
					uint32_t(cmd->gp0.env.texwindow.offset_y));
			case psx::EnvCommandType::SET_DRAW_TOP:
				return fmt::format("DRAW AREA TOP LEFT (X: {}, Y: {})",
					uint32_t(cmd->gp0.env.draw_area.x_coord),
					uint32_t(cmd->gp0.env.draw_area.y_coord));
			case psx::EnvCommandType::SET_DRAW_BOTTOM:
				return fmt::format("DRAW AREA BOTTOM RIGHT (X: {}, Y: {})",
					uint32_t(cmd->gp0.env.draw_area.x_coord),
					uint32_t(cmd->gp0.env.draw_area.y_coord));
			case psx::EnvCommandType::SET_DRAW_OFFSET:
				return fmt::format("DRAW AREA OFFSET (X: {}, Y: {})",
					int32_t(cmd->gp0.env.draw_offset.x_offset), int32_t(cmd->gp0.env.draw_offset.y_offset));
			case psx::EnvCommandType::MASK_BIT:
				return fmt::format("MASK SETTING (SET MASK: {}, CHECK MASK: {})",
					bool(cmd->gp0.env.mask_bit.set_mask), bool(cmd->gp0.env.mask_bit.check_mask));
			}
		}
		}
		break;
	case psx::CommandRegister::GP1:
		using GP1Command = psx::GP1CommandType;
		switch (cmd->gp1.type) {
		case GP1Command::RESET:
			return "RESET";
		case GP1Command::RESET_CMD_FIFO:
			return "EMPTY FIFO";
		case GP1Command::IRQ_ACK:
			return "ACK IRQ";
		case GP1Command::DISPLAY_ENABLE:
			return fmt::format("DISP ENABLE (ENABLED: {})",
				bool(cmd->gp1.disp_enable.display_on));
		case GP1Command::DMA_DIRECTION:
			return fmt::format("DMA DIRECTION (DIRECTION: {})",
				magic_enum::enum_name(psx::DmaDir(cmd->gp1.dma_dir.direction)));
		case GP1Command::DISPLAY_AREA_START:
			return fmt::format("DISPLAY START (X: {}, Y: {})",
				uint32_t(cmd->gp1.disp_start.x), uint32_t(cmd->gp1.disp_start.y));
		case GP1Command::HORIZONTAL_DISPLAY_RANGE:
			return fmt::format("HORIZONTAL DISPLAY RANGE (X1: {}, X2: {})",
				uint32_t(cmd->gp1.hoz_disp_range.x1), uint32_t(cmd->gp1.hoz_disp_range.x2));
		case GP1Command::VERTICAL_DISPLAY_RANGE:
			return fmt::format("VERTICAL DISPLAY RANGE (Y1: {}, Y2: {})",
				uint32_t(cmd->gp1.vert_disp_range.y1), uint32_t(cmd->gp1.vert_disp_range.y2));
		case GP1Command::DISPLAY_MODE:
			return "DISPLAY MODE";
		case GP1Command::READ_GPU_REGISTER:
			return "READ GPU REGISTER";
		case GP1Command::SET_VRAM_SIZE:
			return fmt::format("SET VRAM SIZE (SET 2MB: {})",
				bool(cmd->gp1.set_vram_size.two_mbytes));
		}
		break;
	}

	return "<UNKNOWN>";
}

bool DebugView::GetGpuCommandHasDetails(psx::GPUCommand const* cmd) {
	//switch (cmd->reg)
	//{
	//case psx::CommandRegister::GP0:
	//	using GP0Command = psx::GP0CommandType;
	//	switch (cmd->gp0.type)
	//	{
	//	case GP0Command::MISC: {
	//		switch (cmd->gp0.misc.type)
	//		{
	//		case psx::MiscCommandType::NOP:
	//		case psx::MiscCommandType::NOP_FIFO:
	//			return false;
	//		case psx::MiscCommandType::CLEAR_CACHE:
	//			return false;
	//		case psx::MiscCommandType::QUICK_FILL:
	//			return true;
	//		}
	//	} break;
	//	case GP0Command::POLYGON:
	//		return true;
	//	case GP0Command::LINE:
	//		return true;
	//	case GP0Command::RECTANGLE:
	//		return true;
	//	case GP0Command::POLYLINE_END:
	//		return false;
	//	case GP0Command::VRAM_BLIT:
	//		return false;
	//	case GP0Command::CPU_VRAM_BLIT:
	//		return false;
	//	case GP0Command::VRAM_CPU_BLIT:
	//		return false;
	//	case GP0Command::ENV: {
	//		switch (cmd->gp0.env.type)
	//		{
	//		case psx::EnvCommandType::TEXTURE_PAGE:
	//			return true;
	//		case psx::EnvCommandType::TEXTURE_WINDOW:
	//			return false;
	//		case psx::EnvCommandType::SET_DRAW_TOP:
	//			return false;
	//		case psx::EnvCommandType::SET_DRAW_BOTTOM:
	//			return false;
	//		case psx::EnvCommandType::SET_DRAW_OFFSET:
	//			return false;
	//		case psx::EnvCommandType::MASK_BIT:
	//			return false;
	//		}
	//	}
	//	}
	//	break;
	//case psx::CommandRegister::GP1:
	//	using GP1Command = psx::GP1CommandType;
	//	switch (cmd->gp1.type) {
	//	case GP1Command::RESET:
	//		return false;
	//	case GP1Command::RESET_CMD_FIFO:
	//		return false;
	//	case GP1Command::IRQ_ACK:
	//		return false;
	//	case GP1Command::DISPLAY_ENABLE:
	//		return false;
	//	case GP1Command::DMA_DIRECTION:
	//		return false;
	//	case GP1Command::DISPLAY_AREA_START:
	//		return false;
	//	case GP1Command::HORIZONTAL_DISPLAY_RANGE:
	//		return false;
	//	case GP1Command::VERTICAL_DISPLAY_RANGE:
	//		return false;
	//	case GP1Command::DISPLAY_MODE:
	//		return true;
	//	case GP1Command::READ_GPU_REGISTER:
	//		return false;
	//	case GP1Command::SET_VRAM_SIZE:
	//		return false;
	//	}
	//	break;
	//}

	return true;
}

void DebugView::GpuCommandAppendVramAreas(psx::GPUCommand const* cmd, size_t cmd_index) {
	switch (cmd->reg)
	{
	case psx::CommandRegister::GP0:
		using GP0Command = psx::GP0CommandType;
		switch (cmd->gp0.type)
		{
		case GP0Command::MISC: {
			switch (cmd->gp0.misc.type)
			{
			case psx::MiscCommandType::NOP:
			case psx::MiscCommandType::NOP_FIFO:
				return;
			case psx::MiscCommandType::CLEAR_CACHE:
				return;
			case psx::MiscCommandType::QUICK_FILL: {
				uint32_t x{}, y{};
				uint32_t w{}, h{};
				x = cmd->params.quick_fill.x;
				y = cmd->params.quick_fill.y;
				w = cmd->params.quick_fill.w;
				h = cmd->params.quick_fill.h;

				GpuCommandAppendPossiblyOverflowedAreas(x, y, w, h, "Rectangle fill", cmd_index);
			}
				return;
			}
		} break;
		case GP0Command::POLYGON:
			return;
		case GP0Command::LINE:
			return;
		case GP0Command::RECTANGLE:
			return;
		case GP0Command::POLYLINE_END:
			return;
		case GP0Command::VRAM_BLIT: {
			uint32_t x{}, y{};
			uint32_t w{}, h{};

			x = cmd->params.vram_vram_blit.src_x;
			y = cmd->params.vram_vram_blit.src_y;
			w = cmd->params.vram_vram_blit.w;
			h = cmd->params.vram_vram_blit.h;

			GpuCommandAppendPossiblyOverflowedAreas(x, y, w, h, "VRAM-VRAM src", cmd_index,
				std::array{1.0f, 0.0f, 0.0f, .75f});

			x = cmd->params.vram_vram_blit.dst_x;
			y = cmd->params.vram_vram_blit.dst_y;

			GpuCommandAppendPossiblyOverflowedAreas(x, y, w, h, "VRAM-VRAM dst", cmd_index,
				std::array{ 240.f / 360.f, 0.0f, 1.0f, .75f });
		}
			return;
		case GP0Command::CPU_VRAM_BLIT: {
			uint32_t x{}, y{};
			uint32_t w{}, h{};
			x = cmd->params.cpu_vram_blit.dst_x;
			y = cmd->params.cpu_vram_blit.dst_y;
			w = cmd->params.cpu_vram_blit.w;
			h = cmd->params.cpu_vram_blit.h;

			GpuCommandAppendPossiblyOverflowedAreas(x, y, w, h, "CPU-VRAM Blit", cmd_index);
		}
			return;
		case GP0Command::VRAM_CPU_BLIT: {
			uint32_t x{}, y{};
			uint32_t w{}, h{};
			x = cmd->params.vram_cpu_blit.src_x;
			y = cmd->params.vram_cpu_blit.src_y;
			w = cmd->params.vram_cpu_blit.w;
			h = cmd->params.vram_cpu_blit.h;

			GpuCommandAppendPossiblyOverflowedAreas(x, y, w, h, "VRAM-CPU Blit", cmd_index);
		}
			return;
		case GP0Command::ENV: {
			switch (cmd->gp0.env.type)
			{
			case psx::EnvCommandType::TEXTURE_PAGE: {
				auto texpage = cmd->gp0.env.texpage;
				auto ybase = (texpage.y_base_1 | (texpage.y_base_2 << 1)) * 256;
				auto xbase = texpage.x_base * 64;

				HighlitArea area{};
				area.cmd_index = cmd_index;
				area.name = "Texpage start";
				area.num_vertices = 2;
				area.x[0] = 512;
				area.y[0] = 256;
				area.x[1] = xbase;
				area.y[1] = ybase;
				m_highlited_areas.emplace_back(area);
			}
				return;
			case psx::EnvCommandType::TEXTURE_WINDOW:
				return;
			case psx::EnvCommandType::SET_DRAW_TOP: {
				HighlitArea area{};
				area.cmd_index = cmd_index;
				area.name = "Draw top";
				area.num_vertices = 2;
				area.x[0] = 512;
				area.y[0] = 256;
				area.x[1] = cmd->gp0.env.draw_area.x_coord;
				area.y[1] = cmd->gp0.env.draw_area.y_coord;
				m_highlited_areas.emplace_back(area);
				GpuCommandAppendClipRect(cmd);
			}
				return;
			case psx::EnvCommandType::SET_DRAW_BOTTOM: {
				HighlitArea area{};
				area.cmd_index = cmd_index;
				area.name = "Draw bottom";
				area.num_vertices = 2;
				area.x[0] = 512;
				area.y[0] = 256;
				area.x[1] = cmd->gp0.env.draw_area.x_coord;
				area.y[1] = cmd->gp0.env.draw_area.y_coord;
				m_highlited_areas.emplace_back(area);
				GpuCommandAppendClipRect(cmd);
			}
				return;
			case psx::EnvCommandType::SET_DRAW_OFFSET: {
				HighlitArea area{};
				area.cmd_index = cmd_index;
				area.name = "Draw offset";
				area.num_vertices = 2;
				area.x[0] = 512;
				area.y[0] = 256;
				area.x[1] = cmd->gp0.env.draw_offset.x_offset;
				area.y[1] = cmd->gp0.env.draw_offset.y_offset;
				m_highlited_areas.emplace_back(area);
				GpuCommandAppendClipRect(cmd);
			}
				return;
			case psx::EnvCommandType::MASK_BIT:
				return;
			}
		}
		}
		break;
	case psx::CommandRegister::GP1:
		using GP1Command = psx::GP1CommandType;
		switch (cmd->gp1.type) {
		case GP1Command::RESET:
			return;
		case GP1Command::RESET_CMD_FIFO:
			return;
		case GP1Command::IRQ_ACK:
			return;
		case GP1Command::DISPLAY_ENABLE:
			return;
		case GP1Command::DMA_DIRECTION:
			return;
		case GP1Command::DISPLAY_AREA_START: {
			HighlitArea area{};
			area.cmd_index = cmd_index;
			area.name = "Display start";
			area.num_vertices = 2;
			area.x[0] = 512;
			area.y[0] = 256;
			area.x[1] = cmd->gp1.disp_start.x;
			area.y[1] = cmd->gp1.disp_start.y;
			m_highlited_areas.emplace_back(area);
		}
			return;
		case GP1Command::HORIZONTAL_DISPLAY_RANGE:
			return;
		case GP1Command::VERTICAL_DISPLAY_RANGE:
			return;
		case GP1Command::DISPLAY_MODE:
			return;
		case GP1Command::READ_GPU_REGISTER:
			return;
		case GP1Command::SET_VRAM_SIZE:
			return;
		}
		break;
	}
	
	return;
}

void DebugView::GpuCommandAppendPossiblyOverflowedAreas(uint32_t x, uint32_t y, uint32_t w, uint32_t h, std::string name, size_t cmd_index,
	std::optional<std::array<float, 4>> color) {
	if (x + w >= 1024 && y + h >= 512) {
		uint32_t x0_begin = x;
		uint32_t x0_end = 1024;
		uint32_t y0_begin = y;
		uint32_t y0_end = 512;

		uint32_t x1_begin = 0;
		uint32_t x1_end = (x + w) & 1023;
		uint32_t y1_begin = 0;
		uint32_t y1_end = (y + h) & 511;

		uint32_t x2_begin = 0;
		uint32_t x2_end = (x + w) & 1023;
		uint32_t y2_begin = y;
		uint32_t y2_end = 512;

		uint32_t x3_begin = x;
		uint32_t x3_end = 1024;
		uint32_t y3_begin = 0;
		uint32_t y3_end = (y + h) & 511;

		HighlitArea rect0{};
		HighlitArea rect1{};
		HighlitArea rect2{};
		HighlitArea rect3{};

		rect0.num_vertices = 4;
		rect1.num_vertices = 4;
		rect2.num_vertices = 4;
		rect3.num_vertices = 4;

		rect0.color = color;
		rect1.color = color;
		rect2.color = color;
		rect3.color = color;

		rect0.cmd_index = cmd_index;
		rect1.cmd_index = cmd_index;
		rect2.cmd_index = cmd_index;
		rect3.cmd_index = cmd_index;

		rect0.name = fmt::format("{}, no overflow"  , name);
		rect1.name = fmt::format("{}, both overflow", name);
		rect2.name = fmt::format("{}, x overflow"   , name);
		rect3.name = fmt::format("{}, y overflow"   , name);

		{
			rect0.x[0] = x0_begin;
			rect0.x[1] = x0_end;
			rect0.x[2] = x0_end;
			rect0.x[3] = x0_begin;

			rect0.y[0] = y0_begin;
			rect0.y[1] = y0_begin;
			rect0.y[2] = y0_end;
			rect0.y[3] = y0_end;
		}

		{
			rect1.x[0] = x1_begin;
			rect1.x[1] = x1_end;
			rect1.x[2] = x1_end;
			rect1.x[3] = x1_begin;

			rect1.y[0] = y1_begin;
			rect1.y[1] = y1_begin;
			rect1.y[2] = y1_end;
			rect1.y[3] = y1_end;
		}

		{
			rect2.x[0] = x2_begin;
			rect2.x[1] = x2_end;
			rect2.x[2] = x2_end;
			rect2.x[3] = x2_begin;

			rect2.y[0] = y2_begin;
			rect2.y[1] = y2_begin;
			rect2.y[2] = y2_end;
			rect2.y[3] = y2_end;
		}

		{
			rect3.x[0] = x3_begin;
			rect3.x[1] = x3_end;
			rect3.x[2] = x3_end;
			rect3.x[3] = x3_begin;

			rect3.y[0] = y3_begin;
			rect3.y[1] = y3_begin;
			rect3.y[2] = y3_end;
			rect3.y[3] = y3_end;
		}

		m_highlited_areas.push_back(rect0);
		m_highlited_areas.push_back(rect1);
		m_highlited_areas.push_back(rect2);
		m_highlited_areas.push_back(rect3);
	}
	else if (x + w >= 1024) {
		uint32_t x0_begin = x;
		uint32_t x0_end = 1024;
		uint32_t y0_begin = y;
		uint32_t y0_end = y + h;

		uint32_t x1_begin = 0;
		uint32_t x1_end = (x + w) & 1023;
		uint32_t y1_begin = y;
		uint32_t y1_end = y + h;

		HighlitArea rect0{};
		HighlitArea rect1{};

		rect0.num_vertices = 4;
		rect1.num_vertices = 4;

		rect0.color = color;
		rect1.color = color;

		rect0.cmd_index = cmd_index;
		rect1.cmd_index = cmd_index;

		rect0.name = fmt::format("{}, no overflow", name);
		rect1.name = fmt::format("{}, x overflow" , name);

		{
			rect0.x[0] = x0_begin;
			rect0.x[1] = x0_end;
			rect0.x[2] = x0_end;
			rect0.x[3] = x0_begin;

			rect0.y[0] = y0_begin;
			rect0.y[1] = y0_begin;
			rect0.y[2] = y0_end;
			rect0.y[3] = y0_end;
		}

		{
			rect1.x[0] = x1_begin;
			rect1.x[1] = x1_end;
			rect1.x[2] = x1_end;
			rect1.x[3] = x1_begin;

			rect1.y[0] = y1_begin;
			rect1.y[1] = y1_begin;
			rect1.y[2] = y1_end;
			rect1.y[3] = y1_end;
		}

		m_highlited_areas.push_back(rect0);
		m_highlited_areas.push_back(rect1);
	}
	else if (y + h >= 512) {
		uint32_t x0_begin = x;
		uint32_t x0_end = x + w;
		uint32_t y0_begin = y;
		uint32_t y0_end = 512;

		uint32_t x1_begin = x;
		uint32_t x1_end = x + w;
		uint32_t y1_begin = 0;
		uint32_t y1_end = (y + h) & 511;

		HighlitArea rect0{};
		HighlitArea rect1{};

		rect0.num_vertices = 4;
		rect1.num_vertices = 4;

		rect0.color = color;
		rect1.color = color;

		rect0.cmd_index = cmd_index;
		rect1.cmd_index = cmd_index;

		rect0.name = fmt::format("{}, no overflow", name);
		rect1.name = fmt::format("{}, y overflow" , name);

		{
			rect0.x[0] = x0_begin;
			rect0.x[1] = x0_end;
			rect0.x[2] = x0_end;
			rect0.x[3] = x0_begin;

			rect0.y[0] = y0_begin;
			rect0.y[1] = y0_begin;
			rect0.y[2] = y0_end;
			rect0.y[3] = y0_end;
		}

		{
			rect1.x[0] = x1_begin;
			rect1.x[1] = x1_end;
			rect1.x[2] = x1_end;
			rect1.x[3] = x1_begin;

			rect1.y[0] = y1_begin;
			rect1.y[1] = y1_begin;
			rect1.y[2] = y1_end;
			rect1.y[3] = y1_end;
		}

		m_highlited_areas.push_back(rect0);
		m_highlited_areas.push_back(rect1);
	}
	else {
		uint32_t x0_begin = x;
		uint32_t x0_end = x + w;
		uint32_t y0_begin = y;
		uint32_t y0_end = y + h;

		HighlitArea rect0{};
		rect0.num_vertices = 4;
		rect0.cmd_index = cmd_index;
		rect0.name = name;
		rect0.color = color;

		{
			rect0.x[0] = x0_begin;
			rect0.x[1] = x0_end;
			rect0.x[2] = x0_end;
			rect0.x[3] = x0_begin;

			rect0.y[0] = y0_begin;
			rect0.y[1] = y0_begin;
			rect0.y[2] = y0_end;
			rect0.y[3] = y0_end;
		}

		m_highlited_areas.push_back(rect0);
	}
}

void DebugView::GpuCommandLoadConfig(psx::GPUCommand const* cmd) {
	using psx::CommandRegister;
	using psx::GP0CommandType;
	using psx::GP1CommandType;
	using psx::EnvCommandType;

	if (cmd->reg == CommandRegister::GP1) {
		//
	}
	else {
		if (cmd->gp0.type == GP0CommandType::ENV) {
			switch (cmd->gp0.env.type)
			{
			case EnvCommandType::SET_DRAW_TOP: {
				m_gpu_saved_conf.x_top = cmd->gp0.env.draw_area.x_coord;
				m_gpu_saved_conf.y_top = cmd->gp0.env.draw_area.y_coord;
			} break;
			case EnvCommandType::SET_DRAW_BOTTOM: {
				m_gpu_saved_conf.x_bot = cmd->gp0.env.draw_area.x_coord;
				m_gpu_saved_conf.y_bot = cmd->gp0.env.draw_area.y_coord;
			} break;
			case EnvCommandType::SET_DRAW_OFFSET: {
				m_gpu_saved_conf.x_off = cmd->gp0.env.draw_offset.x_offset;
				m_gpu_saved_conf.y_off = cmd->gp0.env.draw_offset.y_offset;
			} break;
			default:
				break;
			}
		}
	}
}

void DebugView::GpuCommandAppendClipRect(psx::GPUCommand const* cmd) {
	HighlitArea clip_rect{};
	clip_rect.filled = false;
	clip_rect.num_vertices = 4;
	clip_rect.x[0] = m_gpu_saved_conf.x_top;
	clip_rect.x[1] = m_gpu_saved_conf.x_bot;
	clip_rect.x[2] = m_gpu_saved_conf.x_bot;
	clip_rect.x[3] = m_gpu_saved_conf.x_top;

	clip_rect.y[0] = m_gpu_saved_conf.y_top;
	clip_rect.y[1] = m_gpu_saved_conf.y_top;
	clip_rect.y[2] = m_gpu_saved_conf.y_bot;
	clip_rect.y[3] = m_gpu_saved_conf.y_bot;
	m_highlited_areas.emplace_back(clip_rect);
}

static bool GpuCommandAreaIntersectsRect(uint32_t area_x, uint32_t area_y, uint32_t area_w, uint32_t area_h,
	uint32_t rect_x, uint32_t rect_y, uint32_t rect_w, uint32_t rect_h) {
	uint32_t area_x_l = area_x;
	uint32_t area_x_r = area_x + area_w;

	uint32_t area_y_t = area_y;
	uint32_t area_y_b = area_y + area_h;

	uint32_t rect_x_l = rect_x;
	uint32_t rect_x_r = rect_x + rect_w;
			 
	uint32_t rect_y_t = rect_y;
	uint32_t rect_y_b = rect_y + rect_h;

	//Do proof by contradiction
	//Rightmost verices of area are to the left of the leftmost vertices of the rectangle
	//Leftmost is to the right
	return !(area_x_r < rect_x_l || area_x_l > rect_x_r || area_y_t > rect_y_b || area_y_b < rect_y_t);
}

static bool GpuCommandAreaIntersectsRect2(int32_t area_l, int32_t area_r, int32_t area_t, int32_t area_b,
	int32_t rect_l, int32_t rect_r, int32_t rect_t, int32_t rect_b) {
	//Do proof by contradiction
	//Rightmost verices of area are to the left of the leftmost vertices of the rectangle
	//Leftmost is to the right
	return !(area_r < rect_l || area_l > rect_r || area_t > rect_b || area_b < rect_t);
}

static bool GpuCommandPossiblyOverflowingAreaIntersectsRect(
	uint32_t area_x, uint32_t area_y, uint32_t area_w, uint32_t area_h,
	uint32_t rect_x, uint32_t rect_y, uint32_t rect_w, uint32_t rect_h) {
	//Assume that the 'rect' does not overflow
	if (area_x + area_w <= 1024 && area_y + area_h <= 512) {
		return GpuCommandAreaIntersectsRect(area_x, area_y, area_w, area_h,
			rect_x, rect_y, rect_w, rect_h);
	}
	else if (area_x + area_w > 1024 && area_y + area_h > 512) {
		auto step1 = GpuCommandAreaIntersectsRect(area_x, area_y, 1024 - area_x, 512 - area_y,
			rect_x, rect_y, rect_w, rect_h);
		auto step2 = GpuCommandAreaIntersectsRect(0, 0, (area_x + area_w) & 0x3FF, (area_y + area_h) & 0x1FF,
			rect_x, rect_y, rect_w, rect_h);
		auto step3 = GpuCommandAreaIntersectsRect(0, area_y, (area_x + area_w) & 0x3FF, 512 - area_y,
			rect_x, rect_y, rect_w, rect_h);
		auto step4 = GpuCommandAreaIntersectsRect(area_x, 0, 1024 - area_x, (area_y + area_h) & 0x1FF,
			rect_x, rect_y, rect_w, rect_h);
		return step1 || step2 || step3 || step4;
	}
	else if (area_x + area_w > 1024) {
		auto step1 = GpuCommandAreaIntersectsRect(area_x, area_y, 1024 - area_x, area_h,
			rect_x, rect_y, rect_w, rect_h);
		auto step2 = GpuCommandAreaIntersectsRect(0, area_y, (area_x + area_w) & 0x3FF, area_h,
			rect_x, rect_y, rect_w, rect_h);
		return step1 || step2;
	} 
	else {
		auto step1 = GpuCommandAreaIntersectsRect(area_x, area_y, area_w, 512 - area_y,
			rect_x, rect_y, rect_w, rect_h);
		auto step2 = GpuCommandAreaIntersectsRect(area_x, 0, area_w, (area_y + area_h) & 0x1FF,
			rect_x, rect_y, rect_w, rect_h);
		return step1 || step2;
	}
}

std::pair<bool, bool> DebugView::GetGpuCommandAccessesVramArea(psx::GPUCommand const* cmd, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
	if (cmd->reg == psx::CommandRegister::GP1) {
		return { false, false };
	}

	using GP0Command = psx::GP0CommandType;
	switch (cmd->gp0.type)
	{
	case GP0Command::MISC: {
		switch (cmd->gp0.misc.type)
		{
		case psx::MiscCommandType::NOP:
		case psx::MiscCommandType::NOP_FIFO:
		case psx::MiscCommandType::CLEAR_CACHE:
			return { false, false };
		case psx::MiscCommandType::QUICK_FILL:
		{
			auto fill_x = cmd->params.quick_fill.x;
			auto fill_y = cmd->params.quick_fill.y;
			auto fill_w = cmd->params.quick_fill.w;
			auto fill_h = cmd->params.quick_fill.h;
			return { false, GpuCommandPossiblyOverflowingAreaIntersectsRect(
				fill_x, fill_y, fill_w, fill_h,
				x, y, w, h
			) };
		}
		}
	} break;
	case GP0Command::POLYGON:
		return GetGpuPolygonAccessesVramArea(cmd, x, y, w, h);
	case GP0Command::LINE:
		return GetGpuLineAccessesVramArea(cmd, x, y, w, h);
	case GP0Command::RECTANGLE:
		return GetGpuRectangleAccessesVramArea(cmd, x, y, w, h);
	case GP0Command::POLYLINE_END:
		return { false, false };
	case GP0Command::VRAM_BLIT: {
		auto src_x = cmd->params.vram_vram_blit.src_x;
		auto src_y = cmd->params.vram_vram_blit.src_y;
		auto dst_x = cmd->params.vram_vram_blit.dst_x;
		auto dst_y = cmd->params.vram_vram_blit.dst_y;
		auto blit_w = cmd->params.vram_vram_blit.w;
		auto blit_h = cmd->params.vram_vram_blit.h;
		return
		{
			GpuCommandPossiblyOverflowingAreaIntersectsRect(src_x, src_y, blit_w, blit_h, x, y, w, h),
			GpuCommandPossiblyOverflowingAreaIntersectsRect(dst_x, dst_y, blit_w, blit_h, x, y, w, h)
		};
	}
	case GP0Command::CPU_VRAM_BLIT: {
		auto blit_x = cmd->params.cpu_vram_blit.dst_x;
		auto blit_y = cmd->params.cpu_vram_blit.dst_y;
		auto blit_w = cmd->params.cpu_vram_blit.w;
		auto blit_h = cmd->params.cpu_vram_blit.h;
		return { 
			false,
			GpuCommandPossiblyOverflowingAreaIntersectsRect(
			blit_x, blit_y, blit_w, blit_h,
			x, y, w, h)
		 };
	}
	case GP0Command::VRAM_CPU_BLIT: {
		auto blit_x = cmd->params.vram_cpu_blit.src_x;
		auto blit_y = cmd->params.vram_cpu_blit.src_y;
		auto blit_w = cmd->params.vram_cpu_blit.w;
		auto blit_h = cmd->params.vram_cpu_blit.h;
		return { GpuCommandPossiblyOverflowingAreaIntersectsRect(
			blit_x, blit_y, blit_w, blit_h, 
			x, y, w, h)
			, false };
	}
	case GP0Command::ENV:
		return { false, false };
	}

	return { false, false };
}

std::pair<bool, bool> DebugView::GetGpuRectangleAccessesVramArea(psx::GPUCommand const* cmd, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
	auto rect_cmd = cmd->gp0.rect;

	uint32_t color_depth = {};
	uint32_t x_base = {};
	uint32_t y_base = {};
	uint32_t clut_x = {};
	uint32_t clut_y = {};
	if (rect_cmd.is_textured()) {
		auto texpage = cmd->params.rendering.vertices[0].clut_page & 0xFFFF;
		color_depth = (texpage >> 7) & 0x3;
		x_base = (texpage & 0xF) * 64;
		y_base = (((texpage >> 4) & 1) | (((texpage >> 11) & 1) << 1)) * 256;
		auto clut = (cmd->params.rendering.vertices[0].clut_page >> 16) & 0xFFFF;
		clut_x = (clut & 0x3F) * 16;
		clut_y = (clut >> 6) & 0x1FF;
	}

	/*
	texpage |= gpu_stat.texture_page_x_base;
	texpage |= ((u16)gpu_stat.texture_page_y_base << 4);
	texpage |= ((u16)gpu_stat.semi_transparency << 5);
	texpage |= ((u16)gpu_stat.tex_page_colors << 7);
	texpage |= ((u16)gpu_stat.texture_page_y_base2 << 11);

	clut: 0-5 x coord, 6-14 y coord

	tex_and_clut = (clut << 16) | texpage;
	*/
	int32_t draw_x[4] = {};
	int32_t draw_y[4] = {};
	int32_t tex_u[4] = {};
	int32_t tex_v[4] = {};

	for (size_t vertex_index = 0; vertex_index < 4; vertex_index++) {
		auto const& vertex = cmd->params.rendering.vertices[vertex_index];

		draw_x[vertex_index] = vertex.x;
		draw_y[vertex_index] = vertex.y;

		if (rect_cmd.is_textured()) {
			uint32_t u = cmd->params.rendering.vertices[vertex_index].u;
			uint32_t v = cmd->params.rendering.vertices[vertex_index].v;
			u += x_base;
			v += y_base;
			tex_u[vertex_index] = (int32_t)u;
			tex_v[vertex_index] = (int32_t)v;
		}
	}

	std::swap(draw_y[1], draw_y[2]);
	
	bool write = GpuCommandAreaIntersectsRect2(draw_x[0], draw_x[2], draw_y[0], draw_y[2],
		x, x + w, y, y + h);
	bool read = false;

	if (rect_cmd.is_textured()) {
		//read = GpuCommandAreaIntersectsRect2(tex_u[0], tex_u[2], tex_v[0], tex_v[1],
		//	x, x + w, y, y + h);
		constexpr uint32_t BPP_DIVISOR[] = { 4, 2, 1, 1 };
		read = GpuCommandPossiblyOverflowingAreaIntersectsRect(tex_u[0], tex_v[0], 
			std::abs(tex_u[2] - tex_u[0]) / BPP_DIVISOR[color_depth],
			std::abs(tex_v[1] - tex_v[0]), 
			x, y, w, h);
		if (color_depth != 2) {
			int32_t clut_len = color_depth == 0 ? 16 : 256;
			read = read || GpuCommandAreaIntersectsRect2(clut_x, clut_x + clut_len, clut_y, clut_y + 1,
				x, x + w, y, y + h);
		}
	}

	return { read, write };
}

//Copied straight from https://www.jeffreythompson.org/collision-detection/line-rect.php

// LINE/LINE
static bool LineIntersectLine(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4) {

	// calculate the direction of the lines
	float uA = ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / ((y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1));
	float uB = ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / ((y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1));

	// if uA and uB are between 0-1, lines are colliding
	return (uA >= 0 && uA <= 1 && uB >= 0 && uB <= 1);
}

// LINE/RECTANGLE
static bool LineIntersectsRect(float x1, float y1, float x2, float y2, float rx, float ry, float rw, float rh) {

	// check if the line has hit any of the rectangle's sides
	// uses the Line/Line function below
	bool left = LineIntersectLine(x1, y1, x2, y2, rx, ry, rx, ry + rh);
	bool right = LineIntersectLine(x1, y1, x2, y2, rx + rw, ry, rx + rw, ry + rh);
	bool top = LineIntersectLine(x1, y1, x2, y2, rx, ry, rx + rw, ry);
	bool bottom = LineIntersectLine(x1, y1, x2, y2, rx, ry + rh, rx + rw, ry + rh);

	// if ANY of the above are true, the line
	// has hit the rectangle
	if (left || right || top || bottom) {
		return true;
	}
	return false;
}

std::pair<bool, bool> DebugView::GetGpuLineAccessesVramArea(psx::GPUCommand const* cmd, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
	
	int32_t x0 = cmd->params.rendering.vertices[0].x;
	int32_t y0 = cmd->params.rendering.vertices[0].y;
	int32_t x1 = cmd->params.rendering.vertices[1].x;
	int32_t y1 = cmd->params.rendering.vertices[1].y;

	//Rectangle contains either extrema
	if (GpuCommandAreaIntersectsRect(x, y, w, h, x0, y0, 1, 1) ||
		GpuCommandAreaIntersectsRect(x, y, w, h, x1, y1, 1, 1)) {
		return { false, true };
	}
	
	bool writes = LineIntersectsRect((float)x0, (float)y0, (float)x1, (float)y1, (float)x, (float)y,
		(float)w, (float)h);
	return { false, writes };
}

std::pair<bool, bool> DebugView::GetGpuPolygonAccessesVramArea(psx::GPUCommand const* cmd, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
	auto polygon_cmd = cmd->gp0.polygon;

	uint32_t color_depth = {};
	uint32_t x_base = {};
	uint32_t y_base = {};
	uint32_t clut_x = {};
	uint32_t clut_y = {};
	if (polygon_cmd.is_textured()) {
		auto texpage = cmd->params.rendering.vertices[0].clut_page & 0xFFFF;
		color_depth = (texpage >> 7) & 0x3;
		x_base = (texpage & 0xF) * 64;
		y_base = (((texpage >> 4) & 1) | (((texpage >> 11) & 1) << 1)) * 256;
		auto clut = (cmd->params.rendering.vertices[0].clut_page >> 16) & 0xFFFF;
		clut_x = (clut & 0x3F) * 16;
		clut_y = (clut >> 6) & 0x1FF;
	}

	int32_t vertex_x[4] = {};
	int32_t vertex_y[4] = {};
	int32_t tex_u[4] = {};
	int32_t tex_v[4] = {};

	constexpr uint32_t BPP_DIVISOR[] = { 4, 2, 1, 1 };
	for (size_t vertex_index = 0; vertex_index < (polygon_cmd.is_quad() ? 4 : 3); vertex_index++) {
		auto const& vertex = cmd->params.rendering.vertices[vertex_index];

		vertex_x[vertex_index] = vertex.x;
		vertex_y[vertex_index] = vertex.y;

		if (polygon_cmd.is_textured()) {
			uint32_t u = cmd->params.rendering.vertices[vertex_index].u / BPP_DIVISOR[color_depth];
			uint32_t v = cmd->params.rendering.vertices[vertex_index].v;
			u += x_base;
			v += y_base;
			tex_u[vertex_index] = u;
			tex_v[vertex_index] = v;
		}
	}

	bool writes = ImTriangleContainsPoint(
		ImVec2((float)vertex_x[0], (float)vertex_y[0]),
		ImVec2((float)vertex_x[1], (float)vertex_y[1]),
		ImVec2((float)vertex_x[2], (float)vertex_y[2]),
		ImVec2((float)x, (float)y));
	writes = writes || 
		ImTriangleContainsPoint(
			ImVec2((float)vertex_x[0], (float)vertex_y[0]),
			ImVec2((float)vertex_x[1], (float)vertex_y[1]),
			ImVec2((float)vertex_x[2], (float)vertex_y[2]),
			ImVec2((float)x + w, (float)y)) ||
		ImTriangleContainsPoint(
			ImVec2((float)vertex_x[0], (float)vertex_y[0]),
			ImVec2((float)vertex_x[1], (float)vertex_y[1]),
			ImVec2((float)vertex_x[2], (float)vertex_y[2]),
			ImVec2((float)x, (float)y + h)) ||
		ImTriangleContainsPoint(
			ImVec2((float)vertex_x[0], (float)vertex_y[0]),
			ImVec2((float)vertex_x[1], (float)vertex_y[1]),
			ImVec2((float)vertex_x[2], (float)vertex_y[2]),
			ImVec2((float)x + w, (float)y + h));

	if (polygon_cmd.is_quad()) {
		writes = writes || 
			ImTriangleContainsPoint(
				ImVec2((float)vertex_x[1], (float)vertex_y[1]),
				ImVec2((float)vertex_x[2], (float)vertex_y[2]),
				ImVec2((float)vertex_x[3], (float)vertex_y[3]),
				ImVec2((float)x, (float)y)) ||
			ImTriangleContainsPoint(
				ImVec2((float)vertex_x[1], (float)vertex_y[1]),
				ImVec2((float)vertex_x[2], (float)vertex_y[2]),
				ImVec2((float)vertex_x[3], (float)vertex_y[3]),
				ImVec2((float)x + w, (float)y)) ||
			ImTriangleContainsPoint(
				ImVec2((float)vertex_x[1], (float)vertex_y[1]),
				ImVec2((float)vertex_x[2], (float)vertex_y[2]),
				ImVec2((float)vertex_x[3], (float)vertex_y[3]),
				ImVec2((float)x, (float)y + h)) ||
			ImTriangleContainsPoint(
				ImVec2((float)vertex_x[1], (float)vertex_y[1]),
				ImVec2((float)vertex_x[2], (float)vertex_y[2]),
				ImVec2((float)vertex_x[3], (float)vertex_y[3]),
				ImVec2((float)x + w, (float)y + h));
	}

	bool read = false;

	if (polygon_cmd.is_textured()) {
		read = ImTriangleContainsPoint(
				ImVec2((float)tex_u[0], (float)tex_v[0]),
				ImVec2((float)tex_u[1], (float)tex_v[1]),
				ImVec2((float)tex_u[2], (float)tex_v[2]),
				ImVec2((float)x, (float)y)) ||
			ImTriangleContainsPoint(
				ImVec2((float)tex_u[0], (float)tex_v[0]),
				ImVec2((float)tex_u[1], (float)tex_v[1]),
				ImVec2((float)tex_u[2], (float)tex_v[2]),
				ImVec2((float)x + w, (float)y)) ||
			ImTriangleContainsPoint(
				ImVec2((float)tex_u[0], (float)tex_v[0]),
				ImVec2((float)tex_u[1], (float)tex_v[1]),
				ImVec2((float)tex_u[2], (float)tex_v[2]),
				ImVec2((float)x, (float)y + h)) ||
			ImTriangleContainsPoint(
				ImVec2((float)tex_u[0], (float)tex_v[0]),
				ImVec2((float)tex_u[1], (float)tex_v[1]),
				ImVec2((float)tex_u[2], (float)tex_v[2]),
				ImVec2((float)x + w, (float)y + h));

		if (polygon_cmd.is_quad()) {
			writes = writes ||
				ImTriangleContainsPoint(
					ImVec2((float)tex_u[1], (float)tex_v[1]),
					ImVec2((float)tex_u[2], (float)tex_v[2]),
					ImVec2((float)tex_u[3], (float)tex_v[3]),
					ImVec2((float)x, (float)y)) ||
				ImTriangleContainsPoint(
					ImVec2((float)tex_u[1], (float)tex_v[1]),
					ImVec2((float)tex_u[2], (float)tex_v[2]),
					ImVec2((float)tex_u[3], (float)tex_v[3]),
					ImVec2((float)x + w, (float)y)) ||
				ImTriangleContainsPoint(
					ImVec2((float)tex_u[1], (float)tex_v[1]),
					ImVec2((float)tex_u[2], (float)tex_v[2]),
					ImVec2((float)tex_u[3], (float)tex_v[3]),
					ImVec2((float)x, (float)y + h)) ||
				ImTriangleContainsPoint(
					ImVec2((float)tex_u[1], (float)tex_v[1]),
					ImVec2((float)tex_u[2], (float)tex_v[2]),
					ImVec2((float)tex_u[3], (float)tex_v[3]),
					ImVec2((float)x + w, (float)y + h));
		}
		
		if (color_depth != 2) {
			int32_t clut_len = color_depth == 0 ? 16 : 256;
			read = read || GpuCommandAreaIntersectsRect2(clut_x, clut_x + clut_len, clut_y, clut_y + 1,
				x, x + w, y, y + h);
		}
	}

	return { read, writes };
}

void DebugView::GpuWindow() {
	if (!g_show_gpustat_window) {
		return;
	}

	if (!ImGui::Begin("GPU", &g_show_gpustat_window)) {
		ImGui::End();
		return;
	}

	auto& gpu = m_psx->GetStatus()
		.sysbus->m_gpu;

	ImGui::Text("Currently in VBlank   : %d", gpu.m_vblank);
	ImGui::Text("Current scanline      : %d", gpu.m_scanline);
	ImGui::Text("Current CMD FIFO size : %d", gpu.m_cmd_fifo.len());
	if (ImGui::Button("Reset fifo")) {
		gpu.ResetFifo();
	}
	ImGui::Text("Current status        : %s", magic_enum::enum_name(gpu.m_cmd_status).data());
	ImGui::Text("Read mode             : %s", magic_enum::enum_name(gpu.m_read_status).data());
	ImGui::Text("Read latch            : 0x%08X", gpu.m_gpu_read_latch);

	ImGui::BeginTabBar("##stat");

	if (ImGui::BeginTabItem("Status")) {
		auto& gpustat = gpu.m_stat;
		uint32_t x_base = gpustat.texture_page_x_base * 64;
		ImGui::SetNextItemWidth(150.f);
		ImGui::InputScalar("Texture X page", ImGuiDataType_U32, (void*)&x_base);
		gpustat.texture_page_x_base = std::min<uint32_t>(x_base, 960) / 64;

		uint32_t y_base = gpustat.texture_page_y_base * 256;
		ImGui::SetNextItemWidth(150.f);
		ImGui::InputScalar("Texture Y page", ImGuiDataType_U32, (void*)&y_base);
		gpustat.texture_page_y_base = std::min<uint32_t>(y_base, 256) / 256;

		constexpr const char* SEMI_TRANSPARENCIES[] = { "B/2+F/2", "B+F", "B-F", "B+F/4" };
		auto semi_transparency = int(gpustat.semi_transparency);
		ImGui::SetNextItemWidth(150.f);
		ImGui::Combo("Semi-transparency", &semi_transparency, SEMI_TRANSPARENCIES, IM_ARRAYSIZE(SEMI_TRANSPARENCIES));
		gpustat.semi_transparency = psx::SemiTransparency(semi_transparency);

		constexpr const char* BPP[] = { "4BPP", "8BPP", "15BPP", "RESERVED" };
		auto curr_bpp = int(gpustat.tex_page_colors);
		ImGui::SetNextItemWidth(150.f);
		ImGui::Combo("Texture BPP", &curr_bpp, BPP, IM_ARRAYSIZE(BPP));
		gpustat.tex_page_colors = psx::TexPageColors(curr_bpp);

		ImGui::Checkbox("Dither",          &gpustat.dither);
		ImGui::Checkbox("Draw to display", &gpustat.draw_to_display);
		ImGui::Checkbox("Set mask",        &gpustat.set_mask);
		ImGui::Checkbox("Mask enable",     &gpustat.draw_over_mask_disable);
		ImGui::Checkbox("Interlace field", &gpustat.interlace_field);
		ImGui::Checkbox("Screen flip X",   &gpustat.flip_screen_hoz);
		ImGui::Checkbox("Tex Y page 2",    &gpustat.texture_page_y_base2);
		ImGui::Checkbox("Enable interlace",&gpustat.vertical_interlace);
		ImGui::Checkbox("Display enable",  &gpustat.disp_enable);
		ImGui::Checkbox("IRQ1",            &gpustat.irq1);
		ImGui::Checkbox("Dreq",            &gpustat.dreq);
		ImGui::Checkbox("Recv cmd word",   &gpustat.recv_cmd_word);
		ImGui::Checkbox("Ready for VRAM -> CPU", &gpustat.send_vram_cpu);
		ImGui::Checkbox("Recv DMA",        &gpustat.recv_dma);

		constexpr const char* DMA_DIR[] = { "OFF", "INVALID", "CPU->GPU", "GPU->CPU" };
		auto dma_dir = int(gpustat.dma_dir);
		ImGui::SetNextItemWidth(150.f);
		ImGui::Combo("DMA direction", &dma_dir, DMA_DIR, IM_ARRAYSIZE(DMA_DIR));
		gpustat.dma_dir = psx::DmaDir(dma_dir);

		ImGui::Checkbox("Drawing odd",     &gpustat.drawing_odd);

		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Non-stat")) {
		constexpr const char* VIDEO_MODES[] = { "NTSC", "PAL"};
		auto video_mode = int(gpu.m_video_mode);
		ImGui::SetNextItemWidth(150.f);
		ImGui::Combo("Video mode", &video_mode, VIDEO_MODES, IM_ARRAYSIZE(VIDEO_MODES));
		gpu.m_video_mode = psx::ConsoleVideoMode(video_mode);

		ImGui::Checkbox("Rectangle texture X flip", &gpu.m_tex_x_flip);
		ImGui::Checkbox("Rectangle texture Y flip", &gpu.m_tex_y_flip);

		ImGui::SetNextItemWidth(150.f);
		ImGui::InputScalar("Mask X size", ImGuiDataType_U32, (void*)&gpu.m_tex_win.mask_x, 0, 0,
			0, ImGuiInputTextFlags_ReadOnly);
		ImGui::SetNextItemWidth(150.f);
		ImGui::InputScalar("Mask Y size", ImGuiDataType_U32, (void*)&gpu.m_tex_win.mask_y, 0, 0,
			0, ImGuiInputTextFlags_ReadOnly);
		ImGui::SetNextItemWidth(150.f);
		ImGui::InputScalar("Mask X offset", ImGuiDataType_U32, (void*)&gpu.m_tex_win.offset_x, 0, 0,
			0, ImGuiInputTextFlags_ReadOnly);
		ImGui::SetNextItemWidth(150.f);
		ImGui::InputScalar("Mask Y offset", ImGuiDataType_U32, (void*)&gpu.m_tex_win.offset_y, 0, 0,
			0, ImGuiInputTextFlags_ReadOnly);

		ImGui::SetNextItemWidth(150.f);
		ImGui::InputScalar("Horizontal resolution", ImGuiDataType_U32, (void*)&gpu.m_disp_conf.hoz_res, 0, 0,
			0, ImGuiInputTextFlags_ReadOnly);
		ImGui::SetNextItemWidth(150.f);
		ImGui::InputScalar("Vertical resolution", ImGuiDataType_U32, (void*)&gpu.m_disp_conf.vert_res, 0, 0,
			0, ImGuiInputTextFlags_ReadOnly);

		ImGui::SetNextItemWidth(150.f);
		ImGui::InputScalar("Draw area top left X", ImGuiDataType_U32, (void*)&gpu.m_x_top_left, 0, 0,
			0, ImGuiInputTextFlags_ReadOnly);
		ImGui::SetNextItemWidth(150.f);
		ImGui::InputScalar("Draw area top left Y", ImGuiDataType_U32, (void*)&gpu.m_y_top_left, 0, 0,
			0, ImGuiInputTextFlags_ReadOnly);
		ImGui::SetNextItemWidth(150.f);
		ImGui::InputScalar("Draw area bottom right X", ImGuiDataType_U32, (void*)&gpu.m_x_bot_right, 0, 0,
			0, ImGuiInputTextFlags_ReadOnly);
		ImGui::SetNextItemWidth(150.f);
		ImGui::InputScalar("Draw area bottom right Y", ImGuiDataType_U32, (void*)&gpu.m_y_bot_right, 0, 0,
			0, ImGuiInputTextFlags_ReadOnly);

		ImGui::SetNextItemWidth(150.f);
		ImGui::InputScalar("Draw offset X", ImGuiDataType_U32, (void*)&gpu.m_x_off, 0, 0,
			0, ImGuiInputTextFlags_ReadOnly);
		ImGui::SetNextItemWidth(150.f);
		ImGui::InputScalar("Draw offset Y", ImGuiDataType_U32, (void*)&gpu.m_y_off, 0, 0,
			0, ImGuiInputTextFlags_ReadOnly);

		ImGui::SetNextItemWidth(150.f);
		ImGui::InputScalar("Display start X", ImGuiDataType_U32, (void*)&gpu.m_disp_x_start, 0, 0,
			0, ImGuiInputTextFlags_ReadOnly);
		ImGui::SetNextItemWidth(150.f);
		ImGui::InputScalar("Display start Y", ImGuiDataType_U32, (void*)&gpu.m_disp_y_start, 0, 0,
			0, ImGuiInputTextFlags_ReadOnly);

		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Command fifo")) {
		ImGui::Text("Current size: %d", gpu.m_cmd_fifo.len());
		ImGui::Text("Total bytes : %d", gpu.m_cmd_fifo.len() * sizeof(uint32_t));
		ImGui::Separator();
		if (gpu.m_cmd_fifo.len() > 0) {
			ImGuiListClipper clipper{};
			clipper.Begin((int)gpu.m_cmd_fifo.len());
			while (clipper.Step()) {
				for (int curr_index = clipper.DisplayStart; curr_index < clipper.DisplayEnd; 
					curr_index++) {
					ImGui::Text("Entry %d: %d", curr_index, *(gpu.m_cmd_fifo.begin() + curr_index));
				}
			}
		}
		else {
			ImGui::Text("No entries");
		}	

		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();

	ImGui::End();
}