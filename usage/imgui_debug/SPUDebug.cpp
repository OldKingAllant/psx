#include "DebugView.hpp"

#include <psxemu/include/psxemu/System.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/SPURegisters.hpp>
#include <psxemu/include/psxemu/SPUStructs.hpp>
#include <psxemu/include/psxemu/SPU.hpp>
#include <psxemu/include/psxemu/SPUVoice.hpp>

#include <thirdparty/imgui/imgui.h>
#include <thirdparty/magic_enum/include/magic_enum/magic_enum.hpp>
#include <fmt/format.h>

#include <array>
#include <bit>

void DebugView::SpuControlWindow() {
	psx::SPU& spu = m_psx->GetStatus().sysbus->GetSPU();

	ImGui::Text("Control Register");

	using u8 = psx::u8;

	auto enable = (bool)spu.m_regs.m_cnt.enable;
	auto unmute = (bool)spu.m_regs.m_cnt.unmute;
	auto noise_shift = (u8)spu.m_regs.m_cnt.noise_freq_shift;
	auto noise_step = (u8)spu.m_regs.m_cnt.noise_freq_step;
	auto reverb_enable = (bool)spu.m_regs.m_cnt.reverb_master_en;
	auto irq9_enable = (bool)spu.m_regs.m_cnt.irq9_enable;
	auto transfer_mode = (psx::SoundRamTransferMode)spu.m_regs.m_cnt.transer_mode;
	auto ext_reverb = (bool)spu.m_regs.m_cnt.ext_audio_reverb;
	auto cd_reverb = (bool)spu.m_regs.m_cnt.cd_audio_reverb;
	auto en_ext_audio = (bool)spu.m_regs.m_cnt.ext_audio_en;
	auto en_cd_audio = (bool)spu.m_regs.m_cnt.cd_audio_en;

	if (!ImGui::BeginChild("##spu_cnt_child", ImVec2(.0f, .0f), ImGuiChildFlags_Borders)) {
		ImGui::EndChild();
		return;
	}

	ImGui::BeginTable("##spu_control", 2);
	ImGui::TableNextColumn();
	ImGui::Text("Enable");
	ImGui::TableNextColumn();
	ImGui::Checkbox("##enabled", &enable);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("Unmute");
	ImGui::TableNextColumn();
	ImGui::Checkbox("##unmute", &unmute);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("Noise f. shift");
	ImGui::TableNextColumn();
	ImGui::Text("%d", noise_shift);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("Noise f. step");
	ImGui::TableNextColumn();
	ImGui::Text("%d", noise_step);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("Reverb enable");
	ImGui::TableNextColumn();
	ImGui::Checkbox("##en_reverb", &reverb_enable);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("Enable IRQ 9");
	ImGui::TableNextColumn();
	ImGui::Checkbox("##en_irq9", &irq9_enable);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("Transfer mode");
	ImGui::TableNextColumn();
	
	constexpr const char* tranfser_modes[] = {"STOP", "MANUAL", "DMA_WRITE", "DMA_READ"};
	auto curr_mode = (int)transfer_mode;

	ImGui::Combo("##transfer", &curr_mode, tranfser_modes, IM_ARRAYSIZE(tranfser_modes));
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("Ext. audio reverb");
	ImGui::TableNextColumn();
	ImGui::Checkbox("##ext_reverb", &ext_reverb);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("CD audio reverb");
	ImGui::TableNextColumn();
	ImGui::Checkbox("##cd_reverb", &cd_reverb);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("Ext. audio enable");
	ImGui::TableNextColumn();
	ImGui::Checkbox("##ext_en", &en_ext_audio);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("CD audio enable");
	ImGui::TableNextColumn();
	ImGui::Checkbox("##cd_en", &en_cd_audio);

	ImGui::EndTable();
	ImGui::EndChild();

	auto curr_spu_mode = (u8)spu.m_regs.m_stat.current_spu_mode;
	auto irq_flag = (bool)spu.m_regs.m_stat.irq9_flag;
	auto dreq = (bool)spu.m_regs.m_stat.dma_request;
	auto w_req = (bool)spu.m_regs.m_stat.dma_write_req;
	auto r_req = (bool)spu.m_regs.m_stat.dma_read_req;
	auto transfer_busy = (bool)spu.m_regs.m_stat.transfer_busy;
	auto capture_second_half = (bool)spu.m_regs.m_stat.writing_second_half_of_capture;

	ImGui::Text("Status Register");
	if (!ImGui::BeginChild("##spu_stat_child", ImVec2(.0f, .0f), ImGuiChildFlags_Borders)) {
		ImGui::EndChild();
		return;
	}

	ImGui::BeginTable("##spu_stat", 2);

	ImGui::TableNextColumn();
	ImGui::Text("Current mode");
	ImGui::TableNextColumn();
	ImGui::Text("%d", curr_spu_mode);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("IRQ flag");
	ImGui::TableNextColumn();
	ImGui::Checkbox("##irq_flag", &irq_flag);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("DMA Request");
	ImGui::TableNextColumn();
	ImGui::Checkbox("##dreq", &dreq);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("DMA Write request");
	ImGui::TableNextColumn();
	ImGui::Checkbox("##w_req", &w_req);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("DMA Read request");
	ImGui::TableNextColumn();
	ImGui::Checkbox("##r_req", &r_req);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("Transfer busy");
	ImGui::TableNextColumn();
	ImGui::Checkbox("##transfer_busy", &transfer_busy);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("Second half of capture");
	ImGui::TableNextColumn();
	ImGui::Checkbox("##sec_half", &capture_second_half);
	ImGui::TableNextRow();

	ImGui::EndTable();
	ImGui::EndChild();
}

void DebugView::SpuReverbWindow() {
	if (!ImGui::BeginChild("##spu_reverb", ImVec2(.0f, .0f), ImGuiChildFlags_Borders)) {
		ImGui::EndChild();
		return;
	}

	psx::SPU& spu = m_psx->GetStatus().sysbus->GetSPU();

	ImGui::Text("Registers");
	ImGui::BeginTable("##spu_reverb_table", 2);

	constexpr const char* REG_NAMES[] = {
		"Volume L",
		"Volume R",
		"Reverb area address",
		"APF Offset 1",
		"APF Offset 2",
		"Reflect Volume 1",
		"Comb Volume 1",
		"Comb Volume 2",
		"Comb Volume 3",
		"Comb Volume 4",
		"Reflect Volume 2",
		"APF Volume 1",
		"APF Volume 2",
		"Same Side Reflect Address 1 L",
		"Same Side Reflect Address 1 R",
		"Comb Address 1 L",
		"Comb Address 1 R",
		"Comb Address 2 L",
		"Comb Address 2 R",
		"Same Side Reflect Address 2 L",
		"Same Side Reflect Address 2 R",
		"Opposite Side Reflect Address 1 L",
		"Opposite Side Reflect Address 1 R",
		"Comb Address 3 L",
		"Comb Address 3 R",
		"Comb Address 4 L",
		"Comb Address 4 R",
		"Opposite Side Reflect Address 2 L",
		"Opposite Side Reflect Address 2 R",
		"APF Address 1 L",
		"APF Address 1 R",
		"APF Address 2 L",
		"APF Address 2 R",
		"Input Volume L",
		"Input Volume R",
	};

	constexpr auto IS_REG_UNSIGNED = []() {
		std::array<bool, IM_ARRAYSIZE(REG_NAMES)> is_reg_unsigned_temp = {};

		is_reg_unsigned_temp[2] = true;

		// from index 13 to 32 (included)
		for (size_t index = 13; 32 >= index; index++) {
			is_reg_unsigned_temp[index] = true;
		}

		return is_reg_unsigned_temp;
	} ();

	using u16 = psx::u16;
	auto regs_ptr = std::bit_cast<u16*>(&spu.m_regs.m_reverb);
	for (size_t curr_reg = 0; IS_REG_UNSIGNED.size() > curr_reg; curr_reg++) {
		ImGui::TableNextColumn();
		ImGui::Text(REG_NAMES[curr_reg]);
		ImGui::TableNextColumn();
		auto id_string = fmt::format("##reg_{}", curr_reg);
		ImGui::InputScalar(id_string.c_str(), ImGuiDataType_U16,
			std::bit_cast<void*>(&regs_ptr[curr_reg]),
			nullptr, nullptr, "0x%04x", ImGuiInputTextFlags_ReadOnly |
			ImGuiInputTextFlags_CharsHexadecimal);
		ImGui::TableNextRow();
	}


	ImGui::EndTable();
	ImGui::EndChild();
}

void DebugView::SpuOtherRegistersWindow() {
	if (!ImGui::BeginChild("##spu_other_regs", ImVec2(.0f, .0f), ImGuiChildFlags_Borders)) {
		ImGui::EndChild();
		return;
	}

	psx::SPU& spu = m_psx->GetStatus().sysbus->GetSPU();

	ImGui::BeginTable("##spu_other_regs", 2);

	ImGui::TableNextColumn();
	ImGui::Text("Volume L");
	ImGui::TableNextColumn();
	auto vol_l = spu.m_regs.m_mainvolume_left.volume.half_volume << 1;
	ImGui::InputScalar("##spu_vol_l", ImGuiDataType_S16,
		std::bit_cast<void*>(&vol_l), nullptr, nullptr, 
		"0x%04x", ImGuiInputTextFlags_ReadOnly | 
		ImGuiInputTextFlags_CharsHexadecimal);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("Volume R");
	ImGui::TableNextColumn();
	auto vol_r = spu.m_regs.m_mainvolume_right.volume.half_volume << 1;
	ImGui::InputScalar("##spu_vol_r", ImGuiDataType_S16,
		std::bit_cast<void*>(&vol_r), nullptr, nullptr,
		"0x%04x", ImGuiInputTextFlags_ReadOnly |
		ImGuiInputTextFlags_CharsHexadecimal);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("CD Audio Volume L");
	ImGui::TableNextColumn();
	ImGui::InputScalar("##spu_cd_vol_l", ImGuiDataType_S16,
		std::bit_cast<void*>(&spu.m_regs.m_cd_audio_in_vol_left), nullptr, nullptr,
		"0x%04x", ImGuiInputTextFlags_ReadOnly |
		ImGuiInputTextFlags_CharsHexadecimal);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("CD Audio Volume R");
	ImGui::TableNextColumn();
	ImGui::InputScalar("##spu_cd_vol_r", ImGuiDataType_S16,
		std::bit_cast<void*>(&spu.m_regs.m_cd_audio_in_vol_right), nullptr, nullptr,
		"0x%04x", ImGuiInputTextFlags_ReadOnly |
		ImGuiInputTextFlags_CharsHexadecimal);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("Ext. Audio Volume L");
	ImGui::TableNextColumn();
	ImGui::InputScalar("##spu_ext_vol_l", ImGuiDataType_S16,
		std::bit_cast<void*>(&spu.m_regs.m_ext_audio_in_vol_left), nullptr, nullptr,
		"0x%04x", ImGuiInputTextFlags_ReadOnly |
		ImGuiInputTextFlags_CharsHexadecimal);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("Ext. Audio Volume R");
	ImGui::TableNextColumn();
	ImGui::InputScalar("##spu_ext_vol_r", ImGuiDataType_S16,
		std::bit_cast<void*>(&spu.m_regs.m_ext_audio_in_vol_right), nullptr, nullptr,
		"0x%04x", ImGuiInputTextFlags_ReadOnly |
		ImGuiInputTextFlags_CharsHexadecimal);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("Sound RAM Transfer Address");
	ImGui::TableNextColumn();
	ImGui::InputScalar("##spu_ram_address", ImGuiDataType_U16,
		std::bit_cast<void*>(&spu.m_regs.m_ram_transfer_address), nullptr, nullptr,
		"0x%04x", ImGuiInputTextFlags_ReadOnly |
		ImGuiInputTextFlags_CharsHexadecimal);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("IRQ RAM Address");
	ImGui::TableNextColumn();
	ImGui::InputScalar("##spu_ram_irq_address", ImGuiDataType_U16,
		std::bit_cast<void*>(&spu.m_regs.m_irq_address), nullptr, nullptr,
		"0x%04x", ImGuiInputTextFlags_ReadOnly |
		ImGuiInputTextFlags_CharsHexadecimal);
	ImGui::TableNextRow();

	ImGui::EndTable();
	ImGui::EndChild();
}

void DebugView::SpuVoicesWindow() {
	psx::SPU& spu = m_psx->GetStatus().sysbus->GetSPU();
	for (size_t curr_voice = 0; psx::SPU::NUM_VOICES > curr_voice; curr_voice++) {
		psx::SPUVoice& voice = spu.m_voices[curr_voice];
		ImGui::Text("Voice %d", curr_voice);

		auto str_id = fmt::format("##voice_{}", curr_voice);
		auto str_pmon_id = fmt::format("##voice_{}_pmon", curr_voice);
		auto str_noise_id = fmt::format("##voice_{}_noise", curr_voice);
		auto str_reverb_id = fmt::format("##voice_{}_reverb", curr_voice);

		ImGui::BeginTable(str_id.c_str(), 2);

		ImGui::TableNextColumn();
		ImGui::Text("Pitch Modulation");
		ImGui::TableNextColumn();
		ImGui::RadioButton(str_pmon_id.c_str(), spu.m_regs.m_pmon.is_channel_modulated(curr_voice));
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text("Noise");
		ImGui::TableNextColumn();
		ImGui::RadioButton(str_noise_id.c_str(), spu.m_regs.m_noise_en.is_channel_noise(curr_voice));
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text("Reverb");
		ImGui::TableNextColumn();
		ImGui::RadioButton(str_reverb_id.c_str(), spu.m_regs.m_reverb_en.is_channel_reverb(curr_voice));
		ImGui::TableNextRow();

		ImGui::EndTable();
		ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 10.f));
	}
}

void DebugView::SpuWindow() {
	if (!m_is_main_window_open.contains("SPU Window")) {
		m_is_main_window_open["SPU Window"] = true;
	}
	if (!m_is_main_window_open["SPU Window"]) {
		return;
	}
	ImGui::Begin("SPU Window", &m_is_main_window_open["SPU Window"]);

	if (!ImGui::BeginTabBar("##spu_tabs")) {
		ImGui::End();
		return;
	}

	psx::SPU& spu = m_psx->GetStatus().sysbus->GetSPU();

	if (ImGui::BeginTabItem("Control & Status")) {
		SpuControlWindow();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Reverb regs")) {
		SpuReverbWindow();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Other regs")) {
		SpuOtherRegistersWindow();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Voices")) {
		SpuVoicesWindow();
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();

	ImGui::End();
}