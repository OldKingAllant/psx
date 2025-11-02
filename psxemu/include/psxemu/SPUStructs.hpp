#pragma once

#include <common/Defs.hpp>

namespace psx {
	enum class SoundRamTransferMode : u8 {
		STOP      = 0,
		MANUAL    = 1,
		DMA_WRITE = 2,
		DMA_READ  = 3
	};

	union SPU_Cnt {
#pragma pack(push, 1)
		struct {
			bool cd_audio_en	  : 1;
			bool ext_audio_en     : 1;
			bool cd_audio_reverb  : 1;
			bool ext_audio_reverb : 1;
			SoundRamTransferMode transer_mode : 2;
			bool irq9_enable      : 1;
			bool reverb_master_en : 1;

			u8 noise_freq_step  : 2;
			u8 noise_freq_shift : 4;
			bool mute           : 1;
			bool enable         : 1;
		};
#pragma pack(pop)

		u16 reg;
	};

	union SPU_Stat {
#pragma pack(push, 1)
		struct {
			u8 current_spu_mode : 6;
			bool irq9_flag      : 1;
			bool dma_request    : 1;

			bool dma_write_req  : 1;
			bool dma_read_req   : 1;
			bool transfer_busy  : 1;
			bool writing_loc    : 1;
		};
#pragma pack(pop)

		u16 reg;
	};

	enum class SPU_AttackMode : u8 {
		LINEAR      = 0,
		EXPONENTIAL = 1
	};

	enum class SPU_ReleaseMode : u8 {
		LINEAR = 0,
		EXPONENTIAL = 1
	};

	enum class SPU_SustainMode : u8 {
		LINEAR = 0,
		EXPONENTIAL = 1
	};

	enum class SPU_SustainDirection : u8 {
		INCREASE = 0,
		DECREASE = 1
	};

	union SPU_VoiceADSR {
#pragma pack(push, 1)
		struct {
			u8 sustain_level : 4;
			u8 decay_shift   : 4;
			u8 attack_step   : 2;
			u8 attack_shift  : 5;
			SPU_AttackMode attack_mode : 1;

			u8 release_shift : 5;
			SPU_ReleaseMode release_mode : 1;
			u8 sustain_step  : 2;
			u8 sustain_shift : 5;
			u8 : 1;
			SPU_SustainDirection sustain_dir : 1;
			SPU_SustainMode sustain_mode : 1;
		};
#pragma pack(pop)

		u16 lower;
		u16 upper;
		
		u32 reg;
	};

	enum class SPU_VoiceVolumeMode : u8 {
		VOLUME = 0,
		SWEEP = 1
	};

	enum class SPU_SweepMode : u8 {
		LINEAR = 0,
		EXPONENTIAL = 1
	};

	enum class SPU_SweepDirection : u8 {
		INCREASE = 0,
		DECREASE = 1
	};

	enum class SPU_SweepPhase : u8 {
		POSITIVE = 0,
		NEGATIVE = 1
	};

	struct SPU_VoiceVolume {
#pragma pack(push, 1)
		union {
			struct {
				i16 half_volume;
			} volume;

			struct {
				u8 step : 2;
				u8 shift : 5;
				u8 : 1;

				u8 : 4;
				SPU_SweepPhase phase : 1;
				SPU_SweepDirection direction : 1;
				SPU_SweepMode  mode : 1;
			} sweep;

			u16 reg;
		};
#pragma pack(pop)

		SPU_VoiceVolumeMode mode;

		u16 to_u16() const {
			u16 return_value{reg};
			return_value &= ~(1 << 15);
			return_value |= (u16(mode) << 1);
			return return_value;
		}

		void set_u16(u16 val) {
			reg = val;
			mode = SPU_VoiceVolumeMode(val >> 15);
		}
	};

	//////////////////////////////////////

	union SPU_XA_ADPCM_Header {
#pragma pack(push, 1)
		struct {
			u8 shift  : 4;
			u8 filter : 2;
		};
#pragma pack(pop)

		u8 reg;
	};

	enum SPU_ADPCM_BlockFlags : u8 {
		LOOP_END = 1,
		LOOP_REPEAT = 2,
		LOOP_START = 3
	};

	struct SPU_ADPCM_Block {
		SPU_XA_ADPCM_Header  header;
		SPU_ADPCM_BlockFlags flags;
		u8 compressed_samples[14];
	};
}