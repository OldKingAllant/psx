#include "DebugView.hpp"

#include <thirdparty/imgui/imgui.h>
#include <thirdparty/imgui/imgui_internal.h>
#include <thirdparty/magic_enum/include/magic_enum/magic_enum.hpp>
#include <thirdparty/ImGuiFileDialog/ImGuiFileDialog.h>
#include <thirdparty/stb/stb_image_write.h>

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

void DebugView::GpuCommandWindow() {
	ImGui::Begin("GPU Commands");

	auto& gpu = m_psx->GetStatus().sysbus->GetGPU();

	bool record_commands{ gpu.GetRecordingCommands() };
	if (ImGui::Checkbox("Record commands", &record_commands)) {
		gpu.SetRecordingCommands(record_commands);
	}

	ImGui::SetNextItemWidth(100.f);
	ImGui::SameLine();
	ImGui::InputInt("Frames to record", (int*)&gpu.m_frames_to_record);

	auto cursor_pos = ImGui::GetCursorPos();

	if (record_commands) {
		auto const& commands = gpu.GetRecordedCommands();

		for (auto const& cmd : commands) {
			GpuCommandLoadConfig(&cmd);
		}

		ImGuiListClipper clipper{};
		clipper.Begin((int)commands.size());

		while (clipper.Step()) {
			for (size_t cmd_index = clipper.DisplayStart; cmd_index < clipper.DisplayEnd; cmd_index++) {
				if (cmd_index >= commands.size()) {
					break;
				}
				auto const& cmd = commands[cmd_index];
				auto has_details = GetGpuCommandHasDetails(&cmd);
				auto is_open = ShowGpuCommandEntry(cmd_index, &cmd, has_details);
				auto is_hovered = ImGui::IsItemHovered();

				if (is_open || is_hovered) {
					GpuCommandAppendVramAreas(&cmd, cmd_index);
				}

				if (is_open && has_details) {
					ShowGpuCommandDetails(&cmd, cmd_index, false);
					ImGui::Separator();
				}
				else if(is_hovered && has_details) {
					ImGui::BeginTooltip();
					ShowGpuCommandDetails(&cmd, cmd_index, true);
					ImGui::EndTooltip();
				}
			}
		}
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
	auto new_item_spacing = ImVec2(old_item_spacing.x, 30.f);

	const auto Y_SIZE = 512.f + ImGui::GetFrameHeightWithSpacing() + new_item_spacing.y;

	ImGui::SetNextWindowSizeConstraints(ImVec2(1024.f, Y_SIZE), ImVec2(1024.f, Y_SIZE));
	ImGui::SetNextWindowSize(ImVec2(1024.f, Y_SIZE));

	ImGui::Begin("VRAM", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

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
				blink_color);
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

	for (size_t vertex_index = 0; vertex_index < (polygon_cmd.is_quad() ? 4 : 3); vertex_index++) {
		auto const& vertex = cmd->params.rendering.vertices[vertex_index];
		ImGui::Text("X%d: %d, Y%d: %d", vertex_index, vertex.x, vertex_index, vertex.y);

		area1.x[vertex_index] = vertex.x;
		area1.y[vertex_index] = vertex.y;

		if (polygon_cmd.is_textured()) {
			uint32_t u = cmd->params.rendering.vertices[vertex_index].u;
			uint32_t v = cmd->params.rendering.vertices[vertex_index].v;
			u += x_base;
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

	for (size_t vertex_index = 0; vertex_index < 4; vertex_index++) {
		auto const& vertex = cmd->params.rendering.vertices[vertex_index];
		ImGui::Text("X%d: %d, Y%d: %d", vertex_index, vertex.x, vertex_index, vertex.y);

		area.x[vertex_index] = vertex.x;
		area.y[vertex_index] = vertex.y;

		if (rect_cmd.is_textured()) {
			uint32_t u = cmd->params.rendering.vertices[vertex_index].u;
			uint32_t v = cmd->params.rendering.vertices[vertex_index].v;
			u += x_base;
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

	glGetTextureSubImage(vram_handle, 0, x, y, 0, w, h, 1, GL_RGBA,
		GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, compressed_texture_size_bytes,
		(void*)compressed.data());

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

		for (size_t vertex_index = 0; vertex_index < 4; vertex_index++) {
			auto const& vertex = cmd->params.rendering.vertices[vertex_index];
			ImGui::Text("X%d: %d, Y%d: %d", vertex_index, vertex.x, vertex_index, vertex.y);

			x[vertex_index] = vertex.x;
			y[vertex_index] = vertex.y;
			
			uint32_t u = cmd->params.rendering.vertices[vertex_index].u;
			uint32_t v = cmd->params.rendering.vertices[vertex_index].v;

			view_u[vertex_index] = u;
			view_v[vertex_index] = v;

			u += x_base;
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

		auto begin = ImGui::GetCursorScreenPos();
		begin.y += 20.f;

		auto draw_list = ImGui::GetWindowDrawList();
		auto viewport_size = m_win->GetSize();
		
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
			ImVec2(begin.x / viewport_size.w, begin.y / viewport_size.h),
			ImVec2((begin.x + size_x * s_scale) / viewport_size.w, (begin.y + size_y * s_scale) / viewport_size.h),
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

void DebugView::GpuWindow() {
	if (!ImGui::Begin("GPU")) {
		ImGui::End();
		return;
	}

	auto& gpu = m_psx->GetStatus()
		.sysbus->m_gpu;

	ImGui::Text("Currently in VBlank : %d", gpu.m_vblank);
	ImGui::Text("Current scanline : %d", gpu.m_scanline);
	ImGui::Text("Current CMD FIFO size : %d", gpu.m_cmd_fifo.len());

	auto curr_cmd_status = magic_enum::enum_name(gpu.m_cmd_status);

	ImGui::Text("Command mode : %s", curr_cmd_status.data());

	auto curr_read_stat = magic_enum::enum_name(gpu.m_read_status);

	ImGui::Text("Read mode : %s", curr_cmd_status.data());
	ImGui::Text("Read latch : %08X", gpu.m_gpu_read_latch);

	ImGui::BeginTabBar("##stat");

	if (ImGui::BeginTabItem("Status")) {
		auto& gpustat = gpu.m_stat;
		ImGui::Text("Tex X page : %d", (int)gpustat.texture_page_x_base * 64);
		ImGui::Text("Tex Y page : %d", (int)gpustat.texture_page_y_base * 256);

		auto semi_trans = magic_enum::enum_name(gpustat.semi_transparency);
		ImGui::Text("Semi transparency : %s", semi_trans.data());
		auto texpage_col = magic_enum::enum_name(gpustat.tex_page_colors);
		ImGui::Text("Texpage colors : %s", texpage_col.data());

		ImGui::Text("Dither          : %d", gpustat.dither);
		ImGui::Text("Draw to display : %d", gpustat.draw_to_display);
		ImGui::Text("Set mask        : %d", gpustat.set_mask);
		ImGui::Text("Mask enable     : %d", gpustat.draw_over_mask_disable);
		ImGui::Text("Interlace field : %d", gpustat.interlace_field);
		ImGui::Text("Flip H          : %d", gpustat.flip_screen_hoz);
		ImGui::Text("Tex Y page 2    : %d", (int)gpustat.texture_page_y_base2 * 512);
		ImGui::Text("Vertical interlace : %d", gpustat.vertical_interlace);
		ImGui::Text("Display enable  : %d", gpustat.disp_enable);
		ImGui::Text("IRQ1            : %d", gpustat.irq1);
		ImGui::Text("Dreq            : %d", gpustat.dreq);
		ImGui::Text("Recv cmd word   : %d", gpustat.recv_cmd_word);
		ImGui::Text("Ready for VRAM -> CPU  : %d", gpustat.send_vram_cpu);
		ImGui::Text("Recv DMA        : %d", gpustat.recv_dma);

		auto dma_dir = magic_enum::enum_name(gpustat.dma_dir);
		ImGui::Text("Dma direction   : %s", dma_dir.data());
		ImGui::Text("Drawing odd     : %d", gpustat.drawing_odd);

		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Tex window")) {
		ImGui::Text("Mask X : %d", gpu.m_tex_win.mask_x);
		ImGui::Text("Mask Y : %d", gpu.m_tex_win.mask_y);
		ImGui::Text("Offset X : %d", gpu.m_tex_win.offset_x);
		ImGui::Text("Offset Y : %d", gpu.m_tex_win.offset_y);
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Other data")) {
		ImGui::Text("Horizontal resolution : %d", gpu.m_disp_conf.hoz_res);
		ImGui::Text("Vertical resolution   : %d", gpu.m_disp_conf.vert_res);
		ImGui::Text("Draw area top left X : %d, Y : %d",
			gpu.m_x_top_left, gpu.m_y_top_left);
		ImGui::Text("Draw area bottom right X : %d, Y : %d",
			gpu.m_x_bot_right, gpu.m_y_bot_right);
		ImGui::Text("Draw offset X : %d", gpu.m_x_off);
		ImGui::Text("Draw offset Y : %d", gpu.m_y_off);
		ImGui::Text("Display start X : %d", gpu.m_disp_x_start);
		ImGui::Text("Display start Y : %d", gpu.m_disp_y_start);
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();

	ImGui::End();
}