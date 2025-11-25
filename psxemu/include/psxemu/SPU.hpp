#pragma once

#include <common/Defs.hpp>
#include <common/Queue.hpp>
#include <vector>
#include <memory>

#include "SPUStructs.hpp"

namespace psx {
	struct system_status;

	class SPUVoice;

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
		void WriteSoundRam(const u16* buf, u64 count);

		friend class DebugView;

		static constexpr inline u64 RAM_SIZE = 512 * 1024;
		static constexpr inline u64 FIFO_TRANSFER_DELAY = 0x300;
		static constexpr inline u32 NUM_VOICES = 24;
		static constexpr inline u64 SAMPLE_FREQUENCY = 44100;
		static constexpr inline u64 CYCLES_PER_SAMPLE = SYSTEM_CLOCK / SAMPLE_FREQUENCY;

		friend void fifo_transfer_callback(void*, u64);

		~SPU();

	private :
		void UpdateStat();
		void FifoTransferComplete();

	private :
		system_status* m_sys_status;

		struct {
			SPU_VoiceVolume m_mainvolume_left;
			SPU_VoiceVolume m_mainvolume_right;

			i16 m_reverb_vol_left;
			i16 m_reverb_vol_right;

			SPU_ReverbRegs m_reverb;
			SPU_Cnt m_cnt;
			SPU_Stat m_stat;
			SPU_VoicePitchModulation m_pmon;
			SPU_VoiceNoiseEnable m_noise_en;
			SPU_VoiceReverbEnable m_reverb_en;

			i16 m_cd_audio_in_vol_left;
			i16 m_cd_audio_in_vol_right;
			i16 m_ext_audio_in_vol_left;
			i16 m_ext_audio_in_vol_right;

			SPU_SoundRamTransferControl m_transfer_control;

			u16 m_ram_transfer_address;
		} m_regs;

		u32 m_curr_ram_transfer_address;
		Queue<u16, 32> m_fifo;
		std::vector<u8> m_sound_ram;
		u64 m_fifo_transfer_event;

		std::unique_ptr<SPUVoice[]> m_voices;
	};
}