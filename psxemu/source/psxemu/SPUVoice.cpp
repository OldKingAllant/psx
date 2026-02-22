#include <psxemu/include/psxemu/SPUVoice.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <thirdparty/magic_enum/include/magic_enum/magic_enum.hpp>

namespace psx {
	SPUVoice::SPUVoice() :
		m_voice_id{0xff},
		m_spu{nullptr},
		m_config{},
		m_curr_volume_left{},
		m_next_volume_left{},
		m_curr_volume_right{},
		m_next_volume_right{}
	{
	}

#pragma optimize("", off)
	void SPUVoice::SetVolLeft(SPU_VoiceVolume vol) {
		m_config.m_volume_left = vol;

		/*
		15    Must be set       (1=Sweep Mode)
		14    Sweep Mode        (0=Linear, 1=Exponential)
		13    Sweep Direction   (0=Increase, 1=Decrease)
		12    Sweep Phase       (0=Positive, 1=Negative)
		7-11  Not used?         (should be zero)
		6-2   Sweep Shift       (0..1Fh = Fast..Slow)
		1-0   Sweep Step        (0..3 = "+7,+6,+5,+4" or "-8,-7,-6,-5") (inc/dec)
		*/

		LOG_DEBUG("SPU", "[SPU] VOICE {} SET VOLUME LEFT", m_voice_id);
		LOG_DEBUG("SPU", "      TYPE: {}", magic_enum::enum_name(m_config.m_volume_left.mode));
		
		if (m_config.m_volume_left.mode == SPU_VoiceVolumeMode::VOLUME) {
			LOG_DEBUG("SPU", "      VOLUME: {:#06x}", m_config.m_volume_left.volume.half_volume << 1);
			m_next_volume_left = m_config.m_volume_left.volume.half_volume << 1;
		}
		else {
			LOG_DEBUG("SPU", "      MODE  : {}", magic_enum::enum_name(m_config.m_volume_left.sweep.mode));
			LOG_DEBUG("SPU", "      DIR   : {}", magic_enum::enum_name(m_config.m_volume_left.sweep.direction));
			LOG_DEBUG("SPU", "      PHASE : {}", magic_enum::enum_name(m_config.m_volume_left.sweep.phase));
			LOG_DEBUG("SPU", "      SHIFT : {:x}", u8(m_config.m_volume_left.sweep.shift));
			LOG_DEBUG("SPU", "      STEP  : {:x}", u8(m_config.m_volume_left.sweep.step));
		}
	}
	
	void SPUVoice::SetVolRight(SPU_VoiceVolume vol) {
		m_config.m_volume_right = vol;

		LOG_DEBUG("SPU", "[SPU] VOICE {} SET VOLUME RIGHT", m_voice_id);
		LOG_DEBUG("SPU", "      TYPE: {}", magic_enum::enum_name(m_config.m_volume_right.mode));

		if (m_config.m_volume_right.mode == SPU_VoiceVolumeMode::VOLUME) {
			LOG_DEBUG("SPU", "      VOLUME: {:#06x}", m_config.m_volume_right.volume.half_volume << 1);
			m_next_volume_left = m_config.m_volume_right.volume.half_volume << 1;
		}
		else {
			LOG_DEBUG("SPU", "      MODE  : {}", magic_enum::enum_name(m_config.m_volume_right.sweep.mode));
			LOG_DEBUG("SPU", "      DIR   : {}", magic_enum::enum_name(m_config.m_volume_right.sweep.direction));
			LOG_DEBUG("SPU", "      PHASE : {}", magic_enum::enum_name(m_config.m_volume_right.sweep.phase));
			LOG_DEBUG("SPU", "      SHIFT : {:x}", u8(m_config.m_volume_right.sweep.shift));
			LOG_DEBUG("SPU", "      STEP  : {:x}", u8(m_config.m_volume_right.sweep.step));
		}
	}

	void SPUVoice::SetPitch(u16 pitch) {
		m_config.m_sample_rate = std::min(pitch, PITCH_MAX_VALUE);
		LOG_DEBUG("SPU", "[SPU] VOICE {} SET PITCH TO {:#06x}", m_voice_id, m_config.m_sample_rate);
	}

	void SPUVoice::SetADSRLow(u16 low) {
		m_config.m_adsr.lower = low;
	}

	void SPUVoice::SetADSRHigh(u16 high) {
		m_config.m_adsr.upper = high;

		LOG_DEBUG("SPU", "[SPU] VOICE {} SET ADSR ({:#010x})", m_voice_id, m_config.m_adsr.reg);

		LOG_DEBUG("SPU", "      ATTACK MODE      : {}", magic_enum::enum_name(m_config.m_adsr.attack_mode));
		LOG_DEBUG("SPU", "      ATTACK SHIFT     : {:#x}", u8(m_config.m_adsr.attack_shift));
		LOG_DEBUG("SPU", "      ATTACK STEP      : {:#x}", u8(m_config.m_adsr.attack_step));
		LOG_DEBUG("SPU", "      DECAY SHIFT      : {:#x}", u8(m_config.m_adsr.decay_shift));
		LOG_DEBUG("SPU", "      SUSTAIN LEVEL    : {:#x}", u8(m_config.m_adsr.sustain_level));
		LOG_DEBUG("SPU", "      SUSTAIN MODE     : {}", magic_enum::enum_name(m_config.m_adsr.sustain_mode));
		LOG_DEBUG("SPU", "      SUSTAIN DIRECTION: {}", magic_enum::enum_name(m_config.m_adsr.sustain_dir));
		LOG_DEBUG("SPU", "      SUSTAIN SHIFT    : {:#x}", u8(m_config.m_adsr.sustain_shift));
		LOG_DEBUG("SPU", "      SUSTAIN STEP     : {:#x}", u8(m_config.m_adsr.sustain_step));
		LOG_DEBUG("SPU", "      RELEASE MODE     : {}", magic_enum::enum_name(m_config.m_adsr.release_mode));
		LOG_DEBUG("SPU", "      RELEASE SHIFT    : {:#x}", u8(m_config.m_adsr.release_shift));
	}

	void SPUVoice::SetStartAddress(u16 start) {
		m_config.m_adpcm_start_address = start;
		LOG_DEBUG("SPU", "[SPU] SET VOICE {} START ADDRESS: {:#08x}", m_voice_id, u32(start << 3));
	}
#pragma optimize("", on)
}