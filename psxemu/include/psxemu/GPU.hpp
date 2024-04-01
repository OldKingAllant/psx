#pragma once

#include <common/Defs.hpp>
#include <common/Queue.hpp>

namespace psx {
	static constexpr u64 GP0_ADD = 0x810;
	static constexpr u64 GP1_ADD = 0x814;

	static constexpr u64 VRAM_SIZE = (u64)1024 * 1024;

	enum class SemiTransparency : u8 {
		B05_PLUS_F05,
		B_PLUS_F,
		B_MINUS_F,
		B_PLUS_F025
	};

	enum class TexPageColors : u8 {
		BITS4,
		BITS8,
		BITS15,
		RESERVED
	};

	enum class VideMode : u8 {
		NTSC,
		PAL
	};

	enum class DisplayColorDepth : u8 {
		BITS15,
		BITS24
	};

	enum class DmaDir : u8 {
		OFF,
		IDK,
		CPU_GP0,
		GPUREAD_CPU
	};

	enum class GPUREAD_Status {
		READ_REG,
		READ_VRAM,
		NONE
	};

	enum class Status {
		IDLE,
		WAITING_PARAMETERS,
		BUSY
	};

	/// <summary>
	/// It would have been better to put
	/// everything in a union with packed
	/// bitfields, but the MSVC compiler
	/// would not pack it in 4 bytes, which
	/// renders the union and bitfields completely
	/// useless
	/// </summary>
	struct GpuStat {
		u8 texture_page_x_base;
		u8 texture_page_y_base;
		SemiTransparency semi_transparency;
		TexPageColors tex_page_colors;
		bool dither;
		bool draw_to_display;
		bool set_mask;
		bool draw_over_mask_disable;
		bool interlace_field;
		bool flip_screen_hoz;
		bool texture_page_y_base2;
		bool hoz_res_2;
		u8 hoz_res1;
		bool vertical_res;
		VideMode video_mode;
		DisplayColorDepth disp_color_depth;
		bool vertical_interlace;
		bool disp_enable;
		bool irq1;
		bool dreq;
		bool recv_cmd_word;
		bool send_vram_cpu;
		bool recv_dma;
		DmaDir dma_dir;
		bool drawing_odd;
	};

	class Gpu {
	public :
		Gpu();
		~Gpu();

		void WriteGP0(u32 value);
		void WriteGP1(u32 value);

		u32 ReadData();
		u32 ReadStat();

		void Reset();

		void DispEnable(bool enable);
		void DmaDirection(DmaDir dir);
		void DisplayAreaStart(u32 cmd);
		void HorizontalDispRange(u32 cmd);
		void VerticalDispRange(u32 cmd);
		void DisplayMode(u32 cmd);
		void GpuReadInternal(u32 cmd);
		void ResetFifo();
		void AckIrq();

	private :
		void CommandStart(u32 cmd);

		void EnvCommand(u32 cmd);

		void Texpage(u32 cmd);

	private :
		Queue<u32, 16> m_cmd_fifo;
		GpuStat m_stat;
		GPUREAD_Status m_read_status;
		u32 m_gpu_read_latch;

		//The name refers to the fact that
		//this is a copy of the VRAM that
		//resides on the CPU.
		u8* m_cpu_vram;

		u32 m_disp_x_start;
		u32 m_disp_y_start;
		u32 m_hoz_disp_start;
		u32 m_hoz_disp_end;
		u32 m_vert_disp_start;
		u32 m_vert_disp_end;

		Status m_cmd_status;
	};
}