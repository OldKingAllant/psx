#include <psxemu/include/psxemu/GPU.hpp>

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
	}

	void Gpu::DisplayAreaStart(u32 cmd) {
		m_disp_x_start = cmd & 1023;
		m_disp_y_start = (cmd >> 10) & 511;
	}

	void Gpu::HorizontalDispRange(u32 cmd) {
		m_hoz_disp_start = cmd & ((1 << 12) - 1);
		m_hoz_disp_end = (cmd >> 12) & ((1 << 11) - 1);
	}

	void Gpu::VerticalDispRange(u32 cmd) {
		m_vert_disp_start = cmd & ((1 << 10) - 1);
		m_vert_disp_end = (cmd >> 10) & ((1 << 10) - 1);
	}

	void Gpu::DisplayMode(u32 cmd) {
		m_stat.hoz_res1 = cmd & 3;
		m_stat.vertical_res = (cmd >> 2) & 1;
		m_stat.video_mode = (VideMode)((cmd >> 3) & 1);
		m_stat.disp_color_depth = (DisplayColorDepth)((cmd >> 4) & 1);
		m_stat.vertical_interlace = (cmd >> 5) & 1;
		m_stat.hoz_res_2 = (cmd >> 6) & 1;
		m_stat.flip_screen_hoz = (cmd >> 7) & 1;
	}

	void Gpu::GpuReadInternal(u32 cmd) {
		cmd &= 0x7;

		switch (cmd)
		{
		case 0x2:
			fmt::println("[GPU] Latch for Texture Window");
			m_gpu_read_latch = 0x0;
			break;
		case 0x3:
			fmt::println("[GPU] Latch for draw top-left");
			m_gpu_read_latch = 0x0;
			break;
		case 0x4:
			fmt::println("[GPU] Latch for draw bottom-right");
			m_gpu_read_latch = 0x0;
			break;
		case 0x5:
			fmt::println("[GPU] Latch for draw offset");
			m_gpu_read_latch = 0x0;
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