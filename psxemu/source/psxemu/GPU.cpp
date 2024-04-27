#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/include/psxemu/GPUCommands.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/Interrupts.hpp>

#include <fmt/format.h>

#include <common/Errors.hpp>

#include <psxemu/renderer/GLRenderer.hpp>

namespace psx {
	static constexpr u64 GPU_CLOCKS_FRAME = (u64)(VIDEO_CLOCK / (double)59.94);
	static constexpr u64 VISIBLE_LINE_START = 16;
	static constexpr u64 VISIBLE_LINE_END = 256;
	static constexpr u64 ACTIVE_CLOCKS = 1812;
	static constexpr u64 BLANKING_CLOCKS = 339;
	
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
		m_renderer{ nullptr }, m_disp_conf{} {
		m_renderer = new video::Renderer();
		m_cpu_vram = m_renderer->GetVramPtr();
	}

	void Gpu::WriteGP0(u32 value) {
		if (m_cmd_fifo.full()) {
			fmt::println("[GPU] Command FIFO full!");
			return;
		}

		switch (m_cmd_status)
		{
		case psx::Status::IDLE:
			if ((value & 0xFF00'0000) == 0) {
				fmt::println("[GPU] NOP");
				return;
			}

			CommandStart(value);
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
			fmt::println("[GPU] Ignoring GP0 command during VRAM-CPU blit");
		}
		break;
		default:
			error::DebugBreak();
			break;
		}

		UpdateDreq();
	}

	void Gpu::WriteGP1(u32 value) {
		u32 upper = (value >> 24) & 0xFF;

		switch (upper)
		{
		case 0x0:
			Reset();
			break;
		case 0x1:
			ResetFifo();
			break;
		case 0x2:
			AckIrq();
			break;
		case 0x3:
			DispEnable(value & 1);
			break;
		case 0x4:
			DmaDirection((DmaDir)(value & 3));
			break;
		case 0x5:
			DisplayAreaStart(value);
			break;
		case 0x6:
			HorizontalDispRange(value);
			break;
		case 0x7:
			VerticalDispRange(value);
			break;
		case 0x8:
			DisplayMode(value);
			break;
		default:
			fmt::println("[GPU] Unimplemented ENV command 0x{:x}", (u32)upper);
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
			|| (m_cmd_status == Status::WAITING_PARAMETERS);
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
		(void)m_sys_status->scheduler.Schedule(ACTIVE_CLOCKS, hblank_callback, this);
		(void)m_sys_status->scheduler.Schedule(CLOCKS_SCANLINE, hblank_end_callback, this);
	}

	void Gpu::HBlank(u64 cycles_late) {
		m_sys_status->sysbus->GetCounter0().HBlank();
		(void)m_sys_status->scheduler.Schedule(CLOCKS_SCANLINE - cycles_late, hblank_callback, this);
	}

	void Gpu::HBlankEnd(u64 cycles_late) {
		m_sys_status->sysbus->GetCounter0().HBlankEnd();

		if (m_stat.vertical_interlace && !m_stat.vertical_res && !m_vblank)
			m_stat.drawing_odd = !m_stat.drawing_odd;

		u32 prev_scanline = m_scanline;

		m_scanline++;

		if (m_scanline >= SCANLINES_FRAME)
			m_scanline = 0;

		if (!(m_scanline >= VISIBLE_LINE_START && m_scanline <= VISIBLE_LINE_END)
			&& (prev_scanline >= VISIBLE_LINE_START && prev_scanline <= VISIBLE_LINE_END)) {
			//VBLANK 
			m_sys_status->sysbus->GetCounter1().VBlank();
			m_stat.drawing_odd = false;
			m_vblank = true;
			m_sys_status->Interrupt((u32)Interrupts::VBLANK);
			m_sys_status->vblank = true;
			m_renderer->VBlank();
		}
		else if((m_scanline >= VISIBLE_LINE_START && m_scanline <= VISIBLE_LINE_END)
			&& !(prev_scanline >= VISIBLE_LINE_START && prev_scanline <= VISIBLE_LINE_END)
			&& m_vblank) {
			//Exiting VBlank
			m_sys_status->sysbus->GetCounter1().VBlankEnd();
			m_vblank = false;
		}
		else {
			if (m_stat.vertical_interlace && m_stat.vertical_res && !m_vblank)
				m_stat.drawing_odd = !m_stat.drawing_odd;
		}

		(void)m_sys_status->scheduler.Schedule(CLOCKS_SCANLINE - cycles_late, hblank_end_callback, this);
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

		m_renderer->PrepareBlit(m_stat.draw_over_mask_disable);

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
}