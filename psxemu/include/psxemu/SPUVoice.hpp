#pragma once

#include <common/Defs.hpp>
#include <psxemu/include/psxemu/SPUStructs.hpp>

#include <optional>

namespace psx {
	class SPU;

	class SPUVoice {
	public :
		SPUVoice();

		inline void SetId(u8 id) { m_voice_id = id; }
		inline void SetSPU(SPU* spu) { m_spu = spu; }

		void SetVolLeft(SPU_VoiceVolume vol);
		void SetVolRight(SPU_VoiceVolume vol);
		void SetPitch(u16 pitch);

		void SetADSRLow(u16 low);
		void SetADSRHigh(u16 high);

		void SetStartAddress(u16 start);

		static constexpr u16 PITCH_MAX_VALUE = 0x4000;


	private :
		u8 m_voice_id;
		SPU* m_spu;
		SPU_Voice m_config;

		i16 m_curr_volume_left;
		std::optional<i16> m_next_volume_left;

		i16 m_curr_volume_right;
		std::optional<i16> m_next_volume_right;
	};
}