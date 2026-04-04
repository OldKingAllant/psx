#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/include/psxemu/GPUCommands.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <common/Errors.hpp>

#include <psxemu/renderer/GLRenderer.hpp>

#include <thirdparty/magic_enum/include/magic_enum/magic_enum.hpp>

namespace psx {

	void Gpu::EnvCommand(u32 cmd) {
		auto type = EnvCommandType(cmd >> 24);

		switch (type)
		{
		case EnvCommandType::TEXTURE_PAGE:
			Texpage(cmd);
			break;
		case EnvCommandType::TEXTURE_WINDOW:
			TexWindow(cmd);
			break;
		case EnvCommandType::SET_DRAW_TOP:
			DrawAreaTopLeft(cmd);
			break;
		case EnvCommandType::SET_DRAW_BOTTOM:
			DrawAreaBottomRight(cmd);
			break;
		case EnvCommandType::SET_DRAW_OFFSET:
			DrawOffset(cmd);
			break;
		case EnvCommandType::MASK_BIT:
			MaskSetting(cmd);
			break;
		default:
			LOG_ERROR("GPU", "[GPU] Unimplemented ENV command 0x{:x}", (u32)type);
			error::DebugBreak();
			break;
		}
	}

	void Gpu::Texpage(u32 cmd) {
		cmd &= (1 << 14) - 1;

		LOG_DEBUG("GPU", "[GPU] TEXPAGE(0x{:x})", cmd);

		if (cmd == m_raw_conf.texpage)
			return;

		FlushDrawOps();
		m_renderer->SyncTextures();

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

		LOG_DEBUG("GPU", "          Page X base       0x{:x}", (u32)m_stat.texture_page_x_base);
		LOG_DEBUG("GPU", "          Page Y base 1     0x{:x}", (u32)m_stat.texture_page_y_base);
		LOG_DEBUG("GPU", "          Semi transparency 0x{:x}", (u32)m_stat.semi_transparency);
		LOG_DEBUG("GPU", "          Tex page colors   0x{:x}", (u32)m_stat.tex_page_colors);
		LOG_DEBUG("GPU", "          Dither            {}", m_stat.dither);
		LOG_DEBUG("GPU", "          Draw to display   {}", m_stat.draw_to_display);
		LOG_DEBUG("GPU", "          Page Y base 2     0x{:x}", (u32)m_stat.texture_page_y_base2);
		LOG_DEBUG("GPU", "          Texture X flip    {}", m_tex_x_flip);
		LOG_DEBUG("GPU", "          Texture Y flip    {}", m_tex_y_flip);

		m_renderer->GetUniformBuffer()
			.use_dither = m_stat.dither;
		m_renderer->RequestUniformBufferUpdate();

		if (m_recording_commands) {
			GPUCommand cmd_copy{};
			cmd_copy.value = cmd;
			cmd_copy.reg = CommandRegister::GP0;
			cmd_copy.frame_of_recording = m_curr_vblank_count;
			cmd_copy.gp0.type = GP0CommandType::ENV;
			cmd_copy.gp0.env.type = EnvCommandType::TEXTURE_PAGE;
			cmd_copy.gp0.env.cmd = cmd;
			cmd_copy.start_index = m_latest_idle_index;
			m_recorded_cmds.emplace_back(cmd_copy);
		}
	}

	void Gpu::MiscCommand(u32 cmd) {
		if (m_read_status == GPUREAD_Status::READ_VRAM) {
			LOG_ERROR("GPU", "[GPU] ********GPU-COMMAND DURING VRAM READ!*******");
		}

		auto type = MiscCommandType(cmd >> 24);

		switch (type)
		{
		case MiscCommandType::NOP:
			LOG_DEBUG("GPU", "[GPU] NOP");
			if (m_recording_commands) {
				GPUCommand cmd_copy{};
				cmd_copy.value = cmd;
				cmd_copy.reg = CommandRegister::GP0;
				cmd_copy.frame_of_recording = m_curr_vblank_count;
				cmd_copy.gp0.type = GP0CommandType::MISC;
				cmd_copy.gp0.misc.type = MiscCommandType::NOP;
				cmd_copy.gp0.misc.cmd = cmd;
				cmd_copy.start_index = m_latest_idle_index;
				m_recorded_cmds.emplace_back(cmd_copy);
			}
			break;
		case MiscCommandType::CLEAR_CACHE:
			LOG_DEBUG("GPU", "[GPU] CLEAR TEXTURE CACHE");
			FlushDrawOps();
			m_renderer->SyncTextures();
			if (m_recording_commands) {
				GPUCommand cmd_copy{};
				cmd_copy.value = cmd;
				cmd_copy.reg = CommandRegister::GP0;
				cmd_copy.frame_of_recording = m_curr_vblank_count;
				cmd_copy.gp0.type = GP0CommandType::MISC;
				cmd_copy.gp0.misc.type = MiscCommandType::CLEAR_CACHE;
				cmd_copy.gp0.misc.cmd = cmd;
				cmd_copy.start_index = m_latest_idle_index;
				m_recorded_cmds.emplace_back(cmd_copy);
			}
			break;
		case MiscCommandType::QUICK_FILL:
			m_cmd_fifo.queue(cmd);
			m_required_params = 2;
			m_rem_params = m_required_params;
			m_cmd_status = Status::WAITING_PARAMETERS;
			break;
		case MiscCommandType::NOP_FIFO:
			LOG_DEBUG("GPU", "[GPU] NOP FIFO");
			if (m_recording_commands) {
				GPUCommand cmd_copy{};
				cmd_copy.value = cmd;
				cmd_copy.reg = CommandRegister::GP0;
				cmd_copy.frame_of_recording = m_curr_vblank_count;
				cmd_copy.gp0.type = GP0CommandType::MISC;
				cmd_copy.gp0.misc.type = MiscCommandType::NOP_FIFO;
				cmd_copy.gp0.misc.cmd = cmd;
				cmd_copy.start_index = m_latest_idle_index;
				m_recorded_cmds.emplace_back(cmd_copy);
			}
			break;
		default:
			LOG_ERROR("GPU", "[GPU] Unimplemented MISC command 0x{:x}", (u32)type);
			error::DebugBreak();
			break;
		}
	}

	void Gpu::CommandStart(u32 cmd) {
		GP0CommandType cmd_type = (GP0CommandType)((cmd >> 29) & 0x7);

		m_stat.recv_cmd_word = false;
		m_stat.recv_dma = false;
	
		switch (cmd_type)
		{
		case psx::GP0CommandType::MISC:
			MiscCommand(cmd);
			break;
		case psx::GP0CommandType::POLYGON: {
			auto polygon_cmd = PolygonCmd{ cmd };
			bool quad = polygon_cmd.is_quad();
			bool tex = polygon_cmd.is_textured();
			bool gouraud = polygon_cmd.is_gouraud();

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
		case psx::GP0CommandType::LINE: {
			auto line_cmd = LineCmd{ cmd };
			bool gouraud = line_cmd.is_gouraud();
			bool polyline = line_cmd.is_polyline();

			u32 words_per_vertex = gouraud ? 2 : 1;

			m_cmd_fifo.queue(cmd);

			if (polyline) {
				m_cmd_status = gouraud ?
					Status::POLYLINE_GOURAUD :
					Status::POLYLINE;
			}
			else {
				m_cmd_status = Status::WAITING_PARAMETERS;
				m_rem_params = words_per_vertex * 2;

				if (gouraud)
					m_rem_params -= 1;

				m_required_params = m_rem_params;
			}
		}
			break;
		case psx::GP0CommandType::RECTANGLE: {
			m_cmd_fifo.queue(cmd);
			u8 rect_size = (cmd >> 27) & 3;
			u32 tot_params = 1; //Add vertex1 x+y
			if (rect_size == 0)
				tot_params += 1; //Add w+h
			bool textured = (cmd >> 26) & 1;
			if (textured)
				tot_params += 1; //Add Clut+U+V
			m_required_params = tot_params;
			m_rem_params = tot_params;
			m_cmd_status = Status::WAITING_PARAMETERS;
		}
			break;
		case psx::GP0CommandType::VRAM_BLIT: {
			m_cmd_fifo.queue(cmd);
			m_cmd_status = Status::WAITING_PARAMETERS;
			m_required_params = 3;
			m_rem_params = 3;
		}
			break;
		case psx::GP0CommandType::CPU_VRAM_BLIT: {
			m_cmd_fifo.queue(cmd);
			m_cmd_status = Status::WAITING_PARAMETERS;
			m_required_params = 2;
			m_rem_params = 2;
		}
			break;
		case psx::GP0CommandType::VRAM_CPU_BLIT: {
			m_cmd_fifo.queue(cmd);
			m_cmd_status = Status::WAITING_PARAMETERS;
			m_required_params = 2;
			m_rem_params = 2;
		}
			break;
		case psx::GP0CommandType::ENV:
			EnvCommand(cmd);
			break;
		default:
			LOG_ERROR("GPU", "[GPU] Invalid command type 0x{:x}", (u32)cmd_type);
			break;
		}
	}

	void Gpu::DrawAreaTopLeft(u32 cmd) {
		cmd &= 0xFFFFF;

		LOG_DEBUG("GPU", "[GPU] DRAW_TOP_LEFT(0x{:x})", cmd);

		if (m_raw_conf.draw_top_left == cmd)
			return;

		FlushDrawOps();

		m_raw_conf.draw_top_left = cmd;

		m_x_top_left = cmd & 1023;
		m_y_top_left = (cmd >> 10) & 511;

		LOG_DEBUG("GPU", "      X = {}, Y = {}", m_x_top_left,
			m_y_top_left);

		m_renderer->SetScissorTop(m_x_top_left, m_y_top_left);

		if (m_recording_commands) {
			GPUCommand cmd_copy{};
			cmd_copy.value = cmd;
			cmd_copy.reg = CommandRegister::GP0;
			cmd_copy.frame_of_recording = m_curr_vblank_count;
			cmd_copy.gp0.type = GP0CommandType::ENV;
			cmd_copy.gp0.env.type = EnvCommandType::SET_DRAW_TOP;
			cmd_copy.gp0.env.cmd = cmd;
			cmd_copy.start_index = m_latest_idle_index;
			m_recorded_cmds.emplace_back(cmd_copy);
		}
	}

	void Gpu::DrawAreaBottomRight(u32 cmd) {
		cmd &= 0xFFFFF;

		LOG_DEBUG("GPU", "[GPU] DRAW_BOTTOM_RIGHT(0x{:x})", cmd);

		if (m_raw_conf.draw_bottom_right == cmd)
			return;

		FlushDrawOps();

		m_raw_conf.draw_bottom_right = cmd;

		m_x_bot_right = cmd & 1023;
		m_y_bot_right = (cmd >> 10) & 511;

		LOG_DEBUG("GPU", "      X = {}, Y = {}", m_x_bot_right,
			m_y_bot_right);

		m_renderer->SetScissorBottom(m_x_bot_right, m_y_bot_right);

		if (m_recording_commands) {
			GPUCommand cmd_copy{};
			cmd_copy.value = cmd;
			cmd_copy.reg = CommandRegister::GP0;
			cmd_copy.frame_of_recording = m_curr_vblank_count;
			cmd_copy.gp0.type = GP0CommandType::ENV;
			cmd_copy.gp0.env.type = EnvCommandType::SET_DRAW_BOTTOM;
			cmd_copy.gp0.env.cmd = cmd;
			cmd_copy.start_index = m_latest_idle_index;
			m_recorded_cmds.emplace_back(cmd_copy);
		}
	}

	void Gpu::DrawOffset(u32 cmd) {
		cmd &= 0x1FFFFF;

		LOG_DEBUG("GPU", "[GPU] DRAW_OFFSET(0x{:x})", cmd);

		if (cmd == m_raw_conf.draw_offset)
			return;

		m_raw_conf.draw_offset = cmd;

		FlushDrawOps();

		m_x_off = cmd & 0x7FF;
		m_y_off = (cmd >> 11) & 0x7FF;

		m_x_off = (u32)sign_extend<i32, 10>(m_x_off);
		m_y_off = (u32)sign_extend<i32, 10>(m_y_off);

		LOG_DEBUG("GPU", "      X = {}, Y = {}", (i32)m_x_off,
			(i32)m_y_off);

		m_renderer->GetUniformBuffer()
			.draw_x_off = m_x_off;
		m_renderer->GetUniformBuffer()
			.draw_y_off = m_y_off;
		m_renderer->RequestUniformBufferUpdate();

		if (m_recording_commands) {
			GPUCommand cmd_copy{};
			cmd_copy.value = cmd;
			cmd_copy.reg = CommandRegister::GP0;
			cmd_copy.frame_of_recording = m_curr_vblank_count;
			cmd_copy.gp0.type = GP0CommandType::ENV;
			cmd_copy.gp0.env.type = EnvCommandType::SET_DRAW_OFFSET;
			cmd_copy.gp0.env.cmd = cmd;
			cmd_copy.start_index = m_latest_idle_index;
			m_recorded_cmds.emplace_back(cmd_copy);
		}
	}

	void Gpu::TexWindow(u32 cmd) {
		cmd &= 0xFFFFF;

		LOG_DEBUG("GPU", "[GPU] TEX_WINDOW(0x{:x})", cmd);

		if (cmd == m_raw_conf.tex_window)
			return;

		m_raw_conf.tex_window = cmd;

		FlushDrawOps();

		m_tex_win.mask_x = cmd & 0x1F;
		m_tex_win.mask_y = (cmd >> 5) & 0x1F;
		m_tex_win.offset_x = (cmd >> 10) & 0x1F;
		m_tex_win.offset_y = (cmd >> 15) & 0x1F;

		LOG_DEBUG("GPU", "     Mask X   = {}", m_tex_win.mask_x);
		LOG_DEBUG("GPU", "     Mask Y   = {}", m_tex_win.mask_y);
		LOG_DEBUG("GPU", "     Offset X = {}", m_tex_win.offset_x);
		LOG_DEBUG("GPU", "     Offset Y = {}", m_tex_win.offset_y);

		video::GlobalUniforms& uniforms = m_renderer->GetUniformBuffer();
		uniforms.tex_window_mask_x = m_tex_win.mask_x;
		uniforms.tex_window_mask_y = m_tex_win.mask_y;
		uniforms.tex_window_off_x = m_tex_win.offset_x;
		uniforms.tex_window_off_y = m_tex_win.offset_y;
		m_renderer->RequestUniformBufferUpdate();

		if (m_recording_commands) {
			GPUCommand cmd_copy{};
			cmd_copy.value = cmd;
			cmd_copy.reg = CommandRegister::GP0;
			cmd_copy.frame_of_recording = m_curr_vblank_count;
			cmd_copy.gp0.type = GP0CommandType::ENV;
			cmd_copy.gp0.env.type = EnvCommandType::TEXTURE_WINDOW;
			cmd_copy.gp0.env.cmd = cmd;
			cmd_copy.start_index = m_latest_idle_index;
			m_recorded_cmds.emplace_back(cmd_copy);
		}
	}

	void Gpu::MaskSetting(u32 cmd) {
		cmd &= 3;

		LOG_DEBUG("GPU", "[GPU] MASK_BIT(0x{:x})", cmd);

		bool old_set = m_stat.set_mask;
		bool old_siable = m_stat.draw_over_mask_disable;

		m_stat.set_mask = (bool)(cmd & 1);
		m_stat.draw_over_mask_disable = (bool)((cmd >> 1) & 1);

		FlushDrawOps();

		LOG_DEBUG("GPU", "      Force bit 15 to 1      = {}", m_stat.set_mask);
		LOG_DEBUG("GPU", "      Check mask before draw = {}", m_stat.draw_over_mask_disable);

		video::GlobalUniforms& uniforms = m_renderer->GetUniformBuffer();
		uniforms.set_mask = m_stat.set_mask;
		uniforms.check_mask = m_stat.draw_over_mask_disable;
		m_renderer->RequestUniformBufferUpdate();
		m_renderer->SetDrawOverMaskDisable(m_stat.draw_over_mask_disable);

		if (m_stat.set_mask || m_stat.draw_over_mask_disable)
			LOG_DEBUG("GPU", "[GPU] Mask enabled in one way or another");

		if (m_recording_commands) {
			GPUCommand cmd_copy{};
			cmd_copy.value = cmd;
			cmd_copy.reg = CommandRegister::GP0;
			cmd_copy.frame_of_recording = m_curr_vblank_count;
			cmd_copy.gp0.type = GP0CommandType::ENV;
			cmd_copy.gp0.env.type = EnvCommandType::MASK_BIT;
			cmd_copy.gp0.env.cmd = cmd;
			cmd_copy.start_index = m_latest_idle_index;
			m_recorded_cmds.emplace_back(cmd_copy);
		}
	}

	void Gpu::CommandEnd() {
		if (m_cmd_fifo.empty())
			error::DebugBreak();

		u32 cmd = m_cmd_fifo.peek();

		GP0CommandType cmd_type = (GP0CommandType)((cmd >> 29) & 0x7);

		switch (cmd_type)
		{
		case psx::GP0CommandType::MISC: {
			auto type = MiscCommandType(cmd >> 24);
			if (type != MiscCommandType::QUICK_FILL) {
				LOG_FLUSH();
				error::DebugBreak();
			}
			else {
				QuickFill();
				m_cmd_status = Status::IDLE;
			}
		}
			break;
		case psx::GP0CommandType::POLYGON: {
			bool quad = PolygonCmd{cmd}.is_quad();

			if (quad) {
				DrawQuad();
			}
			else {
				DrawTriangle();
			}

			m_cmd_status = Status::IDLE;
		}
			break;
		case psx::GP0CommandType::LINE: {
			DrawLine();
			m_cmd_status = Status::IDLE;
		}
			break;
		case psx::GP0CommandType::RECTANGLE: {
			DrawRect();
			m_cmd_status = Status::IDLE;
		}
			break;
		case psx::GP0CommandType::VRAM_BLIT: {
			u32 cmd = m_cmd_fifo.deque();
			u32 src_coords = m_cmd_fifo.deque();
			u32 dst_coords = m_cmd_fifo.deque();
			u32 wh = m_cmd_fifo.deque();

			u32 src_x = src_coords & 0xFFFF;
			u32 src_y = (src_coords >> 16) & 0xFFFF;
			u32 dst_x = dst_coords & 0xFFFF;
			u32 dst_y = (dst_coords >> 16) & 0xFFFF;
			u32 w = wh & 0xFFFF;
			u32 h = (wh >> 16) & 0xFFFF;

			src_x &= VRAM_X_SIZE - 1;
			src_y &= VRAM_Y_SIZE - 1;

			dst_x &= VRAM_X_SIZE - 1;
			dst_y &= VRAM_Y_SIZE - 1;

			if (w == 0) {
				w = VRAM_X_SIZE;
			}
			else {
				w = ((w - 1) & (VRAM_X_SIZE - 1)) + 1;
			}

			if (h == 0) {
				h = VRAM_Y_SIZE;
			}
			else {
				h = ((h - 1) & (VRAM_Y_SIZE - 1)) + 1;
			}

			if (src_x + w > VRAM_X_SIZE || dst_x + w > VRAM_X_SIZE) {
				LOG_WARN("GPU", "[GPU] VRAM-VRAM BLIT OUT OF BOUNDS X");
			}

			if (src_y + h > VRAM_Y_SIZE || dst_y + h > VRAM_Y_SIZE) {
				LOG_WARN("GPU", "[GPU] VRAM-VRAM BLIT OUT OF BOUNDS Y");
			}

			LOG_DEBUG("GPU", "[GPU] VRAM-VRAM BLIT");
			LOG_DEBUG("GPU", "      SRC X = {}, Y = {}", src_x, src_y);
			LOG_DEBUG("GPU", "      DST X = {}, Y = {}", dst_x, dst_y);
			LOG_DEBUG("GPU", "      W = {}, H = {}", w, h);
			m_cmd_status = Status::IDLE;

			m_renderer->VramVramBlit(src_x, src_y, dst_x, dst_y,
				w, h, m_stat.draw_over_mask_disable);

			if (m_recording_commands) {
				GPUCommand cmd_copy{};
				cmd_copy.value = cmd;
				cmd_copy.reg = CommandRegister::GP0;
				cmd_copy.frame_of_recording = m_curr_vblank_count;
				cmd_copy.gp0.type = GP0CommandType::VRAM_BLIT;
				cmd_copy.params.vram_vram_blit.src_x = src_x;
				cmd_copy.params.vram_vram_blit.src_y = src_y;
				cmd_copy.params.vram_vram_blit.dst_x = dst_x;
				cmd_copy.params.vram_vram_blit.dst_y = dst_y;
				cmd_copy.params.vram_vram_blit.w = w;
				cmd_copy.params.vram_vram_blit.h = h;
				cmd_copy.start_index = m_latest_idle_index;
				m_recorded_cmds.emplace_back(cmd_copy);
			}
		}
			break;
		case psx::GP0CommandType::CPU_VRAM_BLIT: {
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

			LOG_DEBUG("GPU", "[GPU] CPU-VRAM BLIT");
			LOG_DEBUG("GPU", "      Destination X = {}", m_cpu_vram_blit.source_x);
			LOG_DEBUG("GPU", "      Destination Y = {}", m_cpu_vram_blit.source_y);
			LOG_DEBUG("GPU", "      Size X        = {}", m_cpu_vram_blit.size_x);
			LOG_DEBUG("GPU", "      Size Y        = {}", m_cpu_vram_blit.size_y);

			m_cmd_status = Status::CPU_VRAM_BLIT;

			m_renderer->BeginCpuVramBlit();

			if (m_recording_commands) {
				GPUCommand cmd_copy{};
				cmd_copy.value = cmd;
				cmd_copy.reg = CommandRegister::GP0;
				cmd_copy.frame_of_recording = m_curr_vblank_count;
				cmd_copy.gp0.type = GP0CommandType::CPU_VRAM_BLIT;
				cmd_copy.params.cpu_vram_blit.dst_x = m_cpu_vram_blit.curr_x;
				cmd_copy.params.cpu_vram_blit.dst_y = m_cpu_vram_blit.curr_y;
				cmd_copy.params.cpu_vram_blit.w = m_cpu_vram_blit.size_x;
				cmd_copy.params.cpu_vram_blit.h = m_cpu_vram_blit.size_y;
				cmd_copy.start_index = m_latest_idle_index;
				m_recorded_cmds.emplace_back(cmd_copy);
			}
		}
			break;
		case psx::GP0CommandType::VRAM_CPU_BLIT: {
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

			LOG_DEBUG("GPU", "[GPU] VRAM-CPU BLIT");
			LOG_DEBUG("GPU", "      Source X      = {}", m_vram_cpu_blit.source_x);
			LOG_DEBUG("GPU", "      Source Y      = {}", m_vram_cpu_blit.source_y);
			LOG_DEBUG("GPU", "      Size X        = {}", m_vram_cpu_blit.size_x);
			LOG_DEBUG("GPU", "      Size Y        = {}", m_vram_cpu_blit.size_y);

			m_cmd_status = Status::VRAM_CPU_BLIT;
			m_read_status = GPUREAD_Status::READ_VRAM;

			m_renderer->VramCpuBlit(m_vram_cpu_blit.source_x, m_vram_cpu_blit.source_y,
				m_vram_cpu_blit.size_x, m_vram_cpu_blit.size_y);

			if (m_recording_commands) {
				GPUCommand cmd_copy{};
				cmd_copy.value = cmd;
				cmd_copy.reg = CommandRegister::GP0;
				cmd_copy.frame_of_recording = m_curr_vblank_count;
				cmd_copy.gp0.type = GP0CommandType::VRAM_CPU_BLIT;
				cmd_copy.params.vram_cpu_blit.src_x = m_vram_cpu_blit.curr_x;
				cmd_copy.params.vram_cpu_blit.src_y = m_vram_cpu_blit.curr_y;
				cmd_copy.params.vram_cpu_blit.w = m_vram_cpu_blit.size_x;
				cmd_copy.params.vram_cpu_blit.h = m_vram_cpu_blit.size_y;
				cmd_copy.start_index = m_latest_idle_index;
				m_recorded_cmds.emplace_back(cmd_copy);
			}
		}
			break;
		case psx::GP0CommandType::ENV:
			error::DebugBreak();
			break;
		default:
			LOG_ERROR("GPU", "[GPU] Invalid command type 0x{:x}", (u32)cmd_type);
			break;
		}
	}
}