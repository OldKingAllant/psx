#include <psxemu/include/psxemu/SPU.hpp>
#include <psxemu/include/psxemu/SPURegisters.hpp>
#include <psxemu/include/psxemu/SPUVoice.hpp>

#include <psxemu/include/psxemu/SystemStatus.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <common/Errors.hpp>

#include <thirdparty/magic_enum/include/magic_enum/magic_enum.hpp>

namespace psx {
	SPU::SPU(system_status* sys_status) :
		m_sys_status{ sys_status }, 
		m_regs{},
		m_curr_ram_transfer_address{},
		m_fifo{},
		m_sound_ram{},
		m_fifo_transfer_event{INVALID_EVENT},
		m_voices{} {
		m_sound_ram.resize(RAM_SIZE);

		m_voices = std::make_unique<SPUVoice[]>(NUM_VOICES);
		for (size_t id = 0; id < NUM_VOICES; id++) {
			m_voices[id].SetId(u8(id));
			m_voices[id].SetSPU(this);
		}
	}

	void fifo_transfer_callback(void*, u64);

	u8 SPU::Read8(u32 address) {
		return u8(Read16(address & ~1) >> (8 * (address & 1)));
	}

	u16 SPU::Read16(u32 address) {
		switch (address)
		{
		case SPU_REGS::SPU_STAT:
			UpdateStat();
			return m_regs.m_stat.reg;
			break;
		case SPU_REGS::SPU_CNT:
			return m_regs.m_cnt.reg;
			break;
		default:
			LOG_ERROR("SPU", "[SPU] Read invalid/unimplemented register: {:#x}", address);
			LOG_FLUSH();
			error::DebugBreak();
			break;
		}
		
		return 0x0;
	}

	u32 SPU::Read32(u32 address) {
		u16 low = Read16(address);
		u16 high = Read16(address + 2);
		return (high << 16) | low;
	}

#pragma optimize("", off)
	void SPU::Write8(u32 address, u8 value) {
		LOG_ERROR("SPU", "[SPU] WRITE 8 at {:#010x}", address);
		if (address & 1) {
			LOG_ERROR("SPU", "[SPU] IGNORED 8 BIT WRITE TO ODD ADDRESS");
			return;
		}

		Write16(address, u16(value));
	}

	void SPU::Write16(u32 address, u16 value) {
		if (address & 1) [[unlikely]] {
			LOG_ERROR("SPU", "[SPU] UNALIGNED WRITE AT {:#x}", address);
			LOG_FLUSH();
			error::DebugBreak();
		}

		switch (address)
		{
		case SPU_REGS::MAINVOL_LEFT:
			m_regs.m_mainvolume_left.reg = value;
			break;
		case SPU_REGS::MAINVOL_RIGHT:
			m_regs.m_mainvolume_right.reg = value;
			break;
		case SPU_REGS::REVERB_LVOL:
			m_regs.m_reverb.vol_left = i16(value);
			break;
		case SPU_REGS::REVERB_RVOL:
			m_regs.m_reverb.vol_right = i16(value);
			break;
		case SPU_REGS::SPU_CNT:
			m_regs.m_cnt.reg = value;
			m_regs.m_stat.current_spu_mode = u8(m_regs.m_cnt.reg & 0x1F);

			/*
			15    SPU Enable              (0=Off, 1=On)       (Don't care for CD Audio)
			14    Mute SPU                (0=Mute, 1=Unmute)  (Don't care for CD Audio)
			13-10 Noise Frequency Shift   (0..0Fh = Low .. High Frequency)
			9-8   Noise Frequency Step    (0..03h = Step "4,5,6,7")
			7     Reverb Master Enable    (0=Disabled, 1=Enabled)
			6     IRQ9 Enable (0=Disabled/Acknowledge, 1=Enabled; only when Bit15=1)
			5-4   Sound RAM Transfer Mode (0=Stop, 1=ManualWrite, 2=DMAwrite, 3=DMAread)
			3     External Audio Reverb   (0=Off, 1=On)
			2     CD Audio Reverb         (0=Off, 1=On) (for CD-DA and XA-ADPCM)
			1     External Audio Enable   (0=Off, 1=On)
			0     CD Audio Enable         (0=Off, 1=On) (for CD-DA and XA-ADPCM)
			*/


			LOG_DEBUG("SPU", "[SPU] Write control ({:#06x}):", value);
			LOG_DEBUG("SPU", "      SPU enable        : {}", bool(m_regs.m_cnt.enable));
			LOG_DEBUG("SPU", "      Noise freq. shift : {:x}", u8(m_regs.m_cnt.noise_freq_shift));
			LOG_DEBUG("SPU", "      Noise freq. step  : {:x}", u8(m_regs.m_cnt.noise_freq_step));
			LOG_DEBUG("SPU", "      Reverb enable     : {}", bool(m_regs.m_cnt.reverb_master_en));
			LOG_DEBUG("SPU", "      IRQ9 Enable       : {}", bool(m_regs.m_cnt.irq9_enable));
			LOG_DEBUG("SPU", "      Transfer mode     : {}", magic_enum::enum_name(m_regs.m_cnt.transer_mode));
			LOG_DEBUG("SPU", "      Ext. audio reverb : {}", bool(m_regs.m_cnt.ext_audio_reverb));
			LOG_DEBUG("SPU", "      CD Audio reverb   : {}", bool(m_regs.m_cnt.cd_audio_reverb));
			LOG_DEBUG("SPU", "      Ext. audio enable : {}", bool(m_regs.m_cnt.ext_audio_en));
			LOG_DEBUG("SPU", "      CD Audio enable   : {}", bool(m_regs.m_cnt.cd_audio_en));

			if (!m_regs.m_cnt.enable) {
				return;
			}

			switch (m_regs.m_cnt.transer_mode)
			{
			case SoundRamTransferMode::STOP:
				break;
			case SoundRamTransferMode::MANUAL:
				m_fifo_transfer_event = m_sys_status->scheduler.Schedule(
					FIFO_TRANSFER_DELAY, fifo_transfer_callback,
					std::bit_cast<void*>(this)
				);
				break;
			case SoundRamTransferMode::DMA_WRITE:
			case SoundRamTransferMode::DMA_READ:
				break;
			default:
				error::Unreachable();
				break;
			}
			break;
		case SPU_REGS::KEY_OFF:
			LOG_DEBUG("SPU", "[SPU] KEY OFF LO {:016b}", value);
			break;
		case SPU_REGS::KEY_OFF + 2:
			LOG_DEBUG("SPU", "[SPU] KEY OFF HI {:08b}", u8(value));
			break;
		case SPU_REGS::PMON:
			m_regs.m_pmon.m_reg = value;
			break;
		case SPU_REGS::PMON + 2:
			m_regs.m_pmon.m_reg |= u32(value) << 16;
			LOG_DEBUG("SPU", "[SPU] PITCH MODULATION {:024b}", m_regs.m_pmon.m_reg);
			break;
		case SPU_REGS::NOISE_EN:
			m_regs.m_noise_en.reg = value;
			break;
		case SPU_REGS::NOISE_EN + 2:
			m_regs.m_noise_en.reg |= u32(value) << 16;
			LOG_DEBUG("SPU", "[SPU] NOISE ENABLE {:024b}", m_regs.m_noise_en.reg);
			break;
		case SPU_REGS::REVERB_EN:
			m_regs.m_reverb_en.reg = value;
			break;
		case SPU_REGS::REVERB_EN + 2:
			m_regs.m_reverb_en.reg |= u32(value) << 16;
			LOG_DEBUG("SPU", "[SPU] REVERB ENABLE {:024b}", m_regs.m_reverb_en.reg);
			break;
		case SPU_REGS::CD_INPUT_VOL_LEFT:
			m_regs.m_cd_audio_in_vol_left = i16(value);
			LOG_DEBUG("SPU", "[SPU] CD INPUT VOLUME LEFT {:#06x}", m_regs.m_cd_audio_in_vol_left);
			break;
		case SPU_REGS::CD_INPUT_VOL_RIGHT:
			m_regs.m_cd_audio_in_vol_right = i16(value);
			LOG_DEBUG("SPU", "[SPU] CD INPUT VOLUME RIGHT {:#06x}", m_regs.m_cd_audio_in_vol_right);
			break;
		case SPU_REGS::EXT_INPUT_VOL_LEFT:
			m_regs.m_ext_audio_in_vol_left = i16(value);
			LOG_DEBUG("SPU", "[SPU] EXT INPUT VOLUME RIGHT {:#06x}", m_regs.m_ext_audio_in_vol_left);
			break;
		case SPU_REGS::EXT_INPUT_VOL_RIGHT:
			m_regs.m_ext_audio_in_vol_right = i16(value);
			LOG_DEBUG("SPU", "[SPU] EXT INPUT VOLUME RIGHT {:#06x}", m_regs.m_ext_audio_in_vol_right);
			break;
		case SPU_REGS::SOUND_RAM_TRANSFER_CONTROL:
			m_regs.m_transfer_control.reg = value;
			LOG_DEBUG("SPU", "[SPU] TRANSFER CONTROL: {}", magic_enum::enum_name(m_regs.m_transfer_control.type));
			break;
		case SPU_REGS::SOUND_RAM_TRANSFER_ADDRESS:
			m_regs.m_ram_transfer_address = value;
			m_curr_ram_transfer_address = u32(value) << 3;
			LOG_DEBUG("SPU", "[SPU] TRANSFER ADDRESS: {:#06x}", m_regs.m_ram_transfer_address);
			break;
		case SPU_REGS::SOUND_RAM_TRANSFER_FIFO:
			if (m_regs.m_cnt.transer_mode != SoundRamTransferMode::STOP) {
				LOG_WARN("SPU", "[SPU] FIFO WRITE NOT IN STOP MODE!");
				LOG_FLUSH();
				error::DebugBreak();
				return;
			}
			if (m_fifo.full()) [[unlikely]] {
				LOG_WARN("SPU", "[SPU] FIFO FULL");
				return;
			}
			m_fifo.queue(value);
			break;
		default:
			if (address >= SPU_REGS::VOICE_REGS_START && address <= SPU_REGS::VOICE_REGS_END) {
				u16 address_temp = address - SPU_REGS::VOICE_REGS_START;
				u16 voice_id = address_temp >> 4;
				u16 reg_id = address_temp & 0xF;

				switch (reg_id)
				{
				case SPU_REGS::VOICE_REG_VOL_LEFT: {
					SPU_VoiceVolume vol{};
					vol.set_u16(value);
					m_voices[voice_id].SetVolLeft(vol);
					return;
				} break;
				default:
					break;
				}
			}
			LOG_ERROR("SPU", "[SPU] Write invalid/unimplemented register: {:#x}", address);
			LOG_FLUSH();
			error::DebugBreak();
			break;
		}
	}

	void SPU::Write32(u32 address, u32 value) {
		LOG_ERROR("SPU", "[SPU] WRITE 32 {:x}", address);
		Write16(address, u16(value));
		Write16(address + 0x2, u16(value >> 16));
	}

	void SPU::WriteSoundRam(const u16* buf, u64 count) {
		if (m_curr_ram_transfer_address >= RAM_SIZE) [[unlikely]] {
			LOG_ERROR("SPU", "[SPU] RAM WRITE OUT OF BOUNDS (ADDRESS {:#010x})",
				m_curr_ram_transfer_address);
			return;
		}

		auto ram_ptr = std::bit_cast<u16*>(m_sound_ram.data() + m_curr_ram_transfer_address);

		switch (m_regs.m_transfer_control.type)
		{
		case SoundRamTransferControlType::FILL_0:
		case SoundRamTransferControlType::FILL_1:
		case SoundRamTransferControlType::FILL_6:
		case SoundRamTransferControlType::FILL_7:
			std::fill_n(ram_ptr, count, buf[count - 1]);
			m_curr_ram_transfer_address += u32(count << 1);
			break;
		case SoundRamTransferControlType::NORMAL:
			std::copy_n(buf, count, ram_ptr);
			m_curr_ram_transfer_address += u32(count << 1);
			break;
		case SoundRamTransferControlType::REP2: {
			for (u64 curr_pos = 0; curr_pos < count; curr_pos += 2) {
				ram_ptr[curr_pos] = buf[curr_pos];
				ram_ptr[curr_pos + 1] = buf[curr_pos];
			}
			m_curr_ram_transfer_address += u32((count + 1) & ~1) << 1;
		} break;
		case SoundRamTransferControlType::REP4: {
			for (u64 curr_pos = 0; curr_pos < count; curr_pos += 4) {
				ram_ptr[curr_pos] = buf[curr_pos];
				ram_ptr[curr_pos + 1] = buf[curr_pos];
				ram_ptr[curr_pos + 2] = buf[curr_pos];
				ram_ptr[curr_pos + 3] = buf[curr_pos];
			}
			m_curr_ram_transfer_address += u32((count + 3) & ~3) << 1;
		} break;
		case SoundRamTransferControlType::REP8: {
			for (u64 curr_pos = 7; curr_pos < count; curr_pos += 8) {
				ram_ptr[curr_pos] = buf[curr_pos];
				ram_ptr[curr_pos + 1] = buf[curr_pos];
				ram_ptr[curr_pos + 2] = buf[curr_pos];
				ram_ptr[curr_pos + 3] = buf[curr_pos];
				ram_ptr[curr_pos + 4] = buf[curr_pos];
				ram_ptr[curr_pos + 5] = buf[curr_pos];
				ram_ptr[curr_pos + 6] = buf[curr_pos];
				ram_ptr[curr_pos + 7] = buf[curr_pos];
				m_curr_ram_transfer_address += 16;
			}
		} break;
		default:
			error::Unreachable();
			break;
		}
	}

	SPU::~SPU()
	{}

	void SPU::UpdateStat() {
		m_regs.m_stat.transfer_busy = m_fifo_transfer_event != INVALID_EVENT;

		if (!m_regs.m_stat.transfer_busy) {
			m_regs.m_stat.dma_read_req = m_regs.m_cnt.transer_mode == SoundRamTransferMode::DMA_READ;
			m_regs.m_stat.dma_write_req = m_regs.m_cnt.transer_mode == SoundRamTransferMode::DMA_WRITE;
			m_regs.m_stat.dma_request = m_regs.m_stat.dma_read_req || m_regs.m_stat.dma_write_req;
		}
		else {
			m_regs.m_stat.dma_read_req  = false; 
			m_regs.m_stat.dma_write_req = false;
			m_regs.m_stat.dma_request   = false;
		}
	}

	void SPU::FifoTransferComplete() {
		m_fifo_transfer_event = INVALID_EVENT;
		WriteSoundRam(&m_fifo.peek(), m_fifo.len());
		m_fifo.clear();
	}

	void fifo_transfer_callback(void* spu, u64 cycles_late) {
		std::bit_cast<SPU*>(spu)->FifoTransferComplete();
	}

#pragma optimize("", on)
}