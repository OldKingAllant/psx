#pragma once

#include <common/Defs.hpp>
#include <psxemu/include/psxemu/SPUStructs.hpp>

#include <array>

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

		std::pair<i16, i16> Step();

		static constexpr u16 PITCH_MAX_VALUE = 0x4000;
		static constexpr i16 RELEASE_STEP = -8;
		static constexpr i32 MIN_VOLUME = -0x8000;
		static constexpr i32 MAX_VOLUME = 0x7FFF;

		friend class SPU;

		enum class AdsrPhase {
			NONE,
			ATTACK,
			DECAY,
			SUSTAIN,
			RELEASE
		};

		struct VolumeEnvelope {
			bool exponential;
			bool increase;
			u8 shift;
			u8 step;
			bool phase_negative;

			i32 calc_step;
			i32 counter_increment;
			i32 counter;

			i16 level;

			void Reset(bool _exponential, bool _increase, 
				u8 _shift, u8 _step, bool _phase_negative);

			bool Step();
		};

		struct VolumeSweep {
			VolumeEnvelope envelope;
			bool enabled;
			i16 level;

			void Reset(SPU_VoiceVolume vol);
			void Step();
		};

		struct PitchCounter {
			u32 counter;

			u8 sample_index() const {
				return (counter >> 12) & 0x1F;
			}

			u8 gauss_index() const {
				return (counter >> 4) & 0xFF;
			}
		};

		struct Noise {
			bool cycle_odd;
			i32 timer;
			i16 noise_level;
		};

	private :
		AdsrPhase GetNextAdsrPhase();
		void ResetAdsrPhase();
		void StartAdsrPhase(AdsrPhase phase);
		void StepAdsr();

		bool CalculatePitch();
		void CalculateNoise();

		void ReadNextBlock();

	private :
		u8 m_voice_id;
		SPU* m_spu;
		SPU_Voice m_config;
		
		VolumeEnvelope m_adsr_envelope;
		i16 m_adsr_target;

		VolumeSweep m_sweep_left;
		VolumeSweep m_sweep_right;

		bool m_is_noise;
		bool m_enable_modulation;

		bool m_is_on;

		AdsrPhase m_curr_adsr_phase;
		PitchCounter m_pitch_counter;
		Noise m_noise;

		std::array<i16, 2> m_old_decode_samples;
		std::array<i16, 3> m_old_out_samples;
		
		SPU_ADPCM_Block m_curr_block;
		std::array<i16, 28> m_decoded_block;
		bool m_has_block;

		u32 m_curr_address;
		bool m_endx_flag;

		i16 m_sample_left;
		i16 m_sample_right;
	};
}