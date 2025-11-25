#pragma once

#include <common/Defs.hpp>

namespace psx {
	namespace SPU_REGS {
		constexpr static u32 MAINVOL_LEFT = 0xD80;
		constexpr static u32 MAINVOL_RIGHT = 0xD82;
		constexpr static u32 REVERB_BASE = 0xDC0;
		constexpr static u32 REVERB_END = 0xDFF;
		constexpr static u32 REVERB_LVOL = 0xD84;
		constexpr static u32 REVERB_RVOL = 0xD86;
		constexpr static u32 REVERB_MBASE = 0xDA2;
		constexpr static u32 SPU_CNT = 0xDAA;
		constexpr static u32 SPU_STAT = 0xDAE;
		constexpr static u32 KEY_OFF = 0xD8C;
		constexpr static u32 PMON = 0xD90;
		constexpr static u32 NOISE_EN = 0xD94;
		constexpr static u32 REVERB_EN = 0xD98;
		constexpr static u32 CD_INPUT_VOL_LEFT = 0xDB0;
		constexpr static u32 CD_INPUT_VOL_RIGHT = 0xDB2;
		constexpr static u32 EXT_INPUT_VOL_LEFT = 0xDB4;
		constexpr static u32 EXT_INPUT_VOL_RIGHT = 0xDB6;
		constexpr static u32 SOUND_RAM_TRANSFER_CONTROL = 0xDAC;
		constexpr static u32 SOUND_RAM_TRANSFER_ADDRESS = 0xDA6;
		constexpr static u32 SOUND_RAM_TRANSFER_FIFO = 0xDA8;
		constexpr static u32 VOICE_REGS_START = 0xC00;
		constexpr static u32 VOICE_REGS_END = 0xD7F;

		constexpr static u32 VOICE_REG_VOL_LEFT = 0x00;
		constexpr static u32 VOICE_REG_VOL_RIGHT = 0x02;
	}
}