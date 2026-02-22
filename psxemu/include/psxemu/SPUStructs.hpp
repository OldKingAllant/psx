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

		struct {
			u16 lower;
			u16 upper;
		};
		
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

	struct SPU_VoicePitchModulation {
		u32 m_reg;

		inline bool is_channel_modulated(u32 chan) const noexcept {
			return ((m_reg >> (chan + 1)) & 1) != 0;
		}
	};

	struct SPU_Voice {
		SPU_VoiceVolume m_volume_left;
		SPU_VoiceVolume m_volume_right;
		u16 m_sample_rate;
		u16 m_adpcm_start_address;
		SPU_VoiceADSR m_adsr;
	};

	/*
  1F801D84h spu   vLOUT   volume  Reverb Output Volume Left
  1F801D86h spu   vROUT   volume  Reverb Output Volume Right
  1F801DA2h spu   mBASE   base    Reverb Work Area Start Address in Sound RAM
  1F801DC0h rev00 dAPF1   disp    Reverb APF Offset 1
  1F801DC2h rev01 dAPF2   disp    Reverb APF Offset 2
  1F801DC4h rev02 vIIR    volume  Reverb Reflection Volume 1
  1F801DC6h rev03 vCOMB1  volume  Reverb Comb Volume 1
  1F801DC8h rev04 vCOMB2  volume  Reverb Comb Volume 2
  1F801DCAh rev05 vCOMB3  volume  Reverb Comb Volume 3
  1F801DCCh rev06 vCOMB4  volume  Reverb Comb Volume 4
  1F801DCEh rev07 vWALL   volume  Reverb Reflection Volume 2
  1F801DD0h rev08 vAPF1   volume  Reverb APF Volume 1
  1F801DD2h rev09 vAPF2   volume  Reverb APF Volume 2
  1F801DD4h rev0A mLSAME  src/dst Reverb Same Side Reflection Address 1 Left
  1F801DD6h rev0B mRSAME  src/dst Reverb Same Side Reflection Address 1 Right
  1F801DD8h rev0C mLCOMB1 src     Reverb Comb Address 1 Left
  1F801DDAh rev0D mRCOMB1 src     Reverb Comb Address 1 Right
  1F801DDCh rev0E mLCOMB2 src     Reverb Comb Address 2 Left
  1F801DDEh rev0F mRCOMB2 src     Reverb Comb Address 2 Right
  1F801DE0h rev10 dLSAME  src     Reverb Same Side Reflection Address 2 Left
  1F801DE2h rev11 dRSAME  src     Reverb Same Side Reflection Address 2 Right
  1F801DE4h rev12 mLDIFF  src/dst Reverb Different Side Reflect Address 1 Left
  1F801DE6h rev13 mRDIFF  src/dst Reverb Different Side Reflect Address 1 Right
  1F801DE8h rev14 mLCOMB3 src     Reverb Comb Address 3 Left
  1F801DEAh rev15 mRCOMB3 src     Reverb Comb Address 3 Right
  1F801DECh rev16 mLCOMB4 src     Reverb Comb Address 4 Left
  1F801DEEh rev17 mRCOMB4 src     Reverb Comb Address 4 Right
  1F801DF0h rev18 dLDIFF  src     Reverb Different Side Reflect Address 2 Left
  1F801DF2h rev19 dRDIFF  src     Reverb Different Side Reflect Address 2 Right
  1F801DF4h rev1A mLAPF1  src/dst Reverb APF Address 1 Left
  1F801DF6h rev1B mRAPF1  src/dst Reverb APF Address 1 Right
  1F801DF8h rev1C mLAPF2  src/dst Reverb APF Address 2 Left
  1F801DFAh rev1D mRAPF2  src/dst Reverb APF Address 2 Right
  1F801DFCh rev1E vLIN    volume  Reverb Input Volume Left
  1F801DFEh rev1F vRIN    volume  Reverb Input Volume Right
	*/

	union SPU_ReverbRegs {
#pragma pack(push, 1)
		struct {
			i16 vol_left;
			i16 vol_right;
			u16 work_area_start;
			i16 apf_off_1;
			i16 apf_off_2;
			i16 refl_vol_1;
			i16 comb_vol_1;
			i16 comb_vol_2;
			i16 comb_vol_3;
			i16 comb_vol_4;
			i16 refl_vol_2;
			i16 apf_vol_1;
			i16 apf_vol_2;
			u16 same_side_refl_add_1_left;
			u16 same_side_refl_add_1_right;
			u16 comb_add_1_left;
			u16 comb_add_1_right;
			u16 comb_add_2_left;
			u16 comb_add_2_right;
			u16 same_side_refl_add_2_left;
			u16 same_side_refl_add_2_right;
			u16 diff_side_refl_add_1_left;
			u16 diff_side_refl_add_1_right;
			u16 comb_add_3_left;
			u16 comb_add_3_right;
			u16 comb_add_4_left;
			u16 comb_add_4_right;
			u16 diff_side_refl_add_2_left;
			u16 diff_side_refl_add_2_right;
			u16 apf_add_1_left;
			u16 apf_add_1_right;
			u16 apf_add_2_left;
			u16 apf_add_2_right;
			i16 input_vol_left;
			i16 input_vol_right;
		};
#pragma pack(pop)

		i16 arr[35];
	};

	struct SPU_VoiceNoiseEnable {
		u32 reg;

		inline bool is_channel_noise(u32 chan) const noexcept {
			return ((reg >> chan) & 1) != 0;
		}
	};

	struct SPU_VoiceReverbEnable {
		u32 reg;

		inline bool is_channel_reverb(u32 chan) const noexcept {
			return ((reg >> chan) & 1) != 0;
		}
	};

	enum class SoundRamTransferControlType : u8 {
		FILL_0,
		FILL_1,
		NORMAL,
		REP2,
		REP4,
		REP8,
		FILL_6,
		FILL_7
	};

	union SPU_SoundRamTransferControl {
#pragma pack(push, 1)
		struct {
			u8 : 1;
			SoundRamTransferControlType type : 3;
		};
#pragma pack(pop)

		u16 reg;
	};
}