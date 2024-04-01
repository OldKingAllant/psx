#pragma once

#include <common/Defs.hpp>

namespace psx {
	enum class CommandType {
		MISC,
		POLYGON,
		LINE, 
		RECTANGLE,
		VRAM_BLIT,
		CPU_VRAM_BLIT,
		VRAM_CPU_BLIT,
		ENV
	};
}