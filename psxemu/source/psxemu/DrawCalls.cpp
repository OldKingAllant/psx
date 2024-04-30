#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/renderer/GLRenderer.hpp>

#include <common/Errors.hpp>

#include <fmt/format.h>

namespace psx {
	void Gpu::CheckIfDrawNeeded() {
		//If this is true, batching becomes
		//impossible, since previous draw
		//calls can immediately affect 
		//others, for this reason, we
		//need to always update the VRAM
		//input texture after every draw
		//command
		if (m_stat.draw_over_mask_disable) {
			FlushDrawOps();
			m_renderer->SyncTextures();
		}
	}

	void Gpu::FlushDrawOps() {
		m_renderer->FlushCommands();
	}

	void Gpu::DrawFlatUntexturedOpaqueQuad() {
		u32 color = m_cmd_fifo.deque() & 0xFFFFFF;

		u32 vertex_1 = m_cmd_fifo.deque();
		u32 vertex_2 = m_cmd_fifo.deque();
		u32 vertex_3 = m_cmd_fifo.deque();
		u32 vertex_4 = m_cmd_fifo.deque();

		video::UntexturedOpaqueFlatVertex v1 = {};
		video::UntexturedOpaqueFlatVertex v2 = {};
		video::UntexturedOpaqueFlatVertex v3 = {};
		video::UntexturedOpaqueFlatVertex v4 = {};

		v1.x = sign_extend<i32, 10>(vertex_1 & 0xFFFF);
		v1.y = sign_extend<i32, 10>((vertex_1 >> 16) & 0xFFFF);

		v2.x = sign_extend<i32, 10>(vertex_2 & 0xFFFF);
		v2.y = sign_extend<i32, 10>((vertex_2 >> 16) & 0xFFFF);

		v3.x = sign_extend<i32, 10>(vertex_3 & 0xFFFF);
		v3.y = sign_extend<i32, 10>((vertex_3 >> 16) & 0xFFFF);

		v4.x = sign_extend<i32, 10>(vertex_4 & 0xFFFF);
		v4.y = sign_extend<i32, 10>((vertex_4 >> 16) & 0xFFFF);

		u32 r = color & 0xFF;
		u32 g = (color >> 8) & 0xFF;
		u32 b = (color >> 16) & 0xFF;

		v1.r = r;
		v1.g = g;
		v1.b = b;

		v2.r = r;
		v2.g = g;
		v2.b = b;

		v3.r = r;
		v3.g = g;
		v3.b = b;

		v4.r = r;
		v4.g = g;
		v4.b = b;

		video::UntexturedOpaqueFlatTriangle triangle1 = {};
		video::UntexturedOpaqueFlatTriangle triangle2 = {};

		triangle1.v0 = v1;
		triangle1.v1 = v2;
		triangle1.v2 = v3;

		triangle2.v0 = v2;
		triangle2.v1 = v3;
		triangle2.v2 = v4;

		/*fmt::println("[GPU] DRAW QUAD");
		fmt::println("      R = {}, G = {}, B = {}",
			r, g, b);
		fmt::println("      V0 X = {}, Y = {}", 
			v1.x, v1.y);*/

		m_renderer->DrawFlatUntexturedOpaque(
			triangle1
		);

		m_renderer->DrawFlatUntexturedOpaque(
			triangle2
		);
	}

	void Gpu::DrawBasicGouraudQuad() {
		u32 color1 = m_cmd_fifo.deque() & 0xFFFFFF;

		u32 vertex1 = m_cmd_fifo.deque();
		u32 color2 = m_cmd_fifo.deque();
		u32 vertex2 = m_cmd_fifo.deque();
		u32 color3 = m_cmd_fifo.deque();
		u32 vertex3 = m_cmd_fifo.deque();
		u32 color4 = m_cmd_fifo.deque();
		u32 vertex4 = m_cmd_fifo.deque();

		video::BasicGouraudVertex v1 = {};
		video::BasicGouraudVertex v2 = {};
		video::BasicGouraudVertex v3 = {};
		video::BasicGouraudVertex v4 = {};

		v1.x = sign_extend<i32, 10>(vertex1 & 0xFFFF);
		v1.y = sign_extend<i32, 10>((vertex1 >> 16) & 0xFFFF);

		v2.x = sign_extend<i32, 10>(vertex2 & 0xFFFF);
		v2.y = sign_extend<i32, 10>((vertex2 >> 16) & 0xFFFF);

		v3.x = sign_extend<i32, 10>(vertex3 & 0xFFFF);
		v3.y = sign_extend<i32, 10>((vertex3 >> 16) & 0xFFFF);

		v4.x = sign_extend<i32, 10>(vertex4 & 0xFFFF);
		v4.y = sign_extend<i32, 10>((vertex4 >> 16) & 0xFFFF);

		v1.color = color1;
		v2.color = color2;
		v3.color = color3;
		v4.color = color4;

		video::BasicGouraudTriangle triangle1 = {};
		video::BasicGouraudTriangle triangle2 = {};

		triangle1.v0 = v1;
		triangle1.v1 = v2;
		triangle1.v2 = v3;

		triangle2.v0 = v2;
		triangle2.v1 = v3;
		triangle2.v2 = v4;

		m_renderer->DrawBasicGouraud(triangle1);
		m_renderer->DrawBasicGouraud(triangle2);
	}

	void Gpu::DrawBasicGouraudTriangle() {
		u32 color1 = m_cmd_fifo.deque() & 0xFFFFFF;

		u32 vertex1 = m_cmd_fifo.deque();
		u32 color2 = m_cmd_fifo.deque();
		u32 vertex2 = m_cmd_fifo.deque();
		u32 color3 = m_cmd_fifo.deque();
		u32 vertex3 = m_cmd_fifo.deque();
		
		video::BasicGouraudVertex v1 = {};
		video::BasicGouraudVertex v2 = {};
		video::BasicGouraudVertex v3 = {};

		v1.x = sign_extend<i32, 10>(vertex1 & 0xFFFF);
		v1.y = sign_extend<i32, 10>((vertex1 >> 16) & 0xFFFF);

		v2.x = sign_extend<i32, 10>(vertex2 & 0xFFFF);
		v2.y = sign_extend<i32, 10>((vertex2 >> 16) & 0xFFFF);

		v3.x = sign_extend<i32, 10>(vertex3 & 0xFFFF);
		v3.y = sign_extend<i32, 10>((vertex3 >> 16) & 0xFFFF);

		v1.color = color1;
		v2.color = color2;
		v3.color = color3;

		video::BasicGouraudTriangle triangle1 = {};

		triangle1.v0 = v1;
		triangle1.v1 = v2;
		triangle1.v2 = v3;

		m_renderer->DrawBasicGouraud(triangle1);
	}

	void Gpu::DrawTexturedQuad() {
		u32 cmd = m_cmd_fifo.deque();

		u32 gouraud = (cmd >> 28) & 1;
		u32 semi_trans = (cmd >> 25) & 1;
		u32 raw = (cmd >> 24) & 1;
		u32 first_color = (cmd & 0xFFFFFF);

		uint32_t flags{ 0 };

		if (gouraud)
			flags |= video::TexturedVertexFlags::GOURAUD;

		if (semi_trans)
			flags |= video::TexturedVertexFlags::SEMI_TRANSPARENT;

		if (raw)
			flags |= video::TexturedVertexFlags::RAW_TEXTURE;

		video::TexturedVertex vertices[4] = {};

		u32 clut_and_page = 0;

		for (u32 i = 0; i < 4; i++) {
			if (gouraud && i != 0) {
				vertices[i].color = m_cmd_fifo.deque() & 0xFFFFFF;
			}
			else {
				vertices[i].color = first_color;
			}

			u32 vertex_pos = m_cmd_fifo.deque();

			vertices[i].x = sign_extend<i32, 10>(vertex_pos & 0xFFFF);
			vertices[i].y = sign_extend<i32, 10>((vertex_pos >> 16) & 0xFFFF);

			u32 uv = m_cmd_fifo.deque();

			if (i == 0) {
				u32 clut = (uv >> 16) & 0xFFFF;
				clut_and_page |= (clut << 16);
			}
			else if(i == 1) {
				u32 page = (uv >> 16) & 0xFFFF;
				clut_and_page |= page;
			}

			vertices[i].uv = uv & 0xFFFF;

			vertices[i].flags = flags;
		}

		vertices[0].clut_page = clut_and_page;
		vertices[1].clut_page = clut_and_page;
		vertices[2].clut_page = clut_and_page;
		vertices[3].clut_page = clut_and_page;

		video::TexturedTriangle triangle1 = {};
		video::TexturedTriangle triangle2 = {};

		triangle1.v0 = vertices[0];
		triangle1.v1 = vertices[1];
		triangle1.v2 = vertices[2];

		triangle2.v0 = vertices[1];
		triangle2.v1 = vertices[2];
		triangle2.v2 = vertices[3];

		u16 page = (u16)clut_and_page;

		TryUpdateTexpage(page);

		m_renderer->DrawTexturedTriangle(triangle1);
		m_renderer->DrawTexturedTriangle(triangle2);
	}

	void Gpu::DrawQuad() {
		u32 cmd = m_cmd_fifo.peek();

		bool tex = (cmd >> 26) & 1;
		bool gouraud = (cmd >> 28) & 1;
		bool transparent = (cmd >> 25) & 1;
		bool raw = (cmd >> 24) & 1;

		u32 params_vert = 1;

		u32 curr_params = m_required_params;

		if (!tex && !transparent && !gouraud) {
			DrawFlatUntexturedOpaqueQuad();
		}
		else if (gouraud && !tex && !transparent) {
			DrawBasicGouraudQuad();
		}
		else if (tex) {
			DrawTexturedQuad();
		}
		else {
			m_cmd_fifo.deque();

			while (curr_params--) {
				u32 param = m_cmd_fifo.deque();
			}

			/*fmt::println("[GPU] DRAW QUAD");
			fmt::println("      Gouraud          = {}", gouraud);
			fmt::println("      Textured         = {}", tex);
			fmt::println("      Semi-transparent = {}", transparent);
			fmt::println("      Raw texture      = {}", raw);
			fmt::println("      First colour     = 0x{:x}", cmd & 0xFFFFFF);*/
		}

		CheckIfDrawNeeded();
	}

	void Gpu::DrawTriangle() {
		u32 cmd = m_cmd_fifo.peek();

		bool tex = (cmd >> 26) & 1;
		bool gouraud = (cmd >> 28) & 1;
		bool transparent = (cmd >> 25) & 1;
		bool raw = (cmd >> 24) & 1;

		u32 params_vert = 1;

		u32 curr_params = m_required_params;

		if (gouraud && !transparent && !tex) {
			DrawBasicGouraudTriangle();
		}
		else if (tex) {
			error::DebugBreak();
		}
		else {
			m_cmd_fifo.deque();

			while (curr_params--) {
				u32 param = m_cmd_fifo.deque();
			}

			/*fmt::println("[GPU] DRAW TRIANGLE");
			fmt::println("      Gouraud          = {}", gouraud);
			fmt::println("      Textured         = {}", tex);
			fmt::println("      Semi-transparent = {}", transparent);
			fmt::println("      Raw texture      = {}", raw);
			fmt::println("      First colour     = 0x{:x}", cmd & 0xFFFFFF);*/
		}

		CheckIfDrawNeeded();
	}

	void Gpu::QuickFill() {
		FlushDrawOps();
		m_renderer->SyncTextures();

		u32 color = m_cmd_fifo.deque() & 0xFFFFFF;
		u32 top_left = m_cmd_fifo.deque();

		i32 x_off = u16(top_left & 0x3F0); //Aligned to 16 pixels
		i32 y_off = u16(top_left >> 16) & 0x1FF;

		u32 size = m_cmd_fifo.deque();

		u32 w = (u32(size & 0x3FF) + 0xF) & ~0xF;
		u32 h = u32(size >> 16);

		if (x_off < 0 || y_off < 0)
			fmt::println("[GPU] QUICK-FILL offsets less than zero");

		if (x_off + w >= VRAM_X_SIZE || y_off + h >= VRAM_Y_SIZE)
			fmt::println("[GPU] QUICK-FILL size goes out of bounds");

		m_renderer->Fill(x_off, y_off, w, h, color);
	}
}