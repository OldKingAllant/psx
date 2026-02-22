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
		constexpr static u32 REVERB_APF_OFFSET_1 = 0xDC0;
		constexpr static u32 REVERB_APF_OFFSET_2 = 0xDC2;
		constexpr static u32 REVERB_REFLECTION_VOLUME_1 = 0xDC4;
		constexpr static u32 REVERB_COMB_VOLUME_1 = 0xDC6;
		constexpr static u32 REVERB_COMB_VOLUME_2 = 0xDC8;
		constexpr static u32 REVERB_COMB_VOLUME_3 = 0xDCA;
		constexpr static u32 REVERB_COMB_VOLUME_4 = 0xDCC;
		constexpr static u32 REVERB_REFLECTION_VOLUME_2 = 0xDCE;
		constexpr static u32 REVERB_APF_VOLUME_1 = 0xDD0;
		constexpr static u32 REVERB_APF_VOLUME_2 = 0xDD2;
		constexpr static u32 REVERB_SAME_SIDE_REFL_ADDRESS_1_LEFT = 0xDD4;
		constexpr static u32 REVERB_SAME_SIDE_REFL_ADDRESS_1_RIGHT = 0xDD6;
		constexpr static u32 REVERB_COMB_ADDRESS_1_LEFT = 0xDD8;
		constexpr static u32 REVERB_COMB_ADDRESS_1_RIGHT = 0xDDA;
		constexpr static u32 REVERB_COMB_ADDRESS_2_LEFT = 0xDDC;
		constexpr static u32 REVERB_COMB_ADDRESS_2_RIGHT = 0xDDE;
		constexpr static u32 REVERB_SAME_SIDE_REFL_ADDRESS_2_LEFT = 0xDE0;
		constexpr static u32 REVERB_SAME_SIDE_REFL_ADDRESS_2_RIGHT = 0xDE2;
		constexpr static u32 REVERB_DIFFERENT_SIDE_REFL_ADDRESS_1_LEFT = 0xDE4;
		constexpr static u32 REVERB_DIFFERENT_SIDE_REFL_ADDRESS_1_RIGHT = 0xDE6;
		constexpr static u32 REVERB_COMB_ADDRESS_3_LEFT = 0xDE8;
		constexpr static u32 REVERB_COMB_ADDRESS_3_RIGHT = 0xDEA;
		constexpr static u32 REVERB_COMB_ADDRESS_4_LEFT = 0xDEC;
		constexpr static u32 REVERB_COMB_ADDRESS_4_RIGHT = 0xDEE;
		constexpr static u32 REVERB_DIFFERENT_SIDE_REFL_ADDRESS_2_LEFT = 0xDF0;
		constexpr static u32 REVERB_DIFFERENT_SIDE_REFL_ADDRESS_2_RIGHT = 0xDF2;
		constexpr static u32 REVERB_APF_ADDRESS_1_LEFT = 0xDF4;
		constexpr static u32 REVERB_APF_ADDRESS_1_RIGHT = 0xDF6;
		constexpr static u32 REVERB_APF_ADDRESS_2_LEFT = 0xDF8;
		constexpr static u32 REVERB_APF_ADDRESS_2_RIGHT = 0xDFA;
		constexpr static u32 REVERB_INPUT_VOL_LEFT = 0xDFC;
		constexpr static u32 REVERB_INPUT_VOL_RIGHT = 0xDFE;

		constexpr static u32 SPU_CNT = 0xDAA;
		constexpr static u32 SPU_STAT = 0xDAE;
		constexpr static u32 KEY_OFF = 0xD8C;
		constexpr static u32 KEY_ON = 0xD88;
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
		constexpr static u32 VOICE_REG_PITCH = 0x04;
		constexpr static u32 VOICE_REG_START_ADDRESS = 0x06;
		constexpr static u32 VOICE_REG_ADSR_LOW = 0x08;
		constexpr static u32 VOICE_REG_ADSR_HIGH = 0x0A;
	}
}