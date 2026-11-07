#include "DebugView.hpp"

#include <functional>
#include <optional>
#include <fstream>
#include <unordered_map>

#include <thirdparty/imgui/imgui.h>
#include <thirdparty/ImGuiFileDialog/ImGuiFileDialog.h>

#include <common/Defs.hpp>
#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/include/psxemu/SPU.hpp>
#include <psxemu/include/psxemu/System.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>

#include <fmt/format.h>

void DebugView::MemoryEditor() {
	constexpr const char* REGION_NAMES[] = {
		"RAM",
		"SCRATCHPAD",
		"BIOS",
		"VRAM",
		"SPU_RAM"
	};

	constexpr bool REGION_HAS_ADDRESS[] = { true, true, true, false, false };

	constexpr bool IS_REGION_WRITEABLE[] = { true, true, false, true, true };

	constexpr psx::u32 REGION_STARTS[] = {
		psx::memory::region_offsets::PSX_MAIN_RAM_OFFSET,
		psx::memory::region_offsets::PSX_SCRATCHPAD_OFFSET,
		psx::memory::region_offsets::PSX_BIOS_OFFSET,
		0,
		0
	};

	constexpr psx::u32 REGION_SIZES[] = {
		psx::memory::region_sizes::PSX_MAIN_RAM_SIZE,
		psx::memory::region_sizes::PSX_SCRATCHPAD_PADDED_SIZE >> 2,
		psx::memory::region_sizes::PSX_BIOS_SIZE,
		psx::VRAM_SIZE,
		psx::SPU::RAM_SIZE
	};

	auto sysbus = m_psx->GetStatus().sysbus;

	const std::function<psx::u8(psx::u32)> REGION_READ_FUNCS[] = {
		[sysbus, REGION_STARTS](psx::u32 _address) { return sysbus->m_guest_base[REGION_STARTS[0] + _address]; },
		[sysbus, REGION_STARTS](psx::u32 _address) { return sysbus->m_guest_base[REGION_STARTS[1] + _address]; },
		[sysbus, REGION_STARTS](psx::u32 _address) { return sysbus->m_guest_base[REGION_STARTS[2] + _address]; },
		[sysbus, REGION_STARTS](psx::u32 _address) { return 0; }, //TODO: This requires implementing GPU dirty memory tracking
		[sysbus, REGION_STARTS](psx::u32 _address) { return sysbus->GetSPU().m_sound_ram[_address]; }
	};

	const std::function<void(psx::u32, psx::u8)> REGION_WRITE_FUNCS[] = {
		[sysbus, REGION_STARTS](psx::u32 _address, psx::u8 _value) { sysbus->m_guest_base[REGION_STARTS[0] + _address] = _value; },
		[sysbus, REGION_STARTS](psx::u32 _address, psx::u8 _value) { sysbus->m_guest_base[REGION_STARTS[1] + _address] = _value; },
		[sysbus, REGION_STARTS](psx::u32 _address, psx::u8 _value) { sysbus->m_guest_base[REGION_STARTS[2] + _address] = _value; },
		[sysbus, REGION_STARTS](psx::u32 _address, psx::u8 _value) { }, //TODO: This requires implementing GPU dirty memory tracking
		[sysbus, REGION_STARTS](psx::u32 _address, psx::u8 _value) { sysbus->GetSPU().m_sound_ram[_address] = _value; }
	};

	const std::function<void(psx::u32, psx::u8*, size_t)> REGION_READ_MULTIPLE_FUNCS[] = {
		[sysbus, REGION_STARTS](psx::u32 _address, psx::u8* _dest, size_t _len) { std::copy_n(&sysbus->m_guest_base[REGION_STARTS[0] + _address], _len, _dest); },
		[sysbus, REGION_STARTS](psx::u32 _address, psx::u8* _dest, size_t _len) { std::copy_n(&sysbus->m_guest_base[REGION_STARTS[1] + _address], _len, _dest); },
		[sysbus, REGION_STARTS](psx::u32 _address, psx::u8* _dest, size_t _len) { std::copy_n(&sysbus->m_guest_base[REGION_STARTS[2] + _address], _len, _dest); },
		[sysbus, REGION_STARTS](psx::u32 _address, psx::u8* _dest, size_t _len) { std::fill_n(_dest, _len, 0x0); }, //TODO: This requires implementing GPU dirty memory tracking
		[sysbus, REGION_STARTS](psx::u32 _address, psx::u8* _dest, size_t _len) { std::copy_n(&sysbus->GetSPU().m_sound_ram[_address], _len, _dest); }
	};

	if (!m_is_main_window_open.contains("Memory Editor")) {
		m_is_main_window_open["Memory Editor"] = true;
	}
	if (!m_is_main_window_open["Memory Editor"]) {
		return;
	}
	ImGui::Begin("Memory Editor", &m_is_main_window_open["Memory Editor"]);

	if (!ImGui::BeginTabBar("##mem_tabs")) {
		ImGui::End();
		return;
	}

	static bool s_show_options = {};
	static int32_t s_numerical_base = {};
	static uint32_t s_num_cols = {16};
	static uint32_t s_old_num_cols = {16};
	static bool s_greyed_zeroes = {};
	static bool s_show_ascii = {};
	static bool s_show_preview = {};
	static ImVec2 s_options_pos = {};
	static uint32_t s_goto_addr = {};
	static bool s_goto = false;

	static std::optional<psx::u32> s_selected_addr = std::nullopt;
	static std::optional<size_t> s_selected_addr_region = std::nullopt;

	static std::optional<std::string> s_write_buffer = std::nullopt;
	static std::optional<size_t> s_write_region = std::nullopt;

	static std::optional<size_t> s_last_frame_edit_line_visible = std::nullopt;
	static std::optional<size_t> s_edit_line_frame_blink_timer = std::nullopt;

	constexpr size_t EDIT_LINE_FRAME_BLINK_TIME = 30;

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		s_show_options = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | 
			ImGuiHoveredFlags_ChildWindows);
		s_options_pos = ImGui::GetMousePos();
		ImGui::OpenPopup("##mem_options");
	}

	const uint32_t NUM_COLS_STEP = 1;
	const uint32_t NUM_COLS_STEP_FAST = 1;

	if (s_show_options) {
		if (ImGui::BeginPopup("##mem_options")) {
			ImGui::InputScalar("Columns", ImGuiDataType_U32, std::bit_cast<void*>(&s_num_cols),
				std::bit_cast<const void*>(&NUM_COLS_STEP), std::bit_cast<const void*>(&NUM_COLS_STEP_FAST),
				"%d");
			ImGui::Checkbox("Greyed zeroes", &s_greyed_zeroes);
			ImGui::Checkbox("Show ascii", &s_show_ascii);
			ImGui::Checkbox("Show data preview", &s_show_preview);
			ImGui::EndPopup();
		}
		else {
			s_show_options = false;
		}
	}

	s_num_cols = std::clamp(s_num_cols, 2U, 16U);
	if (s_num_cols != s_old_num_cols) {
		bool above = s_num_cols > s_old_num_cols;

		if (1 != std::popcount(s_num_cols)) {
			auto lzero = std::countl_zero(s_num_cols);
			auto msb_pos = 31 - lzero;
			auto mask = (1 << msb_pos) - 1;
			s_num_cols &= ~mask;
			s_num_cols <<= 1;

			if (!above) {
				s_num_cols >>= 1;
			}
		}

		s_old_num_cols = s_num_cols;
	}
	

	auto text_heigth = ImGui::GetTextLineHeightWithSpacing();
	constexpr const auto PREVIEW_NUM_LINES = 4;
	constexpr const auto FOOTER_NUM_LINES = 1;
	auto footer_height = s_show_preview ? (FOOTER_NUM_LINES + PREVIEW_NUM_LINES + 2) * text_heigth :
		(FOOTER_NUM_LINES + 1) * text_heigth;

	for (size_t region_n = 0; IM_ARRAYSIZE(REGION_NAMES) > region_n; region_n++) {

		if (ImGui::BeginTabItem(REGION_NAMES[region_n])) {
			if (s_selected_addr_region.has_value() && s_selected_addr_region.value() != region_n) {
				s_selected_addr_region = std::nullopt;
				s_selected_addr = std::nullopt;

				s_write_buffer = std::nullopt;
				s_write_region = std::nullopt;
			}
			std::optional<psx::u32> start_address{};
			std::optional<psx::u32> end_address{};

			auto content_region_avail = ImGui::GetContentRegionAvail();
			auto scroll_region_size = content_region_avail.y - footer_height;
			auto cursor_start = ImGui::GetCursorScreenPos();
			if (ImGui::BeginChild("##data", ImVec2(content_region_avail.x, scroll_region_size), ImGuiChildFlags_None, ImGuiWindowFlags_NoMove)) {
				auto num_bytes = REGION_SIZES[region_n];
				auto total_rows = num_bytes / s_num_cols;
				auto rem = num_bytes % s_num_cols; 
				if (0 != rem) { // This should not be possible
					total_rows += 1;
				}

				ImGuiListClipper clipper{};
				clipper.Flags |= ImGuiListClipperFlags_NoSetTableRowCounters;
				clipper.Begin(total_rows);

				if (s_goto) {
					s_goto = false;
					ImGui::SetScrollY(ImGui::GetTextLineHeightWithSpacing() * (float)(s_goto_addr / s_num_cols));
				}

				auto msb_pos = std::countr_zero(REGION_SIZES[region_n]);
				auto address_len = msb_pos / 4 + ((msb_pos & 3) != 0 ? 1 : 0);

				std::vector<psx::u8> row_bytes{};
				row_bytes.reserve(16);

				std::optional<psx::u32> new_address_from_edit{};

				while (clipper.Step()) {
					auto curr_row = clipper.DisplayStart;
					start_address = curr_row * s_num_cols;
					s_goto_addr = start_address.value();
					while (curr_row < clipper.DisplayEnd) {
						auto row_address_rel = curr_row * s_num_cols;
						auto row_address_abs = row_address_rel + REGION_STARTS[region_n];
						auto address_str = fmt::format("{:#0{}x}:", row_address_rel, address_len + 2);
						
						ImGui::TextColored(ImVec4(255.f, 255.f, 255.f, 255.f), "%s", address_str.c_str());
						ImGui::SameLine();
						
						for (auto curr_col = 0U; curr_col < s_num_cols; curr_col++) {
							auto const& read_fun = REGION_READ_FUNCS[region_n];
							auto byte = read_fun(row_address_rel + curr_col);
							row_bytes.push_back(byte);

							auto base_color = IM_COL32(255.f, 255.f, 255.f, 255.f);
							auto grey_color = IM_COL32(150.f, 150.f, 150.f, 255.f);
							auto color_now = (s_greyed_zeroes && (0 == byte)) ? grey_color : base_color;

							auto curr_cursor_pos = ImGui::GetCursorScreenPos();

							auto formatted_byte = fmt::format("{:02x}", byte);
							
							auto show_edit = false;
							if (IS_REGION_WRITEABLE[region_n] && s_write_buffer.has_value() &&
								s_write_region.has_value()) {
								if (s_write_region.value() == region_n &&
									s_selected_addr.has_value() && s_selected_addr_region.has_value()) {
									show_edit = s_selected_addr_region.value() == region_n &&
										s_selected_addr.value() == (row_address_rel + curr_col) &&
										!s_write_buffer.value().empty();
								}
							}
							if (!show_edit) {
								ImGui::GetWindowDrawList()->AddText(curr_cursor_pos, color_now,
									formatted_byte.c_str());
							}
							else {
								auto curr_frame = ImGui::GetFrameCount();
								auto reset_counters = !s_edit_line_frame_blink_timer.has_value()
									|| (s_last_frame_edit_line_visible.has_value() &&
										s_last_frame_edit_line_visible.value() != (size_t)(curr_frame - 1));
								if (reset_counters) {
									s_last_frame_edit_line_visible = curr_frame;
									s_edit_line_frame_blink_timer = 0;
								}

								ImGui::GetWindowDrawList()->AddText(curr_cursor_pos, base_color,
									s_write_buffer.value().c_str());

								bool draw_cursor = (s_edit_line_frame_blink_timer.value() < EDIT_LINE_FRAME_BLINK_TIME);

								if (draw_cursor) {
									auto edit_text_size = ImGui::CalcTextSize(s_write_buffer.value().c_str());
									auto edit_line_pos_x = curr_cursor_pos.x + edit_text_size.x;
									auto edit_line_start_y = curr_cursor_pos.y - 1.f;
									auto edit_line_end_y = curr_cursor_pos.y + edit_text_size.y - 1.f;
									auto edit_line_begin = ImVec2(edit_line_pos_x, edit_line_start_y);
									auto edit_line_end = ImVec2(edit_line_pos_x, edit_line_end_y);
									ImGui::GetWindowDrawList()->AddLine(edit_line_begin, edit_line_end,
										IM_COL32(200.f, 200.f, 200.f, 255.f));
								}

								s_edit_line_frame_blink_timer = s_edit_line_frame_blink_timer.value() + 1;
								if (s_edit_line_frame_blink_timer.value() > EDIT_LINE_FRAME_BLINK_TIME * 2) {
									s_edit_line_frame_blink_timer = 0;
								}

								s_last_frame_edit_line_visible = curr_frame;
							}
							ImGui::SameLine();
							auto text_size = ImGui::CalcTextSize(formatted_byte.c_str());
							if (s_selected_addr.has_value() && s_selected_addr_region.has_value()) {
								if (s_selected_addr_region.value() == region_n &&
									s_selected_addr.value() == (row_address_rel + curr_col)) {
									ImGui::GetWindowDrawList()->AddRectFilled(curr_cursor_pos, 
										ImVec2(curr_cursor_pos.x + text_size.x, curr_cursor_pos.y + text_size.y + 
										text_size.y / 4.f), 
										IM_COL32(150.f, 150.f, 255.f, 100.f));
								}
							}
							ImGui::Dummy(text_size);
							if (s_selected_addr.has_value() && s_selected_addr_region.has_value()) {
								if (s_selected_addr_region.value() == region_n &&
									s_selected_addr.value() == (row_address_rel + curr_col)) {
									if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C) && ImGui::IsWindowFocused()) {
										ImGui::SetClipboardText(formatted_byte.c_str());
									}

									if (!ImGui::IsKeyPressed(ImGuiKey_LeftCtrl) && !ImGui::IsKeyPressed(ImGuiKey_RightCtrl)
										&& !ImGui::IsKeyPressed(ImGuiKey_LeftAlt) && !ImGui::IsKeyPressed(ImGuiKey_RightAlt) &&
										ImGui::IsWindowFocused() && IS_REGION_WRITEABLE[region_n]) {
										auto get_pressed_keys = []() {
											auto key_map = std::unordered_map{
												std::pair{ ImGuiKey_0, '0' },
												std::pair{ ImGuiKey_1, '1' },
												std::pair{ ImGuiKey_2, '2' },
												std::pair{ ImGuiKey_3, '3' },
												std::pair{ ImGuiKey_4, '4' },
												std::pair{ ImGuiKey_5, '5' },
												std::pair{ ImGuiKey_6, '6' },
												std::pair{ ImGuiKey_7, '7' },
												std::pair{ ImGuiKey_8, '8' },
												std::pair{ ImGuiKey_9, '9' },

												std::pair{ ImGuiKey_A, 'A' },
												std::pair{ ImGuiKey_B, 'B' },
												std::pair{ ImGuiKey_C, 'C' },
												std::pair{ ImGuiKey_D, 'D' },
												std::pair{ ImGuiKey_E, 'E' },
												std::pair{ ImGuiKey_F, 'F' },
											};
											std::vector<char> input_digits{};
											for (auto const& [keyname, digit] : key_map) {
												if (ImGui::IsKeyPressed(keyname)) { input_digits.push_back(digit); }
											}
											return input_digits;
										};
										auto pressed_digits = get_pressed_keys();
										if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
											s_write_buffer = std::nullopt;
											s_write_region = std::nullopt;
										}
										else if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
											if (!s_write_buffer.has_value()) {
												new_address_from_edit = s_selected_addr.value() > 0 ?
													s_selected_addr.value() - 1:
													0;
											}
											else if (s_write_buffer.value().size() == 1) {
												s_write_buffer = std::nullopt;
												s_write_region = std::nullopt;
											}
										}
										else if (!pressed_digits.empty()) {
											// Consider only first input
											auto input_digit = pressed_digits[0];
											if (!s_write_buffer.has_value() || !s_write_region.has_value() || 
												(s_write_region.value() != region_n)) {
												s_write_buffer = std::string{};
												s_write_region = region_n;
											}
											s_write_buffer.value() += input_digit;

											if (s_write_buffer.value().size() == 2) {
												new_address_from_edit = s_selected_addr.value() + 1;
												s_write_buffer = std::nullopt;
											}
										}
									}
								}
							}
							if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
								s_selected_addr = row_address_rel + curr_col;
								s_selected_addr_region = region_n;
								s_write_buffer = std::nullopt;
								s_write_region = std::nullopt;
								//ImGui::SetKeyboardFocusHere(-1);
							}

							if (s_num_cols != (curr_col + 1)) {
								ImGui::SameLine();
							}
							
							if (2 == (s_num_cols / (curr_col + 1)) && 0 == (s_num_cols % (curr_col + 1))) {
								ImGui::Text(" "); ImGui::SameLine();
							}
						}

						if (s_show_ascii) {
							auto space_width = ImGui::CalcTextSize("   ").x;

							std::string ascii_repr = {};
							ascii_repr.resize(s_num_cols, ' ');

							for (auto curr_char = 0; row_bytes.size() > curr_char; curr_char++) {
								if (std::isprint(row_bytes[curr_char])) {
									ascii_repr[curr_char] = (char)row_bytes[curr_char];
								}
								else {
									ascii_repr[curr_char] = '.';
								}
							}

							ImGui::SameLine();
							auto cursor_pos = ImGui::GetCursorScreenPos();
							ImGui::GetWindowDrawList()->AddText(ImVec2(cursor_pos.x + space_width, cursor_pos.y),
								IM_COL32(255.f, 255.f, 255.f, 255.f), ascii_repr.c_str());
							ImGui::Dummy(ImVec2(.0f, .0f));
						}

						curr_row++;
						row_bytes.clear();
					}
					end_address = curr_row == 0 ? s_num_cols : (curr_row - 1) * s_num_cols;
				}

				if (ImGui::IsWindowFocused() && s_selected_addr.has_value()) {
					auto new_address = new_address_from_edit.has_value() ? 
						new_address_from_edit.value() : s_selected_addr.value();
					if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
						if (0 <= (int64_t)new_address - (int64_t)s_num_cols) {
							new_address -= s_num_cols;
						}
					} 
					else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
						new_address += s_num_cols;
					}
					else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
						new_address += 1;
					}
					else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
						if (0 <= (int64_t)new_address - (int64_t)1) {
							new_address -= 1;
						}
					}
					new_address = std::min(new_address, REGION_SIZES[s_selected_addr_region.value()] - 1);
					if (start_address.has_value() && end_address.has_value()) {
						auto new_address_row = new_address / s_num_cols;
						auto start_address_row = start_address.value() / s_num_cols;
						auto end_address_row = end_address.value() / s_num_cols;
						if (new_address_row < start_address_row) {
							s_goto = true;
							s_goto_addr = (new_address_row * s_num_cols);
						}
						else if (new_address_row > end_address_row) {
							s_goto = true;
							s_goto_addr = ((start_address_row + 1) * s_num_cols);
						}
					}
					s_selected_addr = new_address;
				}

				if (s_show_ascii) {
					ImGui::SameLine();

					auto space_avail = ImGui::GetContentRegionAvail();
					if (.0f < space_avail.x) {
						auto line_begin = ImVec2(ImGui::GetCursorScreenPos().x, cursor_start.y);
						auto line_end = ImVec2(line_begin.x, line_begin.y + scroll_region_size);
						ImGui::GetWindowDrawList()->AddLine(line_begin, line_end, IM_COL32(255.f, 255.f, 255.f, 255.f));
					}
				}

				ImGui::EndChild();
			}

			if (ImGui::BeginChild("##footer")) {
				auto content_avail = ImGui::GetContentRegionAvail();
				auto curr_cursor_pos = ImGui::GetCursorScreenPos();
				auto line_begin = curr_cursor_pos;
				auto line_end = ImVec2(line_begin.x + content_avail.x, line_begin.y);
				ImGui::GetWindowDrawList()->AddLine(line_begin, line_end, IM_COL32(255.f, 255.f, 255.f, 255.f));
				ImGui::SetCursorScreenPos(ImVec2(line_begin.x, line_begin.y + ImGui::GetTextLineHeightWithSpacing() / 2.f));
				
				if (start_address.has_value() && end_address.has_value()) {
					ImGui::Text("Showing 0x%x...0x%x", start_address.value(), end_address.value());
				}
				else {
					ImGui::Text("No range is currently visualized");
				}

				auto address_size = ImGui::CalcTextSize(" 0x00000000");
				ImGui::SameLine();
				ImGui::Text("Goto:");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(address_size.x);
				if (ImGui::InputScalar("##input_addr", ImGuiDataType_U32, std::bit_cast<void*>(&s_goto_addr),
					nullptr, nullptr, "%x", ImGuiInputTextFlags_CharsHexadecimal)) {
					s_goto = true;
					s_goto_addr = std::min(s_goto_addr, REGION_SIZES[region_n]);
				}

				ImGui::SameLine();
				ImGui::Dummy(ImVec2(50.f, 1.f));
				ImGui::SameLine();

				static uint32_t s_range_start = {};
				static uint32_t s_range_end = {};

				if (ImGui::Button("Dump memory")) {
					ImGui::OpenPopup("Dump memory");
					s_range_start = 0;
					s_range_end = 0;
				}

				if (ImGui::BeginPopup("Dump memory")) {
					ImGui::Text("Range");
					
					ImGui::SetNextItemWidth(address_size.x);
					ImGui::InputScalar("##range_start", ImGuiDataType_U32, std::bit_cast<void*>(&s_range_start),
						nullptr, nullptr, "%x", ImGuiInputTextFlags_CharsHexadecimal);
					ImGui::SameLine();
					ImGui::Text("Start");
					ImGui::SetNextItemWidth(address_size.x);
					ImGui::InputScalar("##range_end", ImGuiDataType_U32, std::bit_cast<void*>(&s_range_end),
						nullptr, nullptr, "%x", ImGuiInputTextFlags_CharsHexadecimal);
					ImGui::SameLine();
					ImGui::Text("End");

					s_range_start = std::min(s_range_start, REGION_SIZES[region_n]);
					s_range_end = std::min(s_range_end, REGION_SIZES[region_n]);

					if (ImGui::Button("Whole region")) {
						s_range_start = 0;
						s_range_end = REGION_SIZES[region_n];
					}

					if (s_range_start >= s_range_end) {
						ImGui::BeginDisabled();
					}

					if (ImGui::Button("Save")) {
						ImGuiFileDialog::Instance()->OpenDialog("DumpMem", "Dump memory", ".bin");
					}

					if (s_range_start >= s_range_end) {
						if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
							if (ImGui::BeginTooltip()) {
								ImGui::Text("Start must be < than End");
								ImGui::EndTooltip();
							}
						}

						ImGui::EndDisabled();
					}

					if (ImGuiFileDialog::Instance()->Display("DumpMem")) {
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
								auto total_bytes = (s_range_end - s_range_start);
								std::vector<psx::u8> all_bytes{};
								all_bytes.resize(total_bytes);
								REGION_READ_MULTIPLE_FUNCS[region_n](s_range_start, all_bytes.data(), total_bytes);
								out_file.write(std::bit_cast<const char*>(all_bytes.data()), total_bytes);
								out_file.close();
							}

							if (!show_error_dialog) {
								ImGuiFileDialog::Instance()->Close();
							}
							else {
								ImGui::OpenPopup("Dump error");
							}
						}
						else {
							ImGuiFileDialog::Instance()->Close();
						}
					}

					if (ImGui::BeginPopupModal("Dump error", nullptr, ImGuiWindowFlags_NoResize)) {
						ImGui::Text("Could not create dump file");
						if (ImGui::Button("OK")) {
							ImGui::CloseCurrentPopup();
						}
						ImGui::EndPopup();
					}

					ImGui::EndPopup();
				}

				if (s_show_preview) {
					auto content_avail = ImGui::GetContentRegionAvail();
					auto curr_cursor_pos = ImGui::GetCursorScreenPos();
					auto line_begin = curr_cursor_pos;
					auto text_line_height = ImGui::GetTextLineHeightWithSpacing();
					line_begin.y += text_line_height / 2.f;
					auto line_end = ImVec2(line_begin.x + content_avail.x, line_begin.y);
					ImGui::GetWindowDrawList()->AddLine(line_begin, line_end, IM_COL32(255.f, 255.f, 255.f, 255.f));
					ImGui::SetCursorScreenPos(ImVec2(line_begin.x, line_begin.y + text_line_height / 2.f));
					ImGui::Text("Format:");
					ImGui::Text("DEC");
					ImGui::Text("HEX");
					ImGui::Text("BIN");
				}
				
				ImGui::EndChild();
			}

			ImGui::EndTabItem();
		}
	}

	ImGui::EndTabBar();
	ImGui::End();
}