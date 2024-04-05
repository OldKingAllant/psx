#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/include/psxemu/GPUCommands.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/Interrupts.hpp>

#include <fmt/format.h>

#include <common/Errors.hpp>

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
		m_vert_disp_start{}, m_vert_disp_end{}, m_cmd_status{Status::IDLE}, 
		m_raw_conf{}, m_tex_x_flip{}, m_tex_y_flip{}, m_sys_status{sys_state}, 
		m_scanline{}, m_vblank{false} {
		m_cpu_vram = new u8[VRAM_SIZE];
	}

	void Gpu::WriteGP0(u32 value) {
		if ((value & 0xFF00'0000) == 0) {
			fmt::println("[GPU] NOP");
			return;
		}

		if (m_cmd_fifo.full()) {
			fmt::println("[GPU] Command FIFO full!");
			return;
		}

		switch (m_cmd_status)
		{
		case psx::Status::IDLE:
			CommandStart(value);
			break;
		case psx::Status::WAITING_PARAMETERS:
			break;
		case psx::Status::BUSY:
			break;
		default:
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
		case GPUREAD_Status::READ_VRAM:
			fmt::println("[GPU] Reading GPUREAD VRAM data is not implemented!");
			return 0;
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

		m_stat.send_vram_cpu = (m_cmd_status == Status::VRAM_GPU);

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
		if (m_cpu_vram)
			delete[] m_cpu_vram;
	}
}