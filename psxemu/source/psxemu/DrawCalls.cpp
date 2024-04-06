#include <psxemu/include/psxemu/GPU.hpp>

#include <common/Errors.hpp>

#include <fmt/format.h>

namespace psx {
	void Gpu::DrawQuad() {
		u32 cmd = m_cmd_fifo.deque();

		bool tex = (cmd >> 26) & 1;
		bool gouraud = (cmd >> 28) & 1;
		bool transparent = (cmd >> 25) & 1;
		bool raw = (cmd >> 24) & 1;

		u32 params_vert = 1;

		u32 curr_params = m_required_params;

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

	void Gpu::DrawTriangle() {
		error::DebugBreak();
	}
}