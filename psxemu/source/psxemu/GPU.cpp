#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/include/psxemu/GPUCommands.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/Interrupts.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <common/Errors.hpp>

#include <psxemu/renderer/GLRenderer.hpp>

namespace psx {
	
	Gpu::Gpu(system_status* sys_state) :
		m_cmd_fifo{}, m_stat{},
		m_cpu_vram{ nullptr }, m_read_status{ GPUREAD_Status::NONE },
		m_gpu_read_latch{ 0 }, m_disp_x_start{}, 
		m_disp_y_start{}, m_hoz_disp_start{}, m_hoz_disp_end{},
		m_vert_disp_start{}, m_vert_disp_end{},
		m_x_top_left{}, m_y_top_left{}, m_x_bot_right{},
		m_y_bot_right{}, m_x_off{}, m_y_off{}, m_tex_win{},
		m_cmd_status{Status::IDLE}, 
		m_raw_conf{}, m_tex_x_flip{}, m_tex_y_flip{}, m_sys_status{sys_state}, 
		m_scanline{}, m_vblank{ false }, m_required_params{}, 
		m_rem_params{}, m_cpu_vram_blit{}, m_vram_cpu_blit{},
		m_renderer{ nullptr }, m_disp_conf{}, m_raw_disp_conf{},
		m_last_event_timestamp {},
		m_curr_vblank_count{}, m_video_mode{ConsoleVideoMode::NTSC},
		m_recording_commands{}, m_recorded_cmds{},
		m_frames_to_record{}, m_recorded_frames{},
		m_copied_vram{}, m_recorded_gp_commands{},
		m_gp_commands_version{}, m_latest_idle_index{} {
		m_renderer = new video::Renderer();
		m_cpu_vram = m_renderer->GetVramPtr();
	}

	void Gpu::SetResolutionMultiplier(u32 mult) {
		m_renderer->SetResolutionMultiplier(mult);
	}

	void Gpu::WriteGP0(u32 value) {
		if (m_cmd_fifo.full()) {
			LOG_WARN("GPU", "[GPU] Command FIFO full!");
			return;
		}

		if (m_recording_commands) {
			if (m_cmd_status == Status::IDLE && GP0CommandType(value >> 29) != GP0CommandType::VRAM_CPU_BLIT) {
				m_latest_idle_index = m_recorded_gp_commands.size();
			}

			if (GP0CommandType(value >> 29) != GP0CommandType::VRAM_CPU_BLIT ||
				m_cmd_status != Status::IDLE) {
				RegisterCommand cmd{};
				cmd.reg_index = 0;
				cmd.value = value;
				m_recorded_gp_commands.push_back(cmd);
			}	
		}
		
		auto prev_status = m_cmd_status;

		switch (m_cmd_status)
		{
		case psx::Status::IDLE:
			if ((value & 0xFF00'0000) == 0) {
				LOG_DEBUG("GPU", "[GPU] NOP");
				if (m_recording_commands) {
					GPUCommand gpu_cmd{};
					gpu_cmd.value = value;
					gpu_cmd.frame_of_recording = m_curr_vblank_count;
					gpu_cmd.reg = CommandRegister::GP0;
					gpu_cmd.gp0.type = GP0CommandType::MISC;
					gpu_cmd.gp0.misc.type = MiscCommandType::NOP;
					gpu_cmd.gp0.misc.cmd = value;
					gpu_cmd.start_index = m_latest_idle_index;
					m_recorded_cmds.emplace_back(gpu_cmd);
				}
			}
			else {
				CommandStart(value);
			}
			break;
		case psx::Status::WAITING_PARAMETERS: {
			if (m_rem_params == 0)
				error::DebugBreak();

			m_rem_params -= 1;
			m_cmd_fifo.queue(value);

			if (m_rem_params == 0) {
				CommandEnd();
			}
		}
		break;
		case psx::Status::BUSY:
			break;
		case psx::Status::CPU_VRAM_BLIT: {
			PerformCpuVramBlit(value);
		}
		break;
		case psx::Status::VRAM_CPU_BLIT: {
			LOG_WARN("GPU", "[GPU] Ignoring GP0 command during VRAM-CPU blit");
		}
		break;
		case psx::Status::POLYLINE: {
			if ((value & 0xF000F000) == 0x50005000) {
				CommandEnd();
			}
			else {
				m_cmd_fifo.queue(value);
			}
		} 
		break;
		case psx::Status::POLYLINE_GOURAUD: {
			if ((m_cmd_fifo.len() & 1) == 0 && (value & 0xF000F000) == 0x50005000) {
				CommandEnd();
			}
			else {
				m_cmd_fifo.queue(value);
			}
		} break;
		default:
			error::DebugBreak();
			break;
		}

		if (m_cmd_status == Status::IDLE && m_cmd_fifo.len() > 0) {
			LOG_ERROR("GPU", "[GPU] Status is idle but command fifo is not empty");
			LOG_FLUSH();
		}

		if (m_recording_commands && m_cmd_status == Status::IDLE && 
			prev_status != Status::VRAM_CPU_BLIT) {
			RegisterCommand cmd{};
			cmd.reg_index = 0;
			cmd.value = 0;
			cmd.end_marker = true;
			cmd.end_reg_independent = false;
			m_recorded_gp_commands.push_back(cmd);
		}

		UpdateDreq();
	}

	void Gpu::WriteGP1(u32 value) {
		auto cmd = GP1CommandType((value >> 24) & 0xFF);

		if (m_recording_commands) {
			m_latest_idle_index = m_recorded_gp_commands.size();
		}

		if (m_recording_commands && cmd != GP1CommandType::READ_GPU_REGISTER) {
			RegisterCommand gp_cmd{};
			gp_cmd.reg_index = 1;
			gp_cmd.value = value;
			m_recorded_gp_commands.push_back(gp_cmd);
			gp_cmd.end_marker = true;
			if (cmd == GP1CommandType::RESET || cmd == GP1CommandType::RESET_CMD_FIFO) {
				gp_cmd.end_reg_independent = true;
			}
			m_recorded_gp_commands.push_back(gp_cmd);
		}
		
		switch (cmd)
		{
		case GP1CommandType::RESET: {
			auto old_recording = m_recording_commands;
			m_recording_commands = false;
			Reset();
			m_recording_commands = old_recording;
			if (m_recording_commands) {
				GPUCommand gpu_cmd{};
				gpu_cmd.value = value;
				gpu_cmd.frame_of_recording = m_curr_vblank_count;
				gpu_cmd.reg = CommandRegister::GP1;
				gpu_cmd.gp1.type = GP1CommandType::RESET;
				gpu_cmd.gp1.cmd = value;
				gpu_cmd.start_index = m_latest_idle_index;
				m_recorded_cmds.emplace_back(gpu_cmd);
			}
		} break;
		case GP1CommandType::RESET_CMD_FIFO:
			ResetFifo();
			if (m_recording_commands) {
				GPUCommand gpu_cmd{};
				gpu_cmd.value = value;
				gpu_cmd.frame_of_recording = m_curr_vblank_count;
				gpu_cmd.reg = CommandRegister::GP1;
				gpu_cmd.gp1.type = GP1CommandType::RESET_CMD_FIFO;
				gpu_cmd.gp1.cmd = value;
				gpu_cmd.start_index = m_latest_idle_index;
				m_recorded_cmds.emplace_back(gpu_cmd);
			}
			break;
		case GP1CommandType::IRQ_ACK:
			AckIrq();
			if (m_recording_commands) {
				GPUCommand gpu_cmd{};
				gpu_cmd.value = value;
				gpu_cmd.frame_of_recording = m_curr_vblank_count;
				gpu_cmd.reg = CommandRegister::GP1;
				gpu_cmd.gp1.type = GP1CommandType::IRQ_ACK;
				gpu_cmd.gp1.cmd = value;
				gpu_cmd.start_index = m_latest_idle_index;
				m_recorded_cmds.emplace_back(gpu_cmd);
			}
			break;
		case GP1CommandType::DISPLAY_ENABLE:
			DispEnable(value & 1);
			if (m_recording_commands) {
				GPUCommand gpu_cmd{};
				gpu_cmd.value = value;
				gpu_cmd.frame_of_recording = m_curr_vblank_count;
				gpu_cmd.reg = CommandRegister::GP1;
				gpu_cmd.gp1.type = GP1CommandType::DISPLAY_ENABLE;
				gpu_cmd.gp1.disp_enable.display_on = value & 1;
				gpu_cmd.start_index = m_latest_idle_index;
				m_recorded_cmds.emplace_back(gpu_cmd);
			}
			break;
		case GP1CommandType::DMA_DIRECTION:
			DmaDirection((DmaDir)(value & 3));
			if (m_recording_commands) {
				GPUCommand gpu_cmd{};
				gpu_cmd.value = value;
				gpu_cmd.frame_of_recording = m_curr_vblank_count;
				gpu_cmd.reg = CommandRegister::GP1;
				gpu_cmd.gp1.type = GP1CommandType::DMA_DIRECTION;
				gpu_cmd.gp1.dma_dir.direction = value & 3;
				gpu_cmd.start_index = m_latest_idle_index;
				m_recorded_cmds.emplace_back(gpu_cmd);
			}
			break;
		case GP1CommandType::DISPLAY_AREA_START:
			DisplayAreaStart(value);
			break;
		case GP1CommandType::HORIZONTAL_DISPLAY_RANGE:
			HorizontalDispRange(value);
			break;
		case GP1CommandType::VERTICAL_DISPLAY_RANGE:
			VerticalDispRange(value);
			break;
		case GP1CommandType::DISPLAY_MODE:
			DisplayMode(value);
			break;
		case GP1CommandType::READ_GPU_REGISTER:
			GpuReadInternal(value);
			if (m_recording_commands) {
				GPUCommand gpu_cmd{};
				gpu_cmd.value = value;
				gpu_cmd.frame_of_recording = m_curr_vblank_count;
				gpu_cmd.reg = CommandRegister::GP1;
				gpu_cmd.gp1.type = GP1CommandType::READ_GPU_REGISTER;
				gpu_cmd.gp1.cmd = value;
				gpu_cmd.start_index = m_latest_idle_index;
				m_recorded_cmds.emplace_back(gpu_cmd);
			}
			break;
		default:
			LOG_ERROR("GPU", "[GPU] Unimplemented ENV command 0x{:x}", (u32)cmd);
			error::DebugBreak();
			break;
		}

		UpdateDreq();
	}

	u32 Gpu::ReadData() {
		switch (m_read_status)
		{
		case GPUREAD_Status::READ_REG:
			return m_gpu_read_latch;
		case GPUREAD_Status::READ_VRAM: {
			u32 read_val = 0;

			u32 end_x = (m_vram_cpu_blit.source_x +
				m_vram_cpu_blit.size_x) % VRAM_X_SIZE;
			u32 end_y = (m_vram_cpu_blit.source_y +
				m_vram_cpu_blit.size_y) % VRAM_Y_SIZE;

			u32 index = (m_vram_cpu_blit.curr_y * VRAM_X_SIZE) +
				m_vram_cpu_blit.curr_x;

			u16* ram_view = std::bit_cast<u16*>(m_cpu_vram);

			read_val |= ram_view[index];

			m_vram_cpu_blit.curr_x += 1;
			m_vram_cpu_blit.curr_x %= VRAM_X_SIZE;

			if (m_vram_cpu_blit.curr_x == end_x) {
				m_vram_cpu_blit.curr_x = m_vram_cpu_blit.source_x;

				m_vram_cpu_blit.curr_y += 1;
				m_vram_cpu_blit.curr_y %= VRAM_Y_SIZE;

				if (m_vram_cpu_blit.curr_y == end_y) {
					m_cmd_status = Status::IDLE;
					m_read_status = GPUREAD_Status::NONE;
					return read_val;
				}
			}

			if ((u64)index + 1 < VRAM_SIZE / 2)
				read_val |= ((u32)ram_view[index + 1] << 16);

			m_vram_cpu_blit.curr_x += 1;
			m_vram_cpu_blit.curr_x %= VRAM_X_SIZE;

			if (m_vram_cpu_blit.curr_x == end_x) {
				m_vram_cpu_blit.curr_x = m_vram_cpu_blit.source_x;

				m_vram_cpu_blit.curr_y += 1;
				m_vram_cpu_blit.curr_y %= VRAM_Y_SIZE;

				if (m_vram_cpu_blit.curr_y == end_y) {
					m_cmd_status = Status::IDLE;
					m_read_status = GPUREAD_Status::NONE;
				}
			}

			return read_val;
		}
		default:
			break;
		}

		return 0;
	}

	u32 Gpu::ReadStat() {
		u32 out = 0;

		if (!m_stat.vertical_interlace)
			m_stat.interlace_field = true;

		UpdateDreq();

		//I absolutely hate this thing, however
		//I don't have any other choice thanks
		//to the compiler

		out |= (m_stat.texture_page_x_base & 0xF);
		out |= (u32)(m_stat.texture_page_y_base & 1) << 4;
		out |= ((u32)m_stat.semi_transparency) << 5;
		out |= ((u32)m_stat.tex_page_colors) << 7;
		out |= ((u32)m_stat.dither) << 9;
		out |= ((u32)m_stat.draw_to_display) << 10;
		out |= ((u32)m_stat.set_mask) << 11;
		out |= ((u32)m_stat.draw_over_mask_disable) << 12;
		out |= ((u32)m_stat.interlace_field) << 13;
		out |= ((u32)m_stat.flip_screen_hoz) << 14;
		out |= ((u32)m_stat.texture_page_y_base2) << 15;
		out |= ((u32)m_stat.hoz_res_2) << 16;
		out |= ((u32)m_stat.hoz_res1) << 17;
		out |= ((u32)m_stat.vertical_res) << 19;
		out |= ((u32)m_stat.video_mode) << 20;
		out |= ((u32)m_stat.disp_color_depth) << 21;
		out |= ((u32)m_stat.vertical_interlace) << 22;
		out |= ((u32)m_stat.disp_enable) << 23;
		out |= ((u32)m_stat.irq1) << 24;
		out |= ((u32)m_stat.dreq) << 25;
		out |= ((u32)m_stat.recv_cmd_word) << 26;
		out |= ((u32)m_stat.send_vram_cpu) << 27;
		out |= ((u32)m_stat.recv_dma) << 28;
		out |= ((u32)m_stat.dma_dir) << 29;
		out |= ((u32)m_stat.drawing_odd) << 31;

		return out;
	}

	void Gpu::UpdateDreq() {
		m_stat.recv_dma = (m_cmd_status == Status::IDLE)
			|| (m_cmd_status == Status::WAITING_PARAMETERS)
			|| (m_cmd_status == Status::CPU_VRAM_BLIT) 
			|| (m_cmd_status == Status::VRAM_CPU_BLIT);
		m_stat.recv_cmd_word = (m_cmd_status == Status::IDLE);

		m_stat.send_vram_cpu = (m_cmd_status == Status::VRAM_CPU_BLIT);

		bool dreq = m_stat.dreq;

		switch (m_stat.dma_dir)
		{
		case DmaDir::OFF:
			m_stat.dreq = false;
			break;
		case DmaDir::IDK:
			m_stat.dreq = !m_cmd_fifo.full();
			break;
		case DmaDir::CPU_GP0:
			m_stat.dreq = m_stat.recv_dma;
			break;
		case DmaDir::GPUREAD_CPU:
			m_stat.dreq = m_stat.send_vram_cpu;
			break;
		default:
			break;
		}

		if (!dreq && m_stat.dreq) {
			//Trigger DMA transfers
			m_sys_status->sysbus->GetDMAControl()
				.GetGpuDma().DreqRisingEdge();
		}

		m_sys_status->sysbus->GetDMAControl()
				.GetGpuDma().SetDreq(m_stat.dreq);
	}

	void hblank_callback(void* gpu, u64 cycles_late) {
		std::bit_cast<Gpu*>(gpu)->HBlank(cycles_late);
	}

	void hblank_end_callback(void* gpu, u64 cycles_late) {
		std::bit_cast<Gpu*>(gpu)->HBlankEnd(cycles_late);
	}

	void Gpu::InitEvents() {
		m_last_event_timestamp = m_video_mode == ConsoleVideoMode::NTSC ? ACTIVE_CLOCKS_NTSC :
			ACTIVE_CLOCKS_PAL;
		(void)m_sys_status->scheduler.ScheduleAbsolute(m_last_event_timestamp, hblank_callback, this);
	}

	void Gpu::HBlank(u64 cycles_late) {
		m_sys_status->sysbus->GetCounter0().HBlank();
		m_sys_status->sysbus->GetCounter1().UpdateFromTimestamp();
		m_last_event_timestamp += m_video_mode == ConsoleVideoMode::NTSC ? (CLOCKS_SCANLINE_NTSC - ACTIVE_CLOCKS_NTSC) :
			(CLOCKS_SCANLINE_PAL - ACTIVE_CLOCKS_PAL);
		(void)m_sys_status->scheduler.ScheduleAbsolute(
			m_last_event_timestamp, 
			hblank_end_callback, 
			this);
	}

	void Gpu::HBlankEnd(u64 cycles_late) {
		m_sys_status->sysbus->GetCounter0().HBlankEnd();

		m_scanline++;

		if (m_scanline == 1 && m_recording_commands) {
			m_recorded_frames++;
			if (m_recorded_frames >= m_frames_to_record) {
				m_recorded_cmds.clear();
				m_recorded_gp_commands.clear();
				m_recorded_frames = 0;
				PushStateConfiguration(m_recorded_cmds);
				StoreVram();
				m_gp_commands_version++;
			}
		}

		auto scanlines_frame = SCANLINES_FRAME_NTSC;
		auto visible_lines_start = VISIBLE_LINE_START_NTSC;
		auto visible_lines_end = VISIBLE_LINE_END_NTSC;
		auto active_clocks = ACTIVE_CLOCKS_NTSC;

		if (m_video_mode == ConsoleVideoMode::PAL) {
			scanlines_frame = SCANLINES_FRAME_PAL;
			visible_lines_start = VISIBLE_LINE_START_PAL;
			visible_lines_end = VISIBLE_LINE_END_PAL;
			active_clocks = ACTIVE_CLOCKS_PAL;
		}

		if (m_scanline >= scanlines_frame) {
			m_curr_vblank_count += 1;
			m_scanline = 0;
			m_sys_status->sysbus->GetCounter1().VBlank();
			m_stat.drawing_odd = false;
			m_vblank = true;
			m_sys_status->Interrupt((u32)Interrupts::VBLANK);
			m_sys_status->vblank = true;
			m_renderer->VBlank();
		}
		else if (m_scanline >= visible_lines_start && m_scanline <= visible_lines_end) {
			//With v-res 240, changes per scanline
			if (m_stat.vertical_interlace && !m_stat.vertical_res) {
				m_stat.drawing_odd = (m_scanline & 1) != 0;
			}
		}
		else if (m_scanline == visible_lines_start - 1) {
			m_sys_status->sysbus->GetCounter1().VBlankEnd();
			m_vblank = false;

			//Changes per frame when v-res is 480
			if (m_stat.vertical_interlace && m_stat.vertical_res) {
				m_stat.drawing_odd = !m_stat.drawing_odd;
			}
		}

		m_last_event_timestamp += active_clocks;
		(void)m_sys_status->scheduler.ScheduleAbsolute(m_last_event_timestamp, 
			hblank_callback, this);
	}

	Gpu::~Gpu() {
		if (m_renderer)
			delete m_renderer;
	}

	void Gpu::FinalizeCpuVramBlit() {
		u32 end_x = (m_cpu_vram_blit.source_x +
			m_cpu_vram_blit.size_x);
		u32 end_y = (m_cpu_vram_blit.source_y +
			m_cpu_vram_blit.size_y);

		//If sourcex + w > 1024 || sourcey + h > 512
		//the coordinates should wrap around
		//This cannot be replicated directly
		//in OpenGL without using geometry
		//shaders (since we would need to generate
		//new vertices for the second part of the
		//blit). For this reason we do it here

		//Draw full or first part of the blit
		//Vertices outside of the 1024x512 viewport
		//are simply clipped

		m_renderer->PrepareBlit(m_stat.draw_over_mask_disable,
			m_stat.set_mask);

		m_renderer->CpuVramBlit(
			m_cpu_vram_blit.source_x,
			m_cpu_vram_blit.source_y,
			m_cpu_vram_blit.size_x,
			m_cpu_vram_blit.size_y
		);

		if (end_x >= VRAM_X_SIZE && end_y >= VRAM_Y_SIZE) {
			u32 start_x = m_cpu_vram_blit.source_x;
			u32 start_y = m_cpu_vram_blit.source_y;

			u32 size_x = (end_x % VRAM_X_SIZE);
			u32 size_y = (end_y % VRAM_Y_SIZE);

			//There are 4 split parts in total:
			//1. (X, Y) Both in bounds
			//2. X out of bounds
			//3. Y out of bounds
			//4. (X, Y) Both out of bounds
			//And we need to draw 4 different quads

			m_renderer->CpuVramBlit(
				0, m_cpu_vram_blit.source_y, 
				size_x, m_cpu_vram_blit.size_y
			);

			m_renderer->CpuVramBlit(
				m_cpu_vram_blit.source_x, 0,
				m_cpu_vram_blit.size_x, size_y
			);

			m_renderer->CpuVramBlit(
				0, 0,
				size_x, size_y
			);
		}
		else if (end_x >= VRAM_X_SIZE || end_y >= VRAM_Y_SIZE) {
			//Emulate wraparound from 1 single side
			u32 start_x = (end_x >= VRAM_X_SIZE) ? 0 :
				m_cpu_vram_blit.source_x;
			u32 start_y = (end_y >= VRAM_Y_SIZE) ? 0 :
				m_cpu_vram_blit.source_y;

			u32 size_x = (end_x % VRAM_X_SIZE) - start_x;
			u32 size_y = (end_y % VRAM_Y_SIZE) - start_y;

			m_renderer->CpuVramBlit(start_x, start_y, size_x, size_y);
		}

		m_renderer->EndBlit();
	}

	void Gpu::PerformCpuVramBlit(u32 data) {
		u32 end_x = (m_cpu_vram_blit.source_x +
			m_cpu_vram_blit.size_x) % VRAM_X_SIZE;
		u32 end_y = (m_cpu_vram_blit.source_y +
			m_cpu_vram_blit.size_y) % VRAM_Y_SIZE;

		u32 curr_index = (m_cpu_vram_blit.curr_y * VRAM_X_SIZE) +
			m_cpu_vram_blit.curr_x;

		curr_index *= 2;

		*reinterpret_cast<u16*>(m_cpu_vram + curr_index) = (u16)(data);

		m_cpu_vram_blit.curr_x += 1;
		m_cpu_vram_blit.curr_x %= VRAM_X_SIZE;

		if (m_cpu_vram_blit.curr_x == end_x) {
			m_cpu_vram_blit.curr_x = m_cpu_vram_blit.source_x;

			m_cpu_vram_blit.curr_y += 1;
			m_cpu_vram_blit.curr_y %= VRAM_Y_SIZE;

			curr_index = (m_cpu_vram_blit.curr_y * VRAM_X_SIZE) +
				m_cpu_vram_blit.curr_x;
			curr_index *= 2;
			
			if (m_cpu_vram_blit.curr_y == end_y) {
				m_cmd_status = Status::IDLE;
				FinalizeCpuVramBlit();
				return;
			}
		}
		else {
			curr_index += 2;
		}

		*reinterpret_cast<u16*>(m_cpu_vram + curr_index) = (u16)(data >> 16);

		m_cpu_vram_blit.curr_x += 1;
		m_cpu_vram_blit.curr_x %= VRAM_X_SIZE;

		if (m_cpu_vram_blit.curr_x == end_x) {
			m_cpu_vram_blit.curr_x = m_cpu_vram_blit.source_x;

			m_cpu_vram_blit.curr_y += 1;
			m_cpu_vram_blit.curr_y %= VRAM_Y_SIZE;

			if (m_cpu_vram_blit.curr_y == end_y) {
				m_cmd_status = Status::IDLE;
				FinalizeCpuVramBlit();
			}
		}
	}

	void Gpu::TryUpdateTexpage(u16 params) {
		u32 x_base = params & 0xF;
		u32 y_base = (params >> 4) & 1;
		u32 semi_trans = (params >> 5) & 3;
		u32 texpage_colors = (params >> 7) & 0x3;
		u32 y_base2 = (params >> 11) & 1;

		m_stat.texture_page_x_base = (u8)x_base;
		m_stat.texture_page_y_base = (u8)y_base;
		m_stat.semi_transparency = (SemiTransparency)semi_trans;
		m_stat.tex_page_colors = (TexPageColors)texpage_colors;
		m_stat.texture_page_y_base2 = (bool)y_base2;
	}

	DisplayRange Gpu::ComputeDisplayRange() const {
		u32 size = (m_vert_disp_end - m_vert_disp_start);
		if (m_stat.vertical_interlace)
			size *= 2;
		DisplayRange range{};
		range.x = 0;
		range.xsize = m_disp_conf.hoz_res;
		range.y = m_vert_disp_start;
		range.ysize = size;
		return range;
	}

	u64 Gpu::GetNextVBlankTime() const {
		auto scanlines_frame = m_video_mode == ConsoleVideoMode::NTSC ?
			SCANLINES_FRAME_NTSC : SCANLINES_FRAME_PAL;
		auto clocks_scanline = m_video_mode == ConsoleVideoMode::NTSC ?
			CLOCKS_SCANLINE_NTSC : CLOCKS_SCANLINE_PAL;

		u64 rem_scanlines = (scanlines_frame - m_scanline) - 1;
		u64 curr_time = m_sys_status->scheduler.GetTimestamp();

		if (curr_time > m_last_event_timestamp) {
			LOG_ERROR("GPU", "[GPU] SCHEDULER TIME < LAST EVENT TIME");
			LOG_FLUSH();
			error::DebugBreak();
		}

		u64 total_time = (m_last_event_timestamp - curr_time) +
			rem_scanlines * clocks_scanline;

		return total_time;
	}
}