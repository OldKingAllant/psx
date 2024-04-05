#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/include/psxemu/GPUCommands.hpp>

#include <fmt/format.h>

#include <common/Errors.hpp>

namespace psx {

	void Gpu::EnvCommand(u32 cmd) {
		u8 upper_byte = (u8)(cmd >> 24);

		switch (upper_byte)
		{
		case 0xE1:
			Texpage(cmd);
			break;
		case 0xE2:
			TexWindow(cmd);
			break;
		case 0xE3:
			DrawAreaTopLeft(cmd);
			break;
		case 0xE4:
			DrawAreaBottomRight(cmd);
			break;
		case 0xE5:
			DrawOffset(cmd);
			break;
		default:
			fmt::println("[GPU] Unimplemented ENV command 0x{:x}", (u32)upper_byte);
			error::DebugBreak();
			break;
		}
	}

	void Gpu::Texpage(u32 cmd) {
		cmd &= (1 << 14) - 1;

		fmt::println("[GPU] TEXPAGE(0x{:x})", cmd);

		if (cmd == m_raw_conf.texpage)
			return;

		m_raw_conf.texpage = cmd;

		m_stat.texture_page_x_base = cmd & 0xF;
		m_stat.texture_page_y_base = (cmd >> 4) & 1;
		m_stat.semi_transparency = (SemiTransparency)((cmd >> 5) & 3);
		m_stat.tex_page_colors = (TexPageColors)((cmd >> 7) & 3);
		m_stat.dither = !!((cmd >> 9) & 1);
		m_stat.draw_to_display = !!((cmd >> 10) & 1);
		m_stat.texture_page_y_base2 = !!((cmd >> 11) & 1);
		
		m_tex_x_flip = !!((cmd >> 12) & 1);
		m_tex_y_flip = !!((cmd >> 13) & 1);

		fmt::println("          Page X base       0x{:x}", (u32)m_stat.texture_page_x_base * 64);
		fmt::println("          Page Y base 1     0x{:x}", (u32)m_stat.texture_page_y_base * 512);
		fmt::println("          Semi transparency 0x{:x}", (u32)m_stat.semi_transparency);
		fmt::println("          Tex page colors   0x{:x}", (u32)m_stat.tex_page_colors);
		fmt::println("          Dither            {}", m_stat.dither);
		fmt::println("          Draw to display   {}", m_stat.draw_to_display);
		fmt::println("          Page Y base 2     0x{:x}", (u32)m_stat.texture_page_y_base2 * 512);
		fmt::println("          Texture X flip    {}", m_tex_x_flip);
		fmt::println("          Texture Y flip    {}", m_tex_y_flip);
	}

	void Gpu::MiscCommand(u32 cmd) {
		u8 upper_byte = (u8)(cmd >> 24);

		switch (upper_byte)
		{
		case 0x0:
			fmt::println("[GPU] NOP");
			break;
		case 0x3:
			fmt::println("[GPU] NOP FIFO");
			break;
		default:
			fmt::println("[GPU] Unimplemented MISC command 0x{:x}", (u32)upper_byte);
			error::DebugBreak();
			break;
		}
	}

	void Gpu::CommandStart(u32 cmd) {
		CommandType cmd_type = (CommandType)((cmd >> 29) & 0x7);

		m_stat.recv_cmd_word = false;
		m_stat.recv_dma = false;
	
		switch (cmd_type)
		{
		case psx::CommandType::MISC:
			MiscCommand(cmd);
			break;
		case psx::CommandType::POLYGON:
			break;
		case psx::CommandType::LINE:
			break;
		case psx::CommandType::RECTANGLE:
			break;
		case psx::CommandType::VRAM_BLIT:
			break;
		case psx::CommandType::CPU_VRAM_BLIT:
			break;
		case psx::CommandType::VRAM_CPU_BLIT:
			break;
		case psx::CommandType::ENV:
			EnvCommand(cmd);
			break;
		default:
			fmt::println("[GPU] Invalid command type 0x{:x}", (u32)cmd_type);
			break;
		}
	}

	void Gpu::DrawAreaTopLeft(u32 cmd) {
		cmd &= 0xFFFFF;

		fmt::println("[GPU] DRAW_TOP_LEFT(0x{:x})", cmd);

		if (m_raw_conf.draw_top_left == cmd)
			return;

		m_raw_conf.draw_top_left = cmd;

		m_x_top_left = cmd & 1023;
		m_y_top_left = (cmd >> 10) & 511;

		fmt::println("      X = {}, Y = {}", m_x_top_left,
			m_y_top_left);
	}

	void Gpu::DrawAreaBottomRight(u32 cmd) {
		cmd &= 0xFFFFF;

		fmt::println("[GPU] DRAW_BOTTOM_RIGHT(0x{:x})", cmd);

		if (m_raw_conf.draw_bottom_right == cmd)
			return;

		m_raw_conf.draw_bottom_right = cmd;

		m_x_bot_right = cmd & 1023;
		m_y_bot_right = (cmd >> 10) & 511;

		fmt::println("      X = {}, Y = {}", m_x_bot_right,
			m_y_bot_right);
	}

	void Gpu::DrawOffset(u32 cmd) {
		cmd &= 0x1FFFFF;

		fmt::println("[GPU] DRAW_OFFSET(0x{:x})", cmd);

		if (cmd == m_raw_conf.draw_offset)
			return;

		m_raw_conf.draw_offset = cmd;

		m_x_off = cmd & 0x7FF;
		m_y_off = (cmd >> 11) & 0x7FF;

		m_x_off = (i32)m_x_off;
		m_y_off = (i32)m_y_off;

		fmt::println("      X = {}, Y = {}", (i32)m_x_off,
			(i32)m_y_off);
	}

	void Gpu::TexWindow(u32 cmd) {
		cmd &= 0xFFFFF;

		fmt::println("[GPU] TEX_WINDOW(0x{:x})", cmd);

		if (cmd == m_raw_conf.tex_window)
			return;

		m_raw_conf.tex_window = cmd;

		m_tex_win.mask_x = cmd & 0x1F;
		m_tex_win.mask_y = (cmd >> 5) & 0x1F;
		m_tex_win.offset_x = (cmd >> 10) & 0x1F;
		m_tex_win.offset_y = (cmd >> 15) & 0x1F;

		fmt::println("     Mask X   = {}", m_tex_win.mask_x);
		fmt::println("     Mask Y   = {}", m_tex_win.mask_y);
		fmt::println("     Offset X = {}", m_tex_win.offset_x);
		fmt::println("     Offset Y = {}", m_tex_win.offset_y);
	}
}