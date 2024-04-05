#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>

#include <fmt/format.h>

namespace psx {
	void Gpu::Reset() {
		fmt::println("[GPU] RESET");
		fmt::println("[GPU] Remember that RESET should also modify draw paramaters");

		ResetFifo();
		AckIrq();
		DispEnable(false);
		DmaDirection(DmaDir::OFF);
		DisplayAreaStart(0);


		constexpr u32 DISP_X = 0x200 | ((0x200 + 256 * 10) << 12);

		HorizontalDispRange(DISP_X);

		constexpr u32 Y_START = 0x10;
		constexpr u32 Y_END = 0x10 + 240;
		constexpr u32 DISP_Y = Y_START | (Y_END << 10);

		VerticalDispRange(DISP_Y);

		DisplayMode(0);
 	}

	void Gpu::ResetFifo() {
		m_cmd_fifo.clear();
		fmt::println("[GPU] FIFO Reset");
	}

	void Gpu::DispEnable(bool enable) {
		m_stat.disp_enable = enable;

		if (enable)
			fmt::println("[GPU] Display enabled");
		else
			fmt::println("[GPU] Display disabled");
	}

	void Gpu::DmaDirection(DmaDir dir) {
		m_stat.dma_dir = dir;

		switch (dir)
		{
		case psx::DmaDir::OFF:
			fmt::println("[GPU] DMA OFF");
			break;
		case psx::DmaDir::IDK:
			fmt::println("[GPU] DMA ?");
			break;
		case psx::DmaDir::CPU_GP0:
			fmt::println("[GPU] DMA CPU to GPU");
			break;
		case psx::DmaDir::GPUREAD_CPU:
			fmt::println("[GPU] DMA GPU to CPU");
			break;
		default:
			break;
		}
	}

	void Gpu::DisplayAreaStart(u32 cmd) {
		m_disp_x_start = cmd & 1023;
		m_disp_y_start = (cmd >> 10) & 511;

		fmt::println("[GPU] Display address X: 0x{:x}, Y: 0x{:x}",
			m_disp_x_start, m_disp_y_start);
	}

	void Gpu::HorizontalDispRange(u32 cmd) {
		m_hoz_disp_start = cmd & 0xFFF;
		m_hoz_disp_end = (cmd >> 12) & 0xFFF;

		fmt::println("[GPU] Display X1: 0x{:x}, X2: 0x{:x}",
			m_hoz_disp_start, m_hoz_disp_end);
	}

	void Gpu::VerticalDispRange(u32 cmd) {
		m_vert_disp_start = cmd & 0x3FF;
		m_vert_disp_end = (cmd >> 10) & 0x3FF;

		fmt::println("[GPU] Display Y1: 0x{:x}, Y2: 0x{:x}",
			m_vert_disp_start, m_vert_disp_end);
	}

	void Gpu::DisplayMode(u32 cmd) {
		m_stat.hoz_res1 = cmd & 3;
		m_stat.vertical_res = (cmd >> 2) & 1;
		m_stat.video_mode = (VideMode)((cmd >> 3) & 1);
		m_stat.disp_color_depth = (DisplayColorDepth)((cmd >> 4) & 1);
		m_stat.vertical_interlace = (cmd >> 5) & 1;
		m_stat.hoz_res_2 = (cmd >> 6) & 1;
		m_stat.flip_screen_hoz = (cmd >> 7) & 1;

		fmt::println("[GPU] DISPMODE(0x{:x})", cmd & 0xFF);

		if (m_stat.hoz_res_2) {
			fmt::println("        Horizontal res : 368");

			m_sys_status->sysbus->GetCounter0().SetDotclock(2);
		}
		else {
			u32 hoz_value = 0;

			switch (m_stat.hoz_res1)
			{
			case 0x0:
				hoz_value = 256;
				m_sys_status->sysbus->GetCounter0().SetDotclock(0);
				break;
			case 0x1:
				hoz_value = 320;
				m_sys_status->sysbus->GetCounter0().SetDotclock(1);
				break;
			case 0x2:
				hoz_value = 512;
				m_sys_status->sysbus->GetCounter0().SetDotclock(3);
				break;
			case 0x3:
				hoz_value = 640;
				m_sys_status->sysbus->GetCounter0().SetDotclock(4);
				break;
			default:
				break;
			}

			fmt::println("        Horizontal res : {}", hoz_value);
		}

		if (m_stat.vertical_res)
			fmt::println("        Vertical res : 480");
		else 
			fmt::println("        Vertical res : 240");

		if((u8)m_stat.video_mode)
			fmt::println("        Video mode : PAL");
		else 
			fmt::println("        Video mode : NTSC");

		if ((u8)m_stat.disp_color_depth)
			fmt::println("        Color depth : 24 bits");
		else
			fmt::println("        Color depth : 15 bits");

		fmt::println("        Vertical interlace : {}", m_stat.vertical_interlace);
	}

	void Gpu::GpuReadInternal(u32 cmd) {
		cmd &= 0x7;

		switch (cmd)
		{
		case 0x2:
			fmt::println("[GPU] Latch for Texture Window");
			m_gpu_read_latch = m_raw_conf.tex_window;
			break;
		case 0x3:
			fmt::println("[GPU] Latch for draw top-left");
			m_gpu_read_latch = m_raw_conf.draw_top_left;
			break;
		case 0x4:
			fmt::println("[GPU] Latch for draw bottom-right");
			m_gpu_read_latch = m_raw_conf.draw_bottom_right;
			break;
		case 0x5:
			fmt::println("[GPU] Latch for draw offset");
			m_gpu_read_latch = m_raw_conf.draw_offset;
			break;
		default:
			return;
			break;
		}

		m_read_status = GPUREAD_Status::READ_REG;
	}

	void Gpu::AckIrq() {
		m_stat.irq1 = false;
		fmt::println("[GPU] IRQ1 ACK");
	}
}