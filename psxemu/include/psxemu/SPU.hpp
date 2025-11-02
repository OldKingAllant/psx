#pragma once

#include <common/Defs.hpp>

#include "SPUStructs.hpp"

namespace psx {
	struct system_status;

	class SPU {
	public :
		SPU(system_status* sys_status);

		u8 Read8(u32 address);
		u16 Read16(u32 address);
		u32 Read32(u32 address);

		void Write8(u32 address, u8 value);
		void Write16(u32 address, u16 value);
		void Write32(u32 address, u32 value);

		//Maybe specific r/w for DMA?

		friend class DebugView;

	private :
		system_status* m_sys_status;

		struct {
			SPU_VoiceVolume m_mainvolume_left;
			SPU_VoiceVolume m_mainvolume_right;

			i16 m_reverb_vol_left;
			i16 m_reverb_vol_right;
		} m_regs;
	};
}