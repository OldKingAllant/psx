#pragma once

#include <common/Defs.hpp>

namespace psx {
	enum class Interrupts {
		VBLANK = 1,
		GPU = (1 << 1),
		CDROM = (1 << 2),
		DMA = (1 << 3),
		TIMER0 = (1 << 4),
		TIMER1 = (1 << 5),
		TIMER2 = (1 << 6),
		PAD_CARD = (1 << 7),
		SIO = (1 << 8),
		SPU = (1 << 9),
		PAD = (1 << 10)
	};
}