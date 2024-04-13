#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/renderer/GLRenderer.hpp>

#include <common/Errors.hpp>

#include <fmt/format.h>

namespace psx {
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

		v1.x = (vertex_1 & 0xFFFF);
		v1.y = ((vertex_1 >> 16) & 0xFFFF);

		v2.x = (vertex_2 & 0xFFFF);
		v2.y = ((vertex_2 >> 16) & 0xFFFF);

		v3.x = (vertex_3 & 0xFFFF);
		v3.y = ((vertex_3 >> 16) & 0xFFFF);

		v4.x = (vertex_4 & 0xFFFF);
		v4.y = ((vertex_4 >> 16) & 0xFFFF);

		video::UntexturedOpaqueFlatTriangle triangle1 = {};
		video::UntexturedOpaqueFlatTriangle triangle2 = {};

		triangle1.v0 = v1;
		triangle1.v1 = v2;
		triangle1.v2 = v3;

		triangle2.v0 = v2;
		triangle2.v1 = v3;
		triangle2.v2 = v4;

		u32 r = color & 0xFF;
		u32 g = (color >> 8) & 0xFF;
		u32 b = (color >> 16) & 0xFF;

		fmt::println("[GPU] DRAW QUAD");
		fmt::println("      R = {}, G = {}, B = {}",
			r, g, b);
		fmt::println("      V0 X = {}, Y = {}", 
			v1.x, v1.y);

		triangle1.r = r;
		triangle1.g = g;
		triangle1.b = b;

		triangle2.r = r;
		triangle2.g = g;
		triangle2.b = b;

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

		v1.x = (vertex1 & 0xFFFF);
		v1.y = ((vertex1 >> 16) & 0xFFFF);

		v2.x = (vertex2 & 0xFFFF);
		v2.y = ((vertex2 >> 16) & 0xFFFF);

		v3.x = (vertex3 & 0xFFFF);
		v3.y = ((vertex3 >> 16) & 0xFFFF);

		v4.x = (vertex4 & 0xFFFF);
		v4.y = ((vertex4 >> 16) & 0xFFFF);

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

		v1.x = (vertex1 & 0xFFFF);
		v1.y = ((vertex1 >> 16) & 0xFFFF);

		v2.x = (vertex2 & 0xFFFF);
		v2.y = ((vertex2 >> 16) & 0xFFFF);

		v3.x = (vertex3 & 0xFFFF);
		v3.y = ((vertex3 >> 16) & 0xFFFF);

		v1.color = color1;
		v2.color = color2;
		v3.color = color3;

		video::BasicGouraudTriangle triangle1 = {};

		triangle1.v0 = v1;
		triangle1.v1 = v2;
		triangle1.v2 = v3;

		m_renderer->DrawBasicGouraud(triangle1);
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
		else {
			m_cmd_fifo.deque();

			while (curr_params--) {
				u32 param = m_cmd_fifo.deque();
			}

			fmt::println("[GPU] DRAW QUAD");
			fmt::println("      Gouraud          = {}", gouraud);
			fmt::println("      Textured         = {}", tex);
			fmt::println("      Semi-transparent = {}", transparent);
			fmt::println("      Raw texture      = {}", raw);
			fmt::println("      First colour     = 0x{:x}", cmd & 0xFFFFFF);
		}
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
		else {
			m_cmd_fifo.deque();

			while (curr_params--) {
				u32 param = m_cmd_fifo.deque();
			}

			fmt::println("[GPU] DRAW TRIANGLE");
			fmt::println("      Gouraud          = {}", gouraud);
			fmt::println("      Textured         = {}", tex);
			fmt::println("      Semi-transparent = {}", transparent);
			fmt::println("      Raw texture      = {}", raw);
			fmt::println("      First colour     = 0x{:x}", cmd & 0xFFFFFF);
		}
	}
}