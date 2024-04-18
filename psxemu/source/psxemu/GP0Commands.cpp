#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/include/psxemu/GPUCommands.hpp>

#include <fmt/format.h>

#include <common/Errors.hpp>

#include <psxemu/renderer/GLRenderer.hpp>

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
		case 0xE6:
			MaskSetting(cmd);
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

		FlushDrawOps();

		m_raw_conf.texpage = cmd;

		m_stat.texture_page_x_base = cmd & 0xF;
		m_stat.texture_page_y_base = (cmd >> 4) & 1;
		m_stat.semi_transparency = (SemiTransparency)((cmd >> 5) & 3);
		m_stat.tex_page_colors = (TexPageColors)((cmd >> 7) & 3);
		m_stat.dither = (bool)((cmd >> 9) & 1);
		m_stat.draw_to_display = (bool)((cmd >> 10) & 1);
		m_stat.texture_page_y_base2 = (bool)((cmd >> 11) & 1);
		
		m_tex_x_flip = (bool)((cmd >> 12) & 1);
		m_tex_y_flip = (bool)((cmd >> 13) & 1);

		fmt::println("          Page X base       0x{:x}", (u32)m_stat.texture_page_x_base);
		fmt::println("          Page Y base 1     0x{:x}", (u32)m_stat.texture_page_y_base);
		fmt::println("          Semi transparency 0x{:x}", (u32)m_stat.semi_transparency);
		fmt::println("          Tex page colors   0x{:x}", (u32)m_stat.tex_page_colors);
		fmt::println("          Dither            {}", m_stat.dither);
		fmt::println("          Draw to display   {}", m_stat.draw_to_display);
		fmt::println("          Page Y base 2     0x{:x}", (u32)m_stat.texture_page_y_base2);
		fmt::println("          Texture X flip    {}", m_tex_x_flip);
		fmt::println("          Texture Y flip    {}", m_tex_y_flip);

		m_renderer->GetUniformBuffer()
			.use_dither = m_stat.dither;
		m_renderer->RequestUniformBufferUpdate();
	}

	void Gpu::MiscCommand(u32 cmd) {
		u8 upper_byte = (u8)(cmd >> 24);

		switch (upper_byte)
		{
		case 0x0:
			fmt::println("[GPU] NOP");
			break;
		case 0x1:
			fmt::println("[GPU] CLEAR TEXTURE CACHE");
			FlushDrawOps();
			m_renderer->SyncTextures();
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
		case psx::CommandType::POLYGON: {
			bool quad = (cmd >> 27) & 1;
			bool tex = (cmd >> 26) & 1;
			bool gouraud = (cmd >> 28) & 1;

			u32 num_vertex = quad ? 4 : 3;
			u32 params_vert = 1;

			if (tex)
				params_vert += 1;
			if (gouraud)
				params_vert += 1;

			m_required_params = num_vertex * params_vert;

			if (gouraud) //The color of the first vertex
						 //is contained in the command
						 //itself
				m_required_params -= 1;

			m_rem_params = m_required_params;

			m_cmd_fifo.queue(cmd);
			m_cmd_status = Status::WAITING_PARAMETERS;
		}
			break;
		case psx::CommandType::LINE:
			error::DebugBreak();
			break;
		case psx::CommandType::RECTANGLE:
			error::DebugBreak();
			break;
		case psx::CommandType::VRAM_BLIT:
			error::DebugBreak();
			break;
		case psx::CommandType::CPU_VRAM_BLIT: {
			m_cmd_fifo.queue(cmd);
			m_cmd_status = Status::WAITING_PARAMETERS;
			m_required_params = 2;
			m_rem_params = 2;
		}
			break;
		case psx::CommandType::VRAM_CPU_BLIT: {
			m_cmd_fifo.queue(cmd);
			m_cmd_status = Status::WAITING_PARAMETERS;
			m_required_params = 2;
			m_rem_params = 2;
		}
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

		FlushDrawOps();

		m_raw_conf.draw_top_left = cmd;

		m_x_top_left = cmd & 1023;
		m_y_top_left = (cmd >> 10) & 511;

		fmt::println("      X = {}, Y = {}", m_x_top_left,
			m_y_top_left);

		m_renderer->SetScissorTop(m_x_top_left, m_y_top_left);
	}

	void Gpu::DrawAreaBottomRight(u32 cmd) {
		cmd &= 0xFFFFF;

		fmt::println("[GPU] DRAW_BOTTOM_RIGHT(0x{:x})", cmd);

		if (m_raw_conf.draw_bottom_right == cmd)
			return;

		FlushDrawOps();

		m_raw_conf.draw_bottom_right = cmd;

		m_x_bot_right = cmd & 1023;
		m_y_bot_right = (cmd >> 10) & 511;

		fmt::println("      X = {}, Y = {}", m_x_bot_right,
			m_y_bot_right);

		m_renderer->SetScissorBottom(m_x_bot_right, m_y_bot_right);
	}

	void Gpu::DrawOffset(u32 cmd) {
		cmd &= 0x1FFFFF;

		fmt::println("[GPU] DRAW_OFFSET(0x{:x})", cmd);

		if (cmd == m_raw_conf.draw_offset)
			return;

		m_raw_conf.draw_offset = cmd;

		FlushDrawOps();

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

		FlushDrawOps();

		m_tex_win.mask_x = cmd & 0x1F;
		m_tex_win.mask_y = (cmd >> 5) & 0x1F;
		m_tex_win.offset_x = (cmd >> 10) & 0x1F;
		m_tex_win.offset_y = (cmd >> 15) & 0x1F;

		fmt::println("     Mask X   = {}", m_tex_win.mask_x);
		fmt::println("     Mask Y   = {}", m_tex_win.mask_y);
		fmt::println("     Offset X = {}", m_tex_win.offset_x);
		fmt::println("     Offset Y = {}", m_tex_win.offset_y);
	}

	void Gpu::MaskSetting(u32 cmd) {
		cmd &= 3;

		fmt::println("[GPU] MASK_BIT(0x{:x})", cmd);

		bool old_set = m_stat.set_mask;
		bool old_siable = m_stat.draw_over_mask_disable;

		m_stat.set_mask = (bool)(cmd & 1);
		m_stat.draw_over_mask_disable = (bool)((cmd >> 1) & 1);

		FlushDrawOps();

		fmt::println("      Force bit 15 to 1      = {}", m_stat.set_mask);
		fmt::println("      Check mask before draw = {}", m_stat.draw_over_mask_disable);
	}

	void Gpu::CommandEnd() {
		if (m_cmd_fifo.empty())
			error::DebugBreak();

		u32 cmd = m_cmd_fifo.peek();

		CommandType cmd_type = (CommandType)((cmd >> 29) & 0x7);

		switch (cmd_type)
		{
		case psx::CommandType::MISC:
			error::DebugBreak();
			break;
		case psx::CommandType::POLYGON: {
			bool quad = (cmd >> 27) & 1;

			if (quad)
				DrawQuad();
			else
				DrawTriangle();

			m_cmd_status = Status::IDLE;
		}
			break;
		case psx::CommandType::LINE:
			error::DebugBreak();
			break;
		case psx::CommandType::RECTANGLE:
			error::DebugBreak();
			break;
		case psx::CommandType::VRAM_BLIT:
			error::DebugBreak();
			break;
		case psx::CommandType::CPU_VRAM_BLIT: {
			u32 cmd = m_cmd_fifo.deque();
			u32 source = m_cmd_fifo.deque();
			u32 size = m_cmd_fifo.deque();

			m_cpu_vram_blit.source_x = source & 0xFFFF;
			m_cpu_vram_blit.source_y = (source >> 16) & 0xFFFF;
			m_cpu_vram_blit.size_x = size & 0xFFFF;
			m_cpu_vram_blit.size_y = (size >> 16) & 0xFFFF;

			m_cpu_vram_blit.source_x &= VRAM_X_SIZE - 1;
			m_cpu_vram_blit.source_y &= VRAM_Y_SIZE - 1;

			if (m_cpu_vram_blit.size_x == 0)
				m_cpu_vram_blit.size_x = VRAM_X_SIZE;
			else {
				m_cpu_vram_blit.size_x =
					((m_cpu_vram_blit.size_x - 1) & (VRAM_X_SIZE - 1)) + 1;
			}

			if (m_cpu_vram_blit.size_y == 0)
				m_cpu_vram_blit.size_y = VRAM_Y_SIZE;
			else {
				m_cpu_vram_blit.size_y =
					((m_cpu_vram_blit.size_y - 1) & (VRAM_Y_SIZE - 1)) + 1;
			}

			m_cpu_vram_blit.curr_x = m_cpu_vram_blit.source_x;
			m_cpu_vram_blit.curr_y = m_cpu_vram_blit.source_y;

			fmt::println("[GPU] CPU-VRAM BLIT");
			fmt::println("      Destination X = {}", m_cpu_vram_blit.source_x);
			fmt::println("      Destination Y = {}", m_cpu_vram_blit.source_y);
			fmt::println("      Size X        = {}", m_cpu_vram_blit.size_x);
			fmt::println("      Size Y        = {}", m_cpu_vram_blit.size_y);

			m_cmd_status = Status::CPU_VRAM_BLIT;

			m_renderer->BeginCpuVramBlit();
		}
			break;
		case psx::CommandType::VRAM_CPU_BLIT: {
			u32 cmd = m_cmd_fifo.deque();
			u32 source = m_cmd_fifo.deque();
			u32 size = m_cmd_fifo.deque();

			m_vram_cpu_blit.source_x = source & 0xFFFF;
			m_vram_cpu_blit.source_y = (source >> 16) & 0xFFFF;
			m_vram_cpu_blit.size_x = size & 0xFFFF;
			m_vram_cpu_blit.size_y = (size >> 16) & 0xFFFF;

			m_vram_cpu_blit.source_x &= VRAM_X_SIZE - 1;
			m_vram_cpu_blit.source_y &= VRAM_Y_SIZE - 1;

			if (m_vram_cpu_blit.size_x == 0)
				m_vram_cpu_blit.size_x = VRAM_X_SIZE;
			else {
				m_vram_cpu_blit.size_x =
					((m_vram_cpu_blit.size_x - 1) & (VRAM_X_SIZE - 1)) + 1;
			}

			if (m_vram_cpu_blit.size_y == 0)
				m_vram_cpu_blit.size_y = VRAM_Y_SIZE;
			else {
				m_vram_cpu_blit.size_y =
					((m_vram_cpu_blit.size_y - 1) & (VRAM_Y_SIZE - 1)) + 1;
			}

			m_vram_cpu_blit.curr_x = m_vram_cpu_blit.source_x;
			m_vram_cpu_blit.curr_y = m_vram_cpu_blit.source_y;

			fmt::println("[GPU] VRAM-CPU BLIT");
			fmt::println("      Source X      = {}", m_vram_cpu_blit.source_x);
			fmt::println("      Source Y      = {}", m_vram_cpu_blit.source_y);
			fmt::println("      Size X        = {}", m_vram_cpu_blit.size_x);
			fmt::println("      Size Y        = {}", m_vram_cpu_blit.size_y);

			m_cmd_status = Status::VRAM_CPU_BLIT;
			m_read_status = GPUREAD_Status::READ_VRAM;

			m_renderer->VramCpuBlit(m_vram_cpu_blit.source_x, m_vram_cpu_blit.source_y,
				m_vram_cpu_blit.size_x, m_vram_cpu_blit.size_y);
		}
			break;
		case psx::CommandType::ENV:
			error::DebugBreak();
			break;
		default:
			fmt::println("[GPU] Invalid command type 0x{:x}", (u32)cmd_type);
			break;
		}
	}
}