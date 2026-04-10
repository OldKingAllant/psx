#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/include/psxemu/GPUCommands.hpp>
#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <psxemu/renderer/GLRenderer.hpp>
#include <psxemu/renderer/GLContext.hpp>

#include <thirdparty/cereal/cereal.hpp>
#include <thirdparty/cereal/archives/portable_binary.hpp>
#include <thirdparty/cereal/types/string.hpp>
#include <thirdparty/cereal/types/vector.hpp>

namespace psx {
	void Gpu::PushStateConfiguration(std::vector<GPUCommand>& commands) const {
		GPUCommand cmd{};

		{
			cmd.frame_of_recording = m_curr_vblank_count;
			cmd.reg = CommandRegister::GP1;
			cmd.gp1.type = GP1CommandType::DISPLAY_ENABLE;
			cmd.gp1.disp_enable.display_on = m_stat.disp_enable;
			cmd.from_prev_frame = true;
			commands.push_back(cmd);
		}

		{
			cmd.frame_of_recording = m_curr_vblank_count;
			cmd.reg = CommandRegister::GP1;
			cmd.gp1.type = GP1CommandType::DISPLAY_MODE;
			cmd.gp1.disp_mode.reg = m_raw_disp_conf;
			cmd.from_prev_frame = true;
			commands.push_back(cmd);
		}

		{
			cmd.frame_of_recording = m_curr_vblank_count;
			cmd.reg = CommandRegister::GP1;
			cmd.gp1.type = GP1CommandType::DMA_DIRECTION;
			cmd.gp1.dma_dir.direction = u8(m_stat.dma_dir);
			cmd.from_prev_frame = true;
			commands.push_back(cmd);
		}

		{
			cmd.frame_of_recording = m_curr_vblank_count;
			cmd.reg = CommandRegister::GP1;
			cmd.gp1.type = GP1CommandType::SET_VRAM_SIZE;
			cmd.gp1.set_vram_size.two_mbytes = 0;
			cmd.from_prev_frame = true;
			commands.push_back(cmd);
		}

		{
			cmd.frame_of_recording = m_curr_vblank_count;
			cmd.reg = CommandRegister::GP0;
			cmd.gp0.type = GP0CommandType::ENV;
			cmd.gp0.env.type = EnvCommandType::SET_DRAW_BOTTOM;
			cmd.gp0.env.cmd = m_raw_conf.draw_bottom_right;
			cmd.from_prev_frame = true;
			commands.push_back(cmd);
		}

		{
			cmd.frame_of_recording = m_curr_vblank_count;
			cmd.reg = CommandRegister::GP0;
			cmd.gp0.type = GP0CommandType::ENV;
			cmd.gp0.env.type = EnvCommandType::SET_DRAW_TOP;
			cmd.gp0.env.cmd = m_raw_conf.draw_top_left;
			cmd.from_prev_frame = true;
			commands.push_back(cmd);
		}

		{
			cmd.frame_of_recording = m_curr_vblank_count;
			cmd.reg = CommandRegister::GP0;
			cmd.gp0.type = GP0CommandType::ENV;
			cmd.gp0.env.type = EnvCommandType::SET_DRAW_OFFSET;
			cmd.gp0.env.cmd = m_raw_conf.draw_offset;
			cmd.from_prev_frame = true;
			commands.push_back(cmd);
		}

		{
			cmd.frame_of_recording = m_curr_vblank_count;
			cmd.reg = CommandRegister::GP0;
			cmd.gp0.type = GP0CommandType::ENV;
			cmd.gp0.env.type = EnvCommandType::MASK_BIT;
			cmd.gp0.env.cmd = m_raw_conf.mask_setting;
			cmd.from_prev_frame = true;
			commands.push_back(cmd);
		}

		{
			cmd.frame_of_recording = m_curr_vblank_count;
			cmd.reg = CommandRegister::GP0;
			cmd.gp0.type = GP0CommandType::ENV;
			cmd.gp0.env.type = EnvCommandType::TEXTURE_PAGE;
			cmd.gp0.env.cmd = m_raw_conf.texpage;
			cmd.from_prev_frame = true;
			commands.push_back(cmd);
		}

		{
			cmd.frame_of_recording = m_curr_vblank_count;
			cmd.reg = CommandRegister::GP0;
			cmd.gp0.type = GP0CommandType::ENV;
			cmd.gp0.env.type = EnvCommandType::TEXTURE_WINDOW;
			cmd.gp0.env.cmd = m_raw_conf.tex_window;
			cmd.from_prev_frame = true;
			commands.push_back(cmd);
		}
	}

	void Gpu::LoadStateConfiguration(std::vector<GPUCommand> const& commands) {
		for (auto const& cmd : commands) {
			LoadStateConfiguration(cmd);
		}
	}

	void Gpu::LoadStateConfiguration(GPUCommand const& cmd) {
		if (cmd.reg == CommandRegister::GP1) {
			switch (cmd.gp1.type) {
			case GP1CommandType::DISPLAY_ENABLE:
				DispEnable(cmd.gp1.disp_enable.display_on);
				if (m_recording_commands) {
					GPUCommand gpu_cmd{};
					gpu_cmd.value = 0;
					gpu_cmd.frame_of_recording = m_curr_vblank_count;
					gpu_cmd.reg = CommandRegister::GP1;
					gpu_cmd.gp1.type = GP1CommandType::DISPLAY_ENABLE;
					gpu_cmd.gp1.disp_enable.display_on = cmd.gp1.disp_enable.display_on;
					gpu_cmd.start_index = m_latest_idle_index;
					m_recorded_cmds.emplace_back(gpu_cmd);
				}
				break;
			case GP1CommandType::DISPLAY_MODE:
				DisplayMode(cmd.gp1.disp_mode.reg);
				break;
			case GP1CommandType::DMA_DIRECTION:
				DmaDirection(DmaDir(cmd.gp1.dma_dir.direction));
				if (m_recording_commands) {
					GPUCommand gpu_cmd{};
					gpu_cmd.value = 0;
					gpu_cmd.frame_of_recording = m_curr_vblank_count;
					gpu_cmd.reg = CommandRegister::GP1;
					gpu_cmd.gp1.type = GP1CommandType::DMA_DIRECTION;
					gpu_cmd.gp1.dma_dir.direction = cmd.gp1.dma_dir.direction;
					gpu_cmd.start_index = m_latest_idle_index;
					m_recorded_cmds.emplace_back(gpu_cmd);
				}
				break;
			case GP1CommandType::SET_VRAM_SIZE:
				break;
			default:
				break;
			}
		}
		else {
			if (cmd.gp0.type == GP0CommandType::ENV) {
				switch (cmd.gp0.env.type)
				{
				case EnvCommandType::SET_DRAW_TOP:
					DrawAreaTopLeft(cmd.gp0.env.cmd);
					break;
				case EnvCommandType::SET_DRAW_BOTTOM:
					DrawAreaBottomRight(cmd.gp0.env.cmd);
					break;
				case EnvCommandType::SET_DRAW_OFFSET:
					DrawOffset(cmd.gp0.env.cmd);
					break;
				case EnvCommandType::TEXTURE_PAGE:
					Texpage(cmd.gp0.env.cmd);
					break;
				case EnvCommandType::MASK_BIT:
					MaskSetting(cmd.gp0.env.cmd);
					break;
				case EnvCommandType::TEXTURE_WINDOW:
					TexWindow(cmd.gp0.env.cmd);
					break;
				default:
					break;
				}
			}
		}

		m_recorded_cmds.back().from_prev_frame = true;
	}

	void Gpu::StoreVram() {
		m_copied_vram.resize(VRAM_SIZE);
		m_renderer->FlushCommands();
		m_renderer->SyncTextures();
		m_renderer->VramCpuBlit(0, 0, VRAM_X_SIZE, VRAM_Y_SIZE);
		auto vram_ptr = m_renderer->GetVram().Get();
		std::copy_n(vram_ptr, VRAM_SIZE, m_copied_vram.data());
	}

	static const std::string IDENTIFIER = { "GPU-DUMP" };
	static constexpr u64 VERSION = 0x2;

	template <typename Ar>
	void Gpu::DumpRecordedCommands(Ar& ar) {
		ar(IDENTIFIER);
		ar(VERSION);

		ar(m_copied_vram);

		ar(m_recorded_cmds.size());
		ar(cereal::binary_data(m_recorded_cmds.data(), sizeof(GPUCommand) * m_recorded_cmds.size()));

		ar(m_recorded_gp_commands.size());
		ar(cereal::binary_data(m_recorded_gp_commands.data(), sizeof(RegisterCommand) *
			m_recorded_gp_commands.size()));
	}

	template <typename Ar>
	bool Gpu::LoadRecordedCommands(Ar& ar,
		std::vector<GPUCommand>& hle_view,
		std::vector<Gpu::RegisterCommand>& raw_data) {
		std::string ident{};
		u64 version{};
		ar(ident);
		ar(version);
		if (ident != IDENTIFIER) {
			LOG_ERROR("GPU", "[GPU] Load failed: expected identifier {}, got {}",
				IDENTIFIER, ident);
			return false;
		}

		if (version != VERSION) {
			LOG_ERROR("GPU", "[GPU] Load failed: expected version {:#x}, got {:#x}",
				VERSION, version);
			return false;
		}

		ar(m_copied_vram);

		m_renderer->BeginCpuVramBlit();
		m_renderer->PrepareBlit(false, false);
		auto vram_ptr = m_renderer->GetVram().Get();
		std::copy_n(m_copied_vram.data(), VRAM_SIZE, vram_ptr);
		m_renderer->CpuVramBlit(0, 0, VRAM_X_SIZE, VRAM_Y_SIZE);
		m_renderer->EndBlit();

		size_t num_commands{};
		size_t num_gp_commands{};

		ar(num_commands);
		hle_view.reserve(num_commands);
		for (size_t curr_cmd_index = 0; curr_cmd_index < num_commands; curr_cmd_index++) {
			GPUCommand cmd{};
			ar(cereal::binary_data(&cmd, sizeof(cmd)));
			hle_view.emplace_back(cmd);
		}

		ar(num_gp_commands);
		raw_data.resize(num_gp_commands);
		ar(cereal::binary_data(raw_data.data(), sizeof(RegisterCommand) * raw_data.size()));

		m_recorded_cmds.clear();
		m_recorded_gp_commands.clear();
		m_recorded_frames = 0;
		m_gp_commands_version++;
		m_latest_idle_index = 0;

		return true;
	}

	template void Gpu::DumpRecordedCommands<cereal::PortableBinaryOutputArchive>(cereal::PortableBinaryOutputArchive&);
	template bool Gpu::LoadRecordedCommands<cereal::PortableBinaryInputArchive>(cereal::PortableBinaryInputArchive&,
		std::vector<GPUCommand>&,
		std::vector<Gpu::RegisterCommand>&);
}