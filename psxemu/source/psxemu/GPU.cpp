#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/include/psxemu/GPUCommands.hpp>

#include <fmt/format.h>

namespace psx {
	Gpu::Gpu() :
		m_cmd_fifo{}, m_stat{},
		m_cpu_vram{ nullptr }, m_read_status{ GPUREAD_Status::NONE },
		m_gpu_read_latch{ 0 }, m_disp_x_start{}, 
		m_disp_y_start{}, m_hoz_disp_start{}, m_hoz_disp_end{},
		m_vert_disp_start{}, m_vert_disp_end{}, m_cmd_status{Status::IDLE} {
		m_cpu_vram = new u8[VRAM_SIZE];
	}

	void Gpu::WriteGP0(u32 value) {
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
	}

	void Gpu::WriteGP1(u32 value) {
		(void)value;
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

	Gpu::~Gpu() {
		if (m_cpu_vram)
			delete[] m_cpu_vram;
	}
}