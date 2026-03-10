#pragma once

#include <common/Defs.hpp>
#include <common/Queue.hpp>
#include <vector>
#include <memory>

#include "SPUStructs.hpp"
#include "FirResample.hpp"

#include <thirdparty/wave/src/wave/file.h>

namespace psx {
	struct system_status;

	class SPUVoice;
	class AudioBackend;

	class SPU {
	public :
		SPU(system_status* sys_status);

		void SetupEvents();

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
		static constexpr inline u64 CAPTURE_BUFFER_SIZE = 1024;
		static constexpr inline u64 FIFO_TRANSFER_DELAY = 0x300;
		static constexpr inline u64 FIFO_SIZE_BYTES = 64;
		static constexpr inline u32 NUM_VOICES = 24;
		static constexpr inline u64 SAMPLE_FREQUENCY = 44100;
		static constexpr inline u64 CYCLES_PER_SAMPLE = SYSTEM_CLOCK / SAMPLE_FREQUENCY;

		static constexpr inline u64 VOICE_1_CAPTURE_BUFFER_POS = 0x800;
		static constexpr inline u64 VOICE_3_CAPTURE_BUFFER_POS = 0xC00;

		static constexpr inline u32 REVERB_BUFFER_END = 0x7FFFE;

		friend void fifo_transfer_callback(void*, u64);
		friend void sample_callback(void*, u64);

		~SPU();

		friend class SPUVoice;

		u32 WriteRamDirect16(u16 value, u32 address);
		u32 WriteRamDirect8(u8 value, u32 address);

		std::pair<u8, u32> ReadRamDirect8(u32 address);
		std::pair<u16, u32> ReadRamDirect16(u32 address);

		void DmaWrite(u32 value);
		u32 DmaRead();

	private :
		void UpdateStat();
		void FifoTransferComplete();
		void SampleCycle(u64 cycles_late);

		void CheckRamIRQ(u32 ram_address);

		std::pair<i32, i32> DoReverb(i32 l, i32 r);
		void ReverbWrite(i16 value, i32 offset);
		i16 ReverbRead(i32 offset);

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
			u16 m_irq_address;
		} m_regs;

		u32 m_curr_ram_transfer_address;
		Queue<u16, 32> m_fifo;
		std::vector<u8> m_sound_ram;
		u64 m_fifo_transfer_event;

		std::unique_ptr<SPUVoice[]> m_voices;

		bool m_irq_happened;
		bool m_reverb_odd_cycle;

		u32 m_curr_voice1_capture_pos;
		u32 m_curr_voice3_capture_pos;
		u32 m_reverb_buf_address;

		FirResampler m_fir_left;
		FirResampler m_fir_right;

		std::unique_ptr<AudioBackend> m_backend;
		std::vector<i16> m_audio_buffer;
		size_t m_curr_buffer_pos;

		wave::File m_wavefile;
	};
}