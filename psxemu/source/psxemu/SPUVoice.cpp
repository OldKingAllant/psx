#include <psxemu/include/psxemu/SPUVoice.hpp>
#include <psxemu/include/psxemu/SPU.hpp>
#include <psxemu/include/psxemu/ADPCM.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>
#include <common/Errors.hpp>

#include <thirdparty/magic_enum/include/magic_enum/magic_enum.hpp>

#include <algorithm>
#include <tuple>
#include <bit>

namespace psx {
	SPUVoice::SPUVoice() :
		m_voice_id{0xff},
		m_spu{nullptr},
		m_config{},
		m_adsr_envelope{},
		m_adsr_target{},
		m_sweep_left{},
		m_sweep_right{},
		m_is_noise{false},
		m_enable_modulation{false},
		m_is_on{false},
		m_curr_adsr_phase{ AdsrPhase::NONE },
		m_pitch_counter{},
		m_noise{},
		m_old_decode_samples{},
		m_old_out_samples{},
		m_curr_block{},
		m_decoded_block{},
		m_has_block{false},
		m_curr_address{},
		m_endx_flag{true},
		m_sample_left{},
		m_sample_right{}
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
		}
		else {
			LOG_DEBUG("SPU", "      MODE  : {}", magic_enum::enum_name(m_config.m_volume_left.sweep.mode));
			LOG_DEBUG("SPU", "      DIR   : {}", magic_enum::enum_name(m_config.m_volume_left.sweep.direction));
			LOG_DEBUG("SPU", "      PHASE : {}", magic_enum::enum_name(m_config.m_volume_left.sweep.phase));
			LOG_DEBUG("SPU", "      SHIFT : {:x}", u8(m_config.m_volume_left.sweep.shift));
			LOG_DEBUG("SPU", "      STEP  : {:x}", u8(m_config.m_volume_left.sweep.step));
		}

		m_sweep_left.Reset(vol);
	}
	
	void SPUVoice::SetVolRight(SPU_VoiceVolume vol) {
		m_config.m_volume_right = vol;

		LOG_DEBUG("SPU", "[SPU] VOICE {} SET VOLUME RIGHT", m_voice_id);
		LOG_DEBUG("SPU", "      TYPE: {}", magic_enum::enum_name(m_config.m_volume_right.mode));

		if (m_config.m_volume_right.mode == SPU_VoiceVolumeMode::VOLUME) {
			LOG_DEBUG("SPU", "      VOLUME: {:#06x}", m_config.m_volume_right.volume.half_volume << 1);
		}
		else {
			LOG_DEBUG("SPU", "      MODE  : {}", magic_enum::enum_name(m_config.m_volume_right.sweep.mode));
			LOG_DEBUG("SPU", "      DIR   : {}", magic_enum::enum_name(m_config.m_volume_right.sweep.direction));
			LOG_DEBUG("SPU", "      PHASE : {}", magic_enum::enum_name(m_config.m_volume_right.sweep.phase));
			LOG_DEBUG("SPU", "      SHIFT : {:x}", u8(m_config.m_volume_right.sweep.shift));
			LOG_DEBUG("SPU", "      STEP  : {:x}", u8(m_config.m_volume_right.sweep.step));
		}

		m_sweep_right.Reset(vol);
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

		ResetAdsrPhase();
	}

	void SPUVoice::SetStartAddress(u16 start) {
		m_config.m_adpcm_start_address = start;
		LOG_DEBUG("SPU", "[SPU] SET VOICE {} START ADDRESS: {:#08x}", m_voice_id, u32(start << 3));
	}

	void SPUVoice::SetRepeatAddress(u16 repeat) {
		m_config.m_repeat_address = repeat;
		LOG_DEBUG("SPU", "[SPU] SET VOICE {} REPEAT ADDRESS: {:#08x}", m_voice_id, u32(repeat << 3));
	}

	void SPUVoice::KeyOn() {
		m_adsr_envelope.level = 0;
		m_curr_address = (u32)m_config.m_adpcm_start_address << 3;
		m_is_on = true;
		StartAdsrPhase(AdsrPhase::ATTACK);
		m_has_block = false;
		m_pitch_counter.counter = 0;
		std::fill_n(m_decoded_block.data(), m_decoded_block.size(), 0x0);
		std::fill_n(m_old_decode_samples.data(), m_old_decode_samples.size(), 0x0);
		m_endx_flag = false;
	}

	void SPUVoice::KeyOff() {
		StartAdsrPhase(AdsrPhase::RELEASE);
		LOG_DEBUG("SPU", "[SPU] Voice {} current address {:#010x}", m_voice_id, m_curr_address);
	}

	void SPUVoice::SetNoiseEnable(bool enable_noise) {
		m_is_noise = enable_noise;
	}

	void SPUVoice::SetPitchModulation(bool enable_modulation) {
		m_enable_modulation = enable_modulation;
	}

	std::pair<i16, i16> SPUVoice::Step() {
		if (m_curr_adsr_phase == AdsrPhase::NONE) {
			return { 0, 0 };
		}

		if (!m_has_block && !m_is_noise) {
			ReadNextBlock();
		}

		StepAdsr();
		m_sweep_left.Step();
		m_sweep_right.Step();

		i32 curr_sample = {};

		if (m_is_noise) {
			CalculateNoise();
			curr_sample = m_noise.noise_level;
		}
		else {
			curr_sample = m_decoded_block[m_pitch_counter.sample_index()];
			curr_sample = InterpolateSamples(m_old_out_samples, curr_sample, m_pitch_counter.gauss_index());
			if (CalculatePitch()) {
				m_has_block = false;
			}
		}

		m_old_out_samples[0] = m_old_out_samples[1];
		m_old_out_samples[1] = m_old_out_samples[2];
		m_old_out_samples[2] = i16(curr_sample);

		curr_sample = (curr_sample * m_adsr_envelope.level) >> 15;

		if (m_curr_adsr_phase == AdsrPhase::NONE) {
			m_sample_left = 0;
			m_sample_right = 0;
			return { m_sample_left, m_sample_right };
		}

		m_sample_left = i16((curr_sample * m_sweep_left.level) >> 15);
		m_sample_right = i16((curr_sample * m_sweep_right.level) >> 15);

		return { m_sample_left, m_sample_right };
	}

	SPUVoice::AdsrPhase SPUVoice::GetNextAdsrPhase() {
		switch (m_curr_adsr_phase)
		{
		case AdsrPhase::NONE:
			return AdsrPhase::NONE;
		case AdsrPhase::ATTACK:
			return AdsrPhase::DECAY;
		case AdsrPhase::DECAY:
			return AdsrPhase::SUSTAIN;
		case AdsrPhase::SUSTAIN:
			return AdsrPhase::SUSTAIN;
		case AdsrPhase::RELEASE:
			return AdsrPhase::NONE;
		}
		return AdsrPhase::NONE;
	}

	void SPUVoice::ResetAdsrPhase() {
		switch (m_curr_adsr_phase)
		{
		case AdsrPhase::NONE:
			m_adsr_target = 0;
			m_adsr_envelope.Reset(false, false, 0, 0, false);
			break;
		case AdsrPhase::ATTACK:
			m_adsr_target = MAX_VOLUME;
			m_adsr_envelope.Reset(
				m_config.m_adsr.attack_mode == SPU_AttackMode::EXPONENTIAL, 
				true, 
				m_config.m_adsr.attack_shift,
				m_config.m_adsr.attack_step,
				false
			);
			break;
		case AdsrPhase::DECAY:
			m_adsr_target = i16(
				std::min<i32>(u32(m_config.m_adsr.sustain_level + 1) * 0x800,
					MAX_VOLUME)
			);
			m_adsr_envelope.Reset(
				true,
				false,
				m_config.m_adsr.decay_shift,
				0,
				false
			);
			break;
		case AdsrPhase::SUSTAIN:
			m_adsr_target = -1;
			m_adsr_envelope.Reset(
				m_config.m_adsr.sustain_mode == SPU_SustainMode::EXPONENTIAL,
				m_config.m_adsr.sustain_dir == SPU_SustainDirection::INCREASE,
				m_config.m_adsr.sustain_shift,
				m_config.m_adsr.sustain_step,
				false
			);
			break;
		case AdsrPhase::RELEASE:
			m_adsr_target = 0;
			m_adsr_envelope.Reset(
				m_config.m_adsr.release_mode == SPU_ReleaseMode::EXPONENTIAL,
				false,
				m_config.m_adsr.release_shift,
				0,
				false
			);
			break;
		}
	}

	void SPUVoice::StartAdsrPhase(AdsrPhase phase) {
		m_curr_adsr_phase = phase;
		if (m_curr_adsr_phase == AdsrPhase::NONE) {
			m_is_on = false;
			m_has_block = false;
			return;
		}
		ResetAdsrPhase();
	}

	void SPUVoice::StepAdsr() {
		m_adsr_envelope.Step();

		if (m_curr_adsr_phase != AdsrPhase::SUSTAIN) {
			bool target_reached = !m_adsr_envelope.increase ?
				(m_adsr_envelope.level <= m_adsr_target) :
				(m_adsr_envelope.level >= m_adsr_target);
			if (target_reached) {
				StartAdsrPhase(GetNextAdsrPhase());
			}
		}
	}

	bool SPUVoice::CalculatePitch() {
		auto step = m_config.m_sample_rate;

		if (m_enable_modulation && m_voice_id > 0) {
			auto vxout = m_spu->m_voices[m_voice_id - 1].m_old_out_samples[2];
			u16 factor = vxout + 0x8000;
			auto sext_step = i32(i16(step));
			sext_step = (sext_step * factor) >> 15;
			step = u16(sext_step & 0xFFFF);
		}

		if (step >= PITCH_MAX_VALUE - 1) {
			step = PITCH_MAX_VALUE;
		}

		m_pitch_counter.counter += (u32)step;
		if (m_pitch_counter.sample_index() >= 28) {
			auto index = m_pitch_counter.sample_index();
			
			m_pitch_counter.counter &= ~(0x1F << 12);
			index -= 28;
			index &= 0x1F;
			m_pitch_counter.counter |= u32(index) << 12;
			
			return true;
		}
		return false;
	}

	void SPUVoice::CalculateNoise() {
		auto is_odd = m_noise.cycle_odd;
		m_noise.cycle_odd = !m_noise.cycle_odd;

		if (!is_odd) {
			return;
		}

		auto step = m_spu->m_regs.m_cnt.noise_freq_step;
		auto shift = m_spu->m_regs.m_cnt.noise_freq_shift;

		m_noise.timer -= step;
		auto parity = ((m_noise.noise_level >> 15) & 1) ^
			((m_noise.noise_level >> 12) & 1) ^
			((m_noise.noise_level >> 11) & 1) ^
			((m_noise.noise_level >> 10) & 1) ^ 1;
		if (m_noise.timer < 0) {
			m_noise.noise_level = m_noise.noise_level * 2 + parity;
			m_noise.timer += (0x20000 >> shift);
			if (m_noise.timer < 0) {
				m_noise.timer += (0x20000 >> shift);
			}
		}
	}

	void SPUVoice::ReadNextBlock() {
		auto start = m_curr_address;
		constexpr auto READ_SIZE = sizeof(SPU_ADPCM_Block);

		std::array<u8, READ_SIZE> raw_block;
		for (size_t byte_pos = 0; byte_pos < READ_SIZE; byte_pos++) {
			std::tie(raw_block[byte_pos], m_curr_address) = m_spu->ReadRamDirect8(start + (u32)byte_pos);
		}

		std::copy_n(raw_block.data(), READ_SIZE,
			std::bit_cast<u8*>(&m_curr_block));

		m_decoded_block = DecodeADPCMBlock(m_curr_block, m_old_decode_samples[0],
			m_old_decode_samples[1]);

		if (u8(m_curr_block.flags) & u8(SPU_ADPCM_BlockFlags::LOOP_START)) {
			m_config.m_repeat_address = u16(start >> 3);
		}

		if (u8(m_curr_block.flags) & u8(SPU_ADPCM_BlockFlags::LOOP_END)) {
			m_curr_address = (i32)m_config.m_repeat_address << 3;

			if ((u8(m_curr_block.flags) & u8(SPU_ADPCM_BlockFlags::LOOP_REPEAT)) == 0) {
				m_adsr_envelope.level = 0;
				StartAdsrPhase(AdsrPhase::RELEASE);
			}

			m_endx_flag = true;
		}

		m_has_block = true;
	}

	void SPUVoice::VolumeEnvelope::Reset(bool _exponential, bool _increase, u8 _shift, u8 _step, bool _phase_negative) {
		exponential = _exponential;
		increase = _increase;
		shift = _shift;
		step = _step;
		phase_negative = _phase_negative;
		adsr_cycles = 0;
	}

	bool SPUVoice::VolumeEnvelope::Step() {
		if (adsr_cycles > 0) {
			adsr_cycles--;
		}

		auto cycles_to_increment = 1 << std::max(0, shift - 11);
		i32 vol_step = GetStep() << std::max(0, 11 - shift);

		if (exponential) {
			if (increase && level > 0x6000) {
				cycles_to_increment *= 4;
			}
			if (!increase) {
				vol_step = (vol_step * level) >> 15;
			}
		}

		if (adsr_cycles == 0) {
			adsr_cycles = cycles_to_increment;
			level = (i16)std::clamp<i32>((i32)level + vol_step, 0, 0x7FFF);
		}

		return increase ? (level >= SPUVoice::MAX_VOLUME) : (level <= 0);
	}

	i32 SPUVoice::VolumeEnvelope::GetStep() const {
		return increase ? (7 - step) : (-8 + step);
	}

	void SPUVoice::VolumeSweep::Reset(SPU_VoiceVolume vol) {
		enabled = false;
		if (vol.mode == SPU_VoiceVolumeMode::SWEEP) {
			enabled = true;
			envelope.Reset(
				vol.sweep.mode == SPU_SweepMode::EXPONENTIAL,
				vol.sweep.direction == SPU_SweepDirection::INCREASE,
				vol.sweep.shift,
				vol.sweep.step,
				vol.sweep.phase == SPU_SweepPhase::NEGATIVE
			);
		}
		else {
			level = (vol.volume.half_volume << 1);
		}
	}

	void SPUVoice::VolumeSweep::Step() {
		if (!enabled) {
			return;
		}

		enabled = envelope.Step();
		level = envelope.level;
	}
#pragma optimize("", off)
}