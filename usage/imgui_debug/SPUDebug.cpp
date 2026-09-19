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
	using u8 = psx::u8;
	using i32 = psx::i32;

	for (size_t curr_voice = 0; psx::SPU::NUM_VOICES > curr_voice; curr_voice++) {
		psx::SPUVoice& voice = spu.m_voices[curr_voice];
		
		ImGui::Text("Voice %d", curr_voice);

		auto str_id = fmt::format("##voice_{}", curr_voice);
		auto str_pmon_id = fmt::format("##voice_{}_pmon", curr_voice);
		auto str_noise_id = fmt::format("##voice_{}_noise", curr_voice);
		auto str_reverb_id = fmt::format("##voice_{}_reverb", curr_voice);

		auto pm = spu.m_regs.m_pmon.is_channel_modulated(curr_voice);
		auto noise = spu.m_regs.m_noise_en.is_channel_noise(curr_voice);
		auto reverb = spu.m_regs.m_reverb_en.is_channel_reverb(curr_voice);
		auto on = voice.m_is_on;

		ImGui::SameLine();
		ImGui::PushID(str_id.c_str());
		ImGui::Checkbox("ON", &on);
		ImGui::PopID();
		ImGui::SameLine();
		ImGui::PushID(str_pmon_id.c_str());
		ImGui::Checkbox("PM", &pm);
		ImGui::PopID();
		ImGui::SameLine();
		ImGui::PushID(str_noise_id.c_str());
		ImGui::Checkbox("N", &noise);
		ImGui::PopID();
		ImGui::SameLine();
		ImGui::PushID(str_reverb_id.c_str());
		ImGui::Checkbox("R", &reverb);
		ImGui::PopID();

		auto other_info_id = fmt::format("##voice_{}_other_info", curr_voice);
		ImGui::BeginTable(other_info_id.c_str(), 2);

		constexpr const char* ADSR_PHASES[] = {
			"NONE",
			"ATTACK",
			"DECAY",
			"SUSTAIN",
			"RELEASE"
		};

		auto adsr_phase = (i32)voice.m_curr_adsr_phase;
		auto address = voice.m_curr_address;
		auto endx_flag = voice.m_endx_flag;

		auto adsr_phase_id = fmt::format("##voice_{}_adsr_curr_phase", curr_voice);
		auto address_id = fmt::format("##voice_{}_curr_address", curr_voice);
		auto endx_id = fmt::format("##voice_{}_endx", curr_voice);


		ImGui::TableNextColumn();
		ImGui::Text("ADSR Phase");
		ImGui::TableNextColumn();
		ImGui::Combo(adsr_phase_id.c_str(), &adsr_phase,
			ADSR_PHASES, IM_ARRAYSIZE(ADSR_PHASES));
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text("Current Address");
		ImGui::TableNextColumn();
		ImGui::InputScalar(address_id.c_str(),
			ImGuiDataType_U32, std::bit_cast<void*>(&address),
			nullptr, nullptr, "0x%08x",
			ImGuiInputTextFlags_ReadOnly);
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text("ENDX Flag");
		ImGui::TableNextColumn();
		ImGui::Checkbox(endx_id.c_str(), &endx_flag);
		ImGui::TableNextRow();

		ImGui::EndTable();

		auto label = fmt::format("Voice {} details", curr_voice);
		if (ImGui::CollapsingHeader(label.c_str(),
			ImGuiTreeNodeFlags_Framed)) {
			auto str_id = fmt::format("##voice_{}_regs_table", curr_voice);

			auto sr_id = fmt::format("##voice_{}_sr", curr_voice);
			auto add_id = fmt::format("##voice_{}_add", curr_voice);
			auto lop_id = fmt::format("##voice_{}_lop", curr_voice);

			ImGui::BeginTable(str_id.c_str(), 2);

			ImGui::TableNextColumn();
			ImGui::Text("Sample Rate");
			ImGui::TableNextColumn();
			ImGui::InputScalar(sr_id.c_str(), ImGuiDataType_U16,
				std::bit_cast<void*>(&voice.m_config.m_sample_rate),
				nullptr, nullptr, "%d",
				ImGuiInputTextFlags_ReadOnly);
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text("ADPCM Start Address");
			ImGui::TableNextColumn();
			ImGui::InputScalar(add_id.c_str(), ImGuiDataType_U16,
				std::bit_cast<void*>(&voice.m_config.m_adpcm_start_address),
				nullptr, nullptr, "0x%04x",
				ImGuiInputTextFlags_CharsHexadecimal |
				ImGuiInputTextFlags_ReadOnly);
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text("Loop Address");
			ImGui::TableNextColumn();
			ImGui::InputScalar(lop_id.c_str(), ImGuiDataType_U16,
				std::bit_cast<void*>(&voice.m_config.m_repeat_address),
				nullptr, nullptr, "0x%04x",
				ImGuiInputTextFlags_CharsHexadecimal |
				ImGuiInputTextFlags_ReadOnly);
			ImGui::TableNextRow();

			ImGui::EndTable();

			auto vol_id = fmt::format("##voice_{}_vol_l", curr_voice);

			ImGui::Text("Volume L");
			if (psx::SPU_VoiceVolumeMode::VOLUME ==
				voice.m_config.m_volume_left.mode) {
				ImGui::Text("Current mode: VOLUME");


				ImGui::BeginTable(vol_id.c_str(), 1);
				ImGui::TableNextColumn();
				ImGui::Text("Volume level");
				ImGui::TableNextColumn();
				auto vol_l = voice.m_config.m_volume_left.volume.half_volume << 1;
				ImGui::InputScalar("", ImGuiDataType_U16,
					std::bit_cast<void*>(&vol_l),
					nullptr, nullptr, "0x%04x",
					ImGuiInputTextFlags_CharsHexadecimal |
					ImGuiInputTextFlags_ReadOnly);
				ImGui::EndTable();
			}
			else {
				ImGui::Text("Current mode: SWEEP");

				auto step = (u8)voice.m_config.m_volume_left.sweep.step;
				auto shift = (u8)voice.m_config.m_volume_left.sweep.shift;

				constexpr const char* SWEEP_MODES[] = { "LINEAR", "EXPONENTIAL" };
				constexpr const char* SWEEP_DIRECTIONS[] = { "INCREASE", "DECREASE" };
				constexpr const char* SWEEP_PHASES[] = { "POSITIVE", "NEGATIVE" };

				auto phase = (i32)voice.m_config.m_volume_left.sweep.phase;
				auto direction = (i32)voice.m_config.m_volume_left.sweep.direction;
				auto mode = (i32)voice.m_config.m_volume_left.sweep.mode;

				auto sweep_id = fmt::format("##voice_{}_sweep_l", curr_voice);

				auto step_id = fmt::format("##voice_{}_sweep_l_step", curr_voice);
				auto shift_id = fmt::format("##voice_{}_sweep_l_shift", curr_voice);
				auto phase_id = fmt::format("##voice_{}_sweep_l_phase", curr_voice);
				auto direction_id = fmt::format("##voice_{}_sweep_l_dir", curr_voice);
				auto mode_id = fmt::format("##voice_{}_sweep_l_mode", curr_voice);

				ImGui::BeginTable(sweep_id.c_str(), 5);

				ImGui::TableNextColumn();
				ImGui::Text("Step");
				ImGui::TableNextColumn();
				ImGui::InputScalar(step_id.c_str(),
					ImGuiDataType_U8,
					std::bit_cast<void*>(&step));
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text("Shift");
				ImGui::TableNextColumn();
				ImGui::InputScalar(shift_id.c_str(),
					ImGuiDataType_U8,
					std::bit_cast<void*>(&shift));
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text("Phase");
				ImGui::TableNextColumn();
				ImGui::Combo(phase_id.c_str(), &phase,
					SWEEP_PHASES, IM_ARRAYSIZE(SWEEP_PHASES));
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text("Direction");
				ImGui::TableNextColumn();
				ImGui::Combo(direction_id.c_str(), &direction,
					SWEEP_DIRECTIONS, IM_ARRAYSIZE(SWEEP_DIRECTIONS));
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text("Mode");
				ImGui::TableNextColumn();
				ImGui::Combo(mode_id.c_str(), &mode,
					SWEEP_MODES, IM_ARRAYSIZE(SWEEP_MODES));
				ImGui::TableNextRow();

				ImGui::EndTable();
			}

			vol_id = fmt::format("##voice_{}_vol_r", curr_voice);
			ImGui::Text("Volume R");
			if (psx::SPU_VoiceVolumeMode::VOLUME ==
				voice.m_config.m_volume_right.mode) {
				ImGui::Text("Current mode: VOLUME");


				ImGui::BeginTable(vol_id.c_str(), 1);
				ImGui::Text("Volume level");
				ImGui::TableNextColumn();
				auto vol_r = voice.m_config.m_volume_right.volume.half_volume << 1;
				ImGui::InputScalar("", ImGuiDataType_U16,
					std::bit_cast<void*>(&vol_r),
					nullptr, nullptr, "0x%04x",
					ImGuiInputTextFlags_CharsHexadecimal |
					ImGuiInputTextFlags_ReadOnly);
				ImGui::EndTable();
			}
			else {
				ImGui::Text("Current mode: SWEEP");

				auto step = (u8)voice.m_config.m_volume_right.sweep.step;
				auto shift = (u8)voice.m_config.m_volume_right.sweep.shift;

				constexpr const char* SWEEP_MODES[] = { "LINEAR", "EXPONENTIAL" };
				constexpr const char* SWEEP_DIRECTIONS[] = { "INCREASE", "DECREASE" };
				constexpr const char* SWEEP_PHASES[] = { "POSITIVE", "NEGATIVE" };

				auto phase = (i32)voice.m_config.m_volume_right.sweep.phase;
				auto direction = (i32)voice.m_config.m_volume_right.sweep.direction;
				auto mode = (i32)voice.m_config.m_volume_right.sweep.mode;

				auto sweep_id = fmt::format("##voice_{}_sweep_r", curr_voice);

				auto step_id = fmt::format("##voice_{}_sweep_r_step", curr_voice);
				auto shift_id = fmt::format("##voice_{}_sweep_r_shift", curr_voice);
				auto phase_id = fmt::format("##voice_{}_sweep_r_phase", curr_voice);
				auto direction_id = fmt::format("##voice_{}_sweep_r_dir", curr_voice);
				auto mode_id = fmt::format("##voice_{}_sweep_r_mode", curr_voice);

				ImGui::BeginTable(sweep_id.c_str(), 5);

				ImGui::TableNextColumn();
				ImGui::Text("Step");
				ImGui::TableNextColumn();
				ImGui::InputScalar(step_id.c_str(),
					ImGuiDataType_U8,
					std::bit_cast<void*>(&step));
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text("Shift");
				ImGui::TableNextColumn();
				ImGui::InputScalar(shift_id.c_str(),
					ImGuiDataType_U8,
					std::bit_cast<void*>(&shift));
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text("Phase");
				ImGui::TableNextColumn();
				ImGui::Combo(phase_id.c_str(), &phase,
					SWEEP_PHASES, IM_ARRAYSIZE(SWEEP_PHASES));
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text("Direction");
				ImGui::TableNextColumn();
				ImGui::Combo(direction_id.c_str(), &direction,
					SWEEP_DIRECTIONS, IM_ARRAYSIZE(SWEEP_DIRECTIONS));
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text("Mode");
				ImGui::TableNextColumn();
				ImGui::Combo(mode_id.c_str(), &mode,
					SWEEP_MODES, IM_ARRAYSIZE(SWEEP_MODES));
				ImGui::TableNextRow();

				ImGui::EndTable();
			}

			ImGui::Text("ADSR Settings");
			auto sustain_level = (u8)voice.m_config.m_adsr.sustain_level;
			auto decay_shift = (u8)voice.m_config.m_adsr.decay_shift;
			auto attack_step = (u8)voice.m_config.m_adsr.attack_step;
			auto attack_shift = (u8)voice.m_config.m_adsr.attack_shift;
			auto attack_mode = (i32)voice.m_config.m_adsr.attack_mode;
			auto release_shift = (u8)voice.m_config.m_adsr.release_shift;
			auto release_mode = (i32)voice.m_config.m_adsr.release_mode;
			auto sustain_step = (u8)voice.m_config.m_adsr.sustain_step;
			auto sustain_shift = (u8)voice.m_config.m_adsr.sustain_shift;
			auto sustain_dir = (i32)voice.m_config.m_adsr.sustain_dir;
			auto sustain_mode = (i32)voice.m_config.m_adsr.sustain_mode;

			auto adsr_id = fmt::format("##voice_{}_adsr", curr_voice);

			auto sustain_level_id = fmt::format("##voice_{}_sustain_level", curr_voice);
			auto decay_shift_id = fmt::format("##voice_{}_decay_shift", curr_voice);
			auto attack_step_id = fmt::format("##voice_{}_attack_step", curr_voice);
			auto attack_shift_id = fmt::format("##voice_{}_attack_shift", curr_voice);
			auto attack_mode_id = fmt::format("##voice_{}_attack_mode", curr_voice);
			auto release_shift_id = fmt::format("##voice_{}_release_shift", curr_voice);
			auto release_mode_id = fmt::format("##voice_{}_release_mode", curr_voice);
			auto sustain_step_id = fmt::format("##voice_{}_sustain_step", curr_voice);
			auto sustain_shift_id = fmt::format("##voice_{}_sustain_shift", curr_voice);
			auto sustain_dir_id = fmt::format("##voice_{}_sustain_dir", curr_voice);
			auto sustain_mode_id = fmt::format("##voice_{}_sustain_mode", curr_voice);
			
			ImGui::BeginTable(adsr_id.c_str(), 2);

			ImGui::TableNextColumn();
			ImGui::Text("Sustain Level");
			ImGui::TableNextColumn();
			ImGui::InputScalar(sustain_level_id.c_str(),
				ImGuiDataType_U8,
				std::bit_cast<void*>(&sustain_level));
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text("Decay Shift");
			ImGui::TableNextColumn();
			ImGui::InputScalar(decay_shift_id.c_str(),
				ImGuiDataType_U8,
				std::bit_cast<void*>(&decay_shift));
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text("Attack Step");
			ImGui::TableNextColumn();
			ImGui::InputScalar(attack_step_id.c_str(),
				ImGuiDataType_U8,
				std::bit_cast<void*>(&attack_step));
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text("Attack Shift");
			ImGui::TableNextColumn();
			ImGui::InputScalar(attack_shift_id.c_str(),
				ImGuiDataType_U8,
				std::bit_cast<void*>(&attack_shift));
			ImGui::TableNextRow();

			constexpr const char* ATTACK_MODES[] = { "LINEAR", "EXPONENTIAL" };
			constexpr const char* RELEASE_MODES[] = { "LINEAR", "EXPONENTIAL" };
			constexpr const char* SUSTAIN_MODES[] = { "LINEAR", "EXPONENTIAL" };
			constexpr const char* SUSTAIN_DIRS[] = { "INCREASE", "DECREASE" };

			ImGui::TableNextColumn();
			ImGui::Text("Attack Mode");
			ImGui::TableNextColumn();
			ImGui::Combo(attack_mode_id.c_str(), &attack_mode,
				ATTACK_MODES, IM_ARRAYSIZE(ATTACK_MODES));
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text("Release Shift");
			ImGui::TableNextColumn();
			ImGui::InputScalar(release_shift_id.c_str(),
				ImGuiDataType_U8,
				std::bit_cast<void*>(&release_shift));
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text("Release Mode");
			ImGui::TableNextColumn();
			ImGui::Combo(release_mode_id.c_str(), &release_mode,
				RELEASE_MODES, IM_ARRAYSIZE(RELEASE_MODES));
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text("Sustain Step");
			ImGui::TableNextColumn();
			ImGui::InputScalar(sustain_step_id.c_str(),
				ImGuiDataType_U8,
				std::bit_cast<void*>(&sustain_step));
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text("Sustain Shift");
			ImGui::TableNextColumn();
			ImGui::InputScalar(sustain_shift_id.c_str(),
				ImGuiDataType_U8,
				std::bit_cast<void*>(&sustain_shift));
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text("Sustain Direction");
			ImGui::TableNextColumn();
			ImGui::Combo(sustain_dir_id.c_str(), &sustain_dir,
				SUSTAIN_DIRS, IM_ARRAYSIZE(SUSTAIN_DIRS));
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text("Sustain Mode");
			ImGui::TableNextColumn();
			ImGui::Combo(sustain_mode_id.c_str(), &sustain_mode,
				SUSTAIN_MODES, IM_ARRAYSIZE(SUSTAIN_MODES));
			ImGui::TableNextRow();

			ImGui::EndTable();
		}

		ImGui::Separator();

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