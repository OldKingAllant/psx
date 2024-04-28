#pragma once

#include <common/Defs.hpp>
#include <common/Queue.hpp>

class DebugView;

namespace psx {
	static constexpr u64 GP0_ADD = 0x810;
	static constexpr u64 GP1_ADD = 0x814;

	static constexpr u64 VRAM_SIZE = (u64)1024 * 1024;

	namespace video {
		class Renderer;
	}

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
		BUSY,
		CPU_VRAM_BLIT,
		VRAM_CPU_BLIT
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

	struct RawDrawConfig {
		u32 texpage;
		u32 tex_window;
		u32 draw_top_left;
		u32 draw_bottom_right;
		u32 draw_offset;
		u32 mask_setting;
	};

	struct TextureWindow {
		u32 mask_x;
		u32 mask_y;
		u32 offset_x;
		u32 offset_y;
	};

	struct CpuVramBlit {
		u32 source_x;
		u32 source_y;
		u32 curr_x;
		u32 curr_y;
		u32 size_x;
		u32 size_y;
	};

	struct DisplayConfig {
		bool display_enable;
		u32 disp_x;
		u32 disp_y;
		u32 hoz_res;
		u32 vert_res;
	};

	static constexpr u32 VRAM_X_SIZE = 1024;
	static constexpr u32 VRAM_Y_SIZE = 512;

	struct system_status;

	class Gpu {
	public :
		Gpu(system_status* sys_status);
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

		friend void hblank_callback(void*, u64);
		friend void hblank_end_callback(void*, u64);

		void InitEvents();

		video::Renderer* GetRenderer() const {
			return m_renderer;
		}

		DisplayConfig& GetDispConfig() {
			return m_disp_conf;
		}

		friend class DebugView;

	private :
		void CommandStart(u32 cmd);
		void CommandEnd();

		void EnvCommand(u32 cmd);
		void MiscCommand(u32 cmd);

		void Texpage(u32 cmd);
		void DrawAreaTopLeft(u32 cmd);
		void DrawAreaBottomRight(u32 cmd);
		void DrawOffset(u32 cmd);
		void TexWindow(u32 cmd);
		void MaskSetting(u32 cmd);

		void UpdateDreq();

		void HBlank(u64 cycles_late);
		void HBlankEnd(u64 cycles_late);

	/// ///////////////////

		void DrawQuad();
		void DrawTriangle();

		void PerformCpuVramBlit(u32 data);

		void DrawFlatUntexturedOpaqueQuad();
		void DrawBasicGouraudQuad();
		void DrawBasicGouraudTriangle();
		void DrawTexturedQuad();

		void QuickFill();

		void CheckIfDrawNeeded();
		void FlushDrawOps();

		void FinalizeCpuVramBlit();

		void TryUpdateTexpage(u16 params);

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
		u32 m_x_top_left;
		u32 m_y_top_left;
		u32 m_x_bot_right;
		u32 m_y_bot_right;
		u32 m_x_off;
		u32 m_y_off;

		TextureWindow m_tex_win;

		Status m_cmd_status;

		RawDrawConfig m_raw_conf;

		bool m_tex_x_flip;
		bool m_tex_y_flip;

		system_status* m_sys_status;

		u32 m_scanline;
		bool m_vblank;

		u32 m_required_params;
		u32 m_rem_params;

		CpuVramBlit m_cpu_vram_blit;
		CpuVramBlit m_vram_cpu_blit;

		video::Renderer* m_renderer;

		DisplayConfig m_disp_conf;
	};
}