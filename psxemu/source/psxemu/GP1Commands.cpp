#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

namespace psx {
	void Gpu::Reset() {
		LOG_WARN("GPU", "[GPU] RESET");
		LOG_WARN("GPU", "[GPU] Remember that RESET should also modify draw paramaters");

		ResetFifo();
		AckIrq();
		DispEnable(true);
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
	}

	void Gpu::DispEnable(bool enable) {
		m_stat.disp_enable = enable;
		m_disp_conf.display_enable = !enable;

		if (!enable)
			LOG_INFO("GPU", "[GPU] Display enabled");
		else
			LOG_INFO("GPU", "[GPU] Display disabled");
	}

	void Gpu::DmaDirection(DmaDir dir) {
		m_stat.dma_dir = dir;

		switch (dir)
		{
		case psx::DmaDir::OFF:
			LOG_DEBUG("GPU", "[GPU] DMA OFF");
			break;
		case psx::DmaDir::IDK:
			LOG_DEBUG("GPU", "[GPU] DMA ?");
			break;
		case psx::DmaDir::CPU_GP0:
			LOG_DEBUG("GPU", "[GPU] DMA CPU to GPU");
			break;
		case psx::DmaDir::GPUREAD_CPU:
			LOG_DEBUG("GPU", "[GPU] DMA GPU to CPU");
			break;
		default:
			break;
		}
	}

	void Gpu::DisplayAreaStart(u32 cmd) {
		m_disp_x_start = cmd & 1023;
		m_disp_y_start = (cmd >> 10) & 511;

		m_disp_conf.disp_x = m_disp_x_start;
		m_disp_conf.disp_y = m_disp_y_start;

		LOG_DEBUG("GPU", "[GPU] Display address X: 0x{:x}, Y: 0x{:x}",
			m_disp_x_start, m_disp_y_start);
	}

	void Gpu::HorizontalDispRange(u32 cmd) {
		m_hoz_disp_start = cmd & 0xFFF;
		m_hoz_disp_end = (cmd >> 12) & 0xFFF;

		LOG_DEBUG("GPU", "[GPU] Display X1: 0x{:x}, X2: 0x{:x}",
			m_hoz_disp_start, m_hoz_disp_end);
	}

	void Gpu::VerticalDispRange(u32 cmd) {
		m_vert_disp_start = cmd & 0x3FF;
		m_vert_disp_end = (cmd >> 10) & 0x3FF;

		LOG_DEBUG("GPU", "[GPU] Display Y1: 0x{:x}, Y2: 0x{:x}",
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

		LOG_INFO("GPU", "[GPU] DISPMODE(0x{:x})", cmd & 0xFF);

		if (m_stat.hoz_res_2) {
			LOG_INFO("GPU", "        Horizontal res : 368");

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

			m_disp_conf.hoz_res = hoz_value;

			LOG_INFO("GPU", "        Horizontal res : {}", hoz_value);
		}

		if (m_stat.vertical_res) {
			LOG_INFO("GPU", "        Vertical res : 480");
			m_disp_conf.vert_res = 480;
		}	
		else {
			LOG_INFO("GPU", "        Vertical res : 240");
			m_disp_conf.vert_res = 240;
		}

		if((u8)m_stat.video_mode)
			LOG_INFO("GPU", "        Video mode : PAL");
		else 
			LOG_INFO("GPU", "        Video mode : NTSC");

		if ((u8)m_stat.disp_color_depth)
			LOG_INFO("GPU", "        Color depth : 24 bits");
		else
			LOG_INFO("GPU", "        Color depth : 15 bits");

		LOG_INFO("GPU", "        Vertical interlace : {}", m_stat.vertical_interlace);
	}

	void Gpu::GpuReadInternal(u32 cmd) {
		cmd &= 0x7;

		switch (cmd)
		{
		case 0x2:
			LOG_DEBUG("GPU", "[GPU] Latch for Texture Window");
			m_gpu_read_latch = m_raw_conf.tex_window;
			break;
		case 0x3:
			LOG_DEBUG("GPU", "[GPU] Latch for draw top-left");
			m_gpu_read_latch = m_raw_conf.draw_top_left;
			break;
		case 0x4:
			LOG_DEBUG("GPU", "[GPU] Latch for draw bottom-right");
			m_gpu_read_latch = m_raw_conf.draw_bottom_right;
			break;
		case 0x5:
			LOG_DEBUG("GPU", "[GPU] Latch for draw offset");
			m_gpu_read_latch = m_raw_conf.draw_offset;
			break;
		case 0x6:
		case 0x7:
			LOG_DEBUG("GPU", "[GPU] Read GPU version (returning v0)");
			break;
		default:
			return;
			break;
		}

		m_read_status = GPUREAD_Status::READ_REG;
	}

	void Gpu::AckIrq() {
		m_stat.irq1 = false;
		LOG_DEBUG("GPU", "[GPU] IRQ1 ACK");
	}
}