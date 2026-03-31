#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/include/psxemu/GPUCommands.hpp>

#include <psxemu/renderer/GLRenderer.hpp>

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

		//m_raw_conf.draw_bottom_right
		//m_raw_conf.draw_offset;
		//m_raw_conf.draw_top_left;
		//m_raw_conf.mask_setting;
		//m_raw_conf.texpage;
		//m_raw_conf.tex_window;

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
		auto old_recording = m_recording_commands;
		m_recording_commands = false;
		for (auto const& cmd : commands) {
			if (cmd.reg == CommandRegister::GP1) {
				switch (cmd.gp1.type) {
				case GP1CommandType::DISPLAY_ENABLE:
					DispEnable(cmd.gp1.disp_enable.display_on);
					break;
				case GP1CommandType::DISPLAY_MODE:
					DisplayMode(cmd.gp1.disp_mode.reg);
					break;
				case GP1CommandType::DMA_DIRECTION:
					DmaDirection(DmaDir(cmd.gp1.dma_dir.direction));
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
		}
		m_recording_commands = old_recording;
	}

	void Gpu::StoreVram() {
		m_copied_vram.resize(VRAM_SIZE);
		m_renderer->FlushCommands();
		m_renderer->SyncTextures();
		m_renderer->VramCpuBlit(0, 0, VRAM_X_SIZE, VRAM_Y_SIZE);
		auto vram_ptr = m_renderer->GetVram().Get();
		std::copy_n(vram_ptr, VRAM_SIZE, m_copied_vram.data());
	}
}