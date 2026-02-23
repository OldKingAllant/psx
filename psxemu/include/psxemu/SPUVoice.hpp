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
		void SetRepeatAddress(u16 repeat);

		void KeyOn();
		void KeyOff();
		void SetNoiseEnable(bool enable_noise);
		void SetPitchModulation(bool enable_modulation);

		static constexpr u16 PITCH_MAX_VALUE = 0x4000;
		static constexpr i16 RELEASE_STEP = -8;

		friend class SPU;

		enum class AdsrPhase {
			NONE,
			ATTACK,
			SUSTAIN,
			RELEASE
		};

	private :
		void BeginAdsrPhase(AdsrPhase phase);

	private :
		u8 m_voice_id;
		SPU* m_spu;
		SPU_Voice m_config;

		i16 m_curr_volume_left;
		i16 m_curr_volume_right;
		std::optional<i16> m_next_volume_left;
		std::optional<i16> m_next_volume_right;
		i16 m_curr_adsr_level;
		i32 m_adsr_step;
		i32 m_counter_increment;

		bool m_is_noise;
		bool m_enable_modulation;

		AdsrPhase m_curr_adsr_phase;

		bool m_is_on;
	};
}