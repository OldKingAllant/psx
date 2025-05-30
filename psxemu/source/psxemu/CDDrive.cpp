#include <psxemu/include/psxemu/CDDrive.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/Interrupts.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <common/Errors.hpp>

#include <psxemu/include/psxemu/CueBin.hpp>

namespace psx {
	CDDrive::CDDrive(system_status* sys_status) :
		m_index_reg{}, m_curr_cmd{}, m_new_cmd{}, 
		m_response_fifo{}, m_param_fifo{}, m_int_enable {}, m_int_flag{},
		m_soundmap_en{false}, m_want_data{false},
		m_volume{}, m_mute_adpcm{true}, m_sound_coding{},
		m_read_paused{false}, m_motor_on{false},
		m_mode{}, m_stat{}, m_sys_status {sys_status},
		m_idle{ true }, m_has_next_cmd{ false }, m_event_id{INVALID_EVENT},
		m_keep_history{ false }, m_history{}, m_cdrom{},
		m_lid_open{ false }, m_seek_loc{}, m_unprocessed_seek_loc{},
		m_has_unprocessed_seek{ false }, m_read_event{INVALID_EVENT},
		m_has_pending_read{ false }, m_curr_sector{}, m_pending_sector{},
		m_has_data_to_load{ false }, m_has_loaded_data{ false },
		m_curr_sector_size{}, m_pending_sector_size{}, 
		m_curr_sector_pos{} {
		m_index_reg.param_fifo_empty = true;
		m_index_reg.param_fifo_not_full = true;
		m_stat.shell_open = false;
		m_motor_on = true;
	}


	u8 CDDrive::Read8(u32 address) {
		switch (address)
		{
		case 0x00: {
			UpdateIndexRegister();
			return m_index_reg.reg;
		}
			break;
		case 0x1:
			return ReadReg1();
			break;
		case 0x2:
			return ReadReg2();
			break;
		case 0x3:
			return ReadReg3();
			break;
		default:
			LOG_ERROR("CDROM", "[CDROM] Reading invalid CDROM register {:#x}", 
				address);
			break;
		}

		return 0x00;
	}

	u16 CDDrive::Read16(u32 address) {
		return (u16)Read8(address) |
			((u16)Read8(address) << 8);
	}

	u32 CDDrive::Read32(u32 address) {
		return (u32)Read16(address) |
			((u32)Read16(address) << 16);
	}

	void CDDrive::Write8(u32 address, u8 value) {
		switch (address)
		{
		case 0x00:
			m_index_reg.index = value & 0x3;
			break;
		case 0x1:
			WriteReg1(value);
			break;
		case 0x2:
			WriteReg2(value);
			break;
		case 0x3:
			WriteReg3(value);
			break;
		default:
			LOG_ERROR("CDROM", "[CDROM] Writing invalid CDROM register {:#x}",
				address);
			break;
		}
	}

	void CDDrive::Write16(u32 address, u16 value) {
		Write8(address, (u8)value);
		Write8(address + 1, (u8)(value >> 8));
	}

	void CDDrive::Write32(u32 address, u32 value) {
		Write16(address, (u16)value);
		Write16(address + 2, (u16)(value >> 16));
	}

	void CDDrive::WriteReg1(u8 value) {
		switch (m_index_reg.index)
		{
		case 0:
			LOG_DEBUG("CDROM", "[CDROM] Command {:#x}", value);
			if (m_index_reg.transmission_busy) {
				LOG_WARN("CDROM", "[CDROM] Command transmission busy!");
				return;
			}

			if (m_idle) {
				m_curr_cmd = value;
				m_idle = false;
				CommandExecute();
				m_index_reg.transmission_busy = false;
			}
			else {
				if (m_has_next_cmd) {
					error::DebugBreak();
				}
				m_new_cmd = value;
				m_has_next_cmd = true;
				m_index_reg.transmission_busy = true;
			}
			break;
		case 1:
			LOG_DEBUG("CDROM", "[CDROM] Write do direct ADPCM");
			break;
		case 2: {
			m_sound_coding.reg = value;
			LOG_DEBUG("CDROM", "[CDROM] SOUND CODING = {:#x}", value);
			LOG_DEBUG("CDROM", "        Stereo              : {}", (bool)m_sound_coding.stereo);
			LOG_DEBUG("CDROM", "        Sample rate 18900Hz : {}", (bool)m_sound_coding.sampler_rate_18900);
			LOG_DEBUG("CDROM", "        8 bits per sample   : {}", (bool)m_sound_coding.bits_per_sample_8);
			LOG_DEBUG("CDROM", "        Emphasis            : {}", (bool)m_sound_coding.emphasis);
		}
			break;
		case 3: {
			m_volume.right_to_right = value;
		}
			break;
		default:
			error::DebugBreak();
			break;
		}
	}

	void CDDrive::WriteReg2(u8 value) {
		switch (m_index_reg.index)
		{
		case 0:
			LOG_DEBUG("CDROM", "[CDROM] Paramater {:#x}", value);
			if (m_param_fifo.full()) {
				LOG_WARN("CDROM", "[CDROM] But FIFO is full!");
				return;
			}
			m_param_fifo.queue(value);
			m_index_reg.param_fifo_empty = false;
			
			if (m_param_fifo.full())
				m_index_reg.param_fifo_not_full = false;
			break;
		case 1:
			m_int_enable.enable_bits = value & 0x1F;
			LOG_DEBUG("CDROM", "[CDROM] Interrupt enable {:#x}",
				(u8)m_int_enable.enable_bits);
			break;
		case 2:
			m_volume.left_to_left = value;
			break;
		case 3:
			m_volume.right_to_left = value;
			break;
		default:
			error::DebugBreak();
			break;
		}
	}

	void CDDrive::WriteReg3(u8 value) {
		switch (m_index_reg.index)
		{
		case 0:
			m_want_data = (bool)((value >> 7) & 1);
			m_soundmap_en = (bool)((value >> 5) & 1);
			if (m_soundmap_en) {
				LOG_ERROR("CDROM", "[CDROM] ENABLE SOUNDMAP");
			}

			if (m_want_data) {
				m_want_data = false;

				//Return if:
				//1. There is already loaded data (buffer has not been read)
				//2. There is literally no available data
				//MGS1 uses the following trick:
				//1. Send data request
				//2. Read first 0xC bytes (header+2 subheaders)
				//3. Send another data request
				//4. Read all 0x800 data bytes
				//Second data request should not reset position in buffer
				if ((!m_has_data_to_load && (m_curr_sector_size != m_curr_sector_pos)) || 
					m_curr_sector_size == 0) {
					LOG_WARN("CDROM", "[CDROM] Attempted data request even if no sectors are available");
					return;
				}

				//There's an unread sector, or current sector has been fully
				//read, reset position to beginning
				if(m_has_data_to_load || m_curr_sector_size == m_curr_sector_pos) {
					LOG_INFO("CDROM", "[CDROM] DATA REQUEST");
					m_index_reg.data_fifo_not_empty = true;
					m_has_data_to_load = false;
					m_has_loaded_data = true;
					m_curr_sector_pos = 0;

					auto& dma = m_sys_status->sysbus->GetDMAControl()
						.GetCDROMDma();
					dma.SetDreq(true);
					dma.DreqRisingEdge();
				}
			}
			else {
				m_index_reg.data_fifo_not_empty = false;
				m_has_loaded_data = false;
				m_curr_sector_pos = 0;
			}
			break;
		case 1:
			if ((value >> 6) & 1) {
				LOG_DEBUG("CDROM", "[CDROM] Param FIFO reset");
				m_param_fifo.clear();
			}

			{
				auto ack_int = value & 0x7;
				
				if (ack_int != 0 && u8(m_int_flag.irq) != 0) {
					InterruptAckd();
				}

				m_int_flag.irq = CdInterrupt( u8(m_int_flag.irq) & ~ack_int);
			}
			break;
		case 2:
			m_volume.left_to_right = value;
			break;
		case 3: {
			bool old_mute = m_mute_adpcm;
			m_mute_adpcm = (bool)(value & 1);
			if (!old_mute && m_mute_adpcm)
				LOG_DEBUG("CDROM", "[CDROM] ADPCM Muted!");
			if ((value >> 5) & 1)
				LOG_DEBUG("CDROM", "[CDROM] Audio volume changes \"applied\"!");
		}
			break;
		default:
			error::DebugBreak();
			break;
		}
	}

	u8 CDDrive::ReadReg1() {
		switch (m_index_reg.index)
		{
		case 0:
		case 1:
		case 2:
		case 3: {
			u8 resp = PopResponseByte();
			return resp;
		}
			break;
		default:
			error::DebugBreak();
			break;
		}
		return 0;
	}

#pragma optimize("", off)
	u8 CDDrive::ReadReg2() {
		switch (m_index_reg.index)
		{
		case 0:
		case 1:
		case 2:
		case 3:
			if (!m_has_loaded_data) {
				if (m_curr_sector_size == 0) {
					return 0;
				}

				if (m_curr_sector_size == CDROM::FULL_SECTOR_SIZE) {
					return m_curr_sector[0x920];
				}
				else {
					return m_curr_sector[CDROM::SECTOR_SIZE - 0x8];
				}
			}

			{
				u8 value = m_curr_sector[m_curr_sector_pos++];
				if (m_curr_sector_pos == m_curr_sector_size) {
					m_curr_sector_pos = 0;
					m_has_loaded_data = false;
					m_index_reg.data_fifo_not_empty = false;
				}
				return value;
			}
			break;
		default:
			error::DebugBreak();
			break;
		}
		return 0;
	}
#pragma optimize("", on)

	u8 CDDrive::ReadReg3() {
		switch (m_index_reg.index)
		{
		case 0:
			return u8(m_int_enable.enable_bits) | (0b111 << 5);
			break;
		case 1:
			return m_int_flag.reg | (0b111 << 5);
			break;
		case 2:
			return u8(m_int_enable.enable_bits) | (0b111 << 5);
			break;
		case 3:
			return m_int_flag.reg | (0b111 << 5);
			break;
		default:
			error::DebugBreak();
			break;
		}
		return 0;
	}

	enum DriveCommands {
		GETSTAT = 0x1,
		TEST = 0x19,
		GETID = 0x1A,
		SETMODE = 0xE,
		STOP = 0x8,
		READTOC = 0x1E,
		SETLOC = 0x2,
		SEEKL = 0x15,
		READN = 0x6,
		PAUSE = 0x9,
		INIT = 0xA,
		DEMUTE = 0xC
	};

	void CDDrive::CommandExecute() {
		switch (m_curr_cmd)
		{
		case DriveCommands::GETSTAT:
			Command_GetStat();
			break;
		case DriveCommands::TEST:
			CommandTest(m_curr_cmd);
			break;
		case DriveCommands::GETID:
			Command_GetID();
			break;
		case DriveCommands::SETMODE:
			Command_Setmode();
			break;
		case DriveCommands::STOP:
			Command_Stop();
			break;
		case DriveCommands::READTOC:
			Command_ReadTOC();
			break;
		case DriveCommands::SETLOC:
			Command_SetLoc();
			break;
		case DriveCommands::SEEKL:
			Command_SeekL();
			break;
		case DriveCommands::READN:
			Command_ReadN();
			break;
		case DriveCommands::PAUSE:
			Command_Pause();
			break;
		case DriveCommands::INIT:
			Command_Init();
			break;
		case DriveCommands::DEMUTE:
			Command_Demute();
			break;
		default:
			LOG_ERROR("CDROM", "[CDROM] Unknown/invalid command {:#x}",
				m_curr_cmd);
			error::DebugBreak();
			break;
		}

		m_index_reg.param_fifo_empty = m_param_fifo.empty();
		m_index_reg.param_fifo_not_full = !m_param_fifo.full();
	}

	void CDDrive::RequestInterrupt(CdInterrupt interrupt) {
		m_index_reg.response_fifo_not_empty = !m_response_fifo.empty();
		m_int_flag.irq = interrupt;

		//(HINTMSK & HINTSTS) != 0
		if ((m_int_enable.enable_bits & u8(interrupt)) == 0) {
			//INT disabled
			return;
		}

		m_sys_status->Interrupt(u32(Interrupts::CDROM));
	}

	void CDDrive::PushResponse(CdInterrupt interrupt, std::initializer_list<u8> args, u64 delay) {
		if (m_response_fifo.full()) {
			LOG_DEBUG("CDROM", "[CDROM] Response FIFO is full!");
			error::DebugBreak();
			return;
		}

		ResponseFifo response{};

		for (u8 val : args)
			response.fifo[response.num_bytes++] = val;

		u64 curr_time = m_sys_status->scheduler.GetTimestamp();

		m_response_fifo.queue({
			.fifo = response,
			.interrupt = interrupt,
			.delay = delay,
			.timestamp = curr_time
			});

		//Interrupt requests are performed by function
		//that emulates INT ack., here we request 
		//interrupt only if there is nothing else
		//that would do it

		//If delay is zero, immediately fire INT
		if (m_response_fifo.len() == 1 && delay == 0) {
			RequestInterrupt(interrupt);
			return;
		}

		//Delay is > 0, use scheduler
		if (delay != 0 && m_response_fifo.len() == 1) {
			ScheduleInterrupt(delay);
		}
	}

	bool CDDrive::ValidateParams(u32 num_params) {
		if (num_params != m_param_fifo.len()) {
			m_stat.err = true;
			PushResponse(CdInterrupt::INT5_ERR, { m_stat.reg, u8(CommandError::WRONG_NUM_OF_PARAMS) }, 0);
			return false;
		}

		return true;
	}

	void CDDrive::UpdateIndexRegister() {
		m_index_reg.response_fifo_not_empty = !m_response_fifo.empty();
		m_index_reg.data_fifo_not_empty = (m_curr_sector_pos != m_curr_sector_size) &&
			m_curr_sector_size != 0;
		m_index_reg.param_fifo_empty = m_param_fifo.empty();
		m_index_reg.param_fifo_not_full = !m_param_fifo.full();
	}

	void CDDrive::InterruptAckd() {
		if (m_response_fifo.empty())
			return;

		{
			auto& response = m_response_fifo.peek();
			auto curr_timestamp = m_sys_status->scheduler.GetTimestamp();

			if (response.timestamp + response.delay > curr_timestamp) {
				LOG_WARN("CDROM", "[CDROM] INTERRUPT ACKNOWLEDGE OF UNDELIVERED INTERRUPT");
				return;
			}
		}

		(void)m_response_fifo.deque();

		m_index_reg.response_fifo_not_empty = false;

		if (!m_response_fifo.empty()) {
			auto const& response = m_response_fifo.peek();
			u64 curr_time = m_sys_status->scheduler.GetTimestamp();
			i64 diff = (i64)curr_time - (response.timestamp + response.delay);
			if (response.delay == 0 || diff >= 0) {
				LOG_DEBUG("CDROM", "[CDROM] Immediate INT request {:#x}", 
					(u8)response.interrupt);
				RequestInterrupt(response.interrupt);
			}
			else {
				LOG_DEBUG("CDROM", "[CDROM] Schedule {} clocks starting from now", 
					std::abs(diff));
				ScheduleInterrupt(std::abs(diff));
			}
		}
		else {
			m_idle = true;
			HandlePendingCommand();
		}
	}

	u8 CDDrive::PopResponseByte() {
		if (m_response_fifo.empty())
			return 0;

		auto& response = m_response_fifo.peek();

		u8 return_value = 0x00;

		if (response.fifo.curr_index <
			response.fifo.num_bytes) {
			return_value = response.fifo.fifo[
				response.fifo.curr_index
			];
		}

		response.fifo.curr_index++;

		if (response.fifo.curr_index >=
			response.fifo.num_bytes)
			m_index_reg.response_fifo_not_empty = false;

		response.fifo.curr_index %= 16;

		return return_value;
	}

	void CDDrive::OpenLid() {
		m_lid_open = true;
		m_stat.shell_open = m_lid_open;
		m_stat.seek_err = true;
		m_motor_on = false;
		PushResponse(CdInterrupt::INT5_ERR, 
			{ u8(CommandError::DRIVE_DOOR_OPEN) }, 0);
	}

	void CDDrive::CloseLid() {
		m_lid_open = false;
		m_motor_on = true;
	}

	bool CDDrive::InsertDisc(std::filesystem::path const& path) {
		auto extension = path.extension().string();
		if (extension != ".cue") {
			LOG_ERROR("CDROM", "[CDROM] Unsupported file extension {}",
				extension);
			return false;
		}

		if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
			LOG_ERROR("CDROM", "[CDROM] Invalid path {}",
				path.string());
			return false;
		}

		m_cdrom.reset(new CueBin(path));

		if (!m_cdrom->Init()) {
			m_cdrom.reset(nullptr);
			return false;
		}

		m_lid_open = false;
		m_motor_on = true;

		return true;
	}

	void CDDrive::HandlePendingCommand() {
		if (m_idle && m_has_next_cmd) {
			m_index_reg.transmission_busy = false;
			m_curr_cmd = m_new_cmd;
			m_has_next_cmd = false;
			m_idle = false;
			CommandExecute();
		}
	}

	void event_callback(void* userdata, u64 cycles_late) {
		std::bit_cast<CDDrive*>(userdata)->DeliverInterrupt(cycles_late);
	}

	void read_callback(void* userdata, u64 cycles_late) {
		std::bit_cast<CDDrive*>(userdata)->ReadCallback(cycles_late);
	}

	void CDDrive::ScheduleInterrupt(u64 cycles) {
		m_event_id = m_sys_status->scheduler.Schedule(cycles,
			event_callback, this);
	}

#pragma optimize("", off)
	void CDDrive::ReadCallback(u64 cycles_late) {
		m_stat.seeking = false;
		m_stat.reading = true;

		decltype(m_curr_sector) sector{};

		if (m_mode.read_whole_sector) {
			auto temp_sector = m_cdrom->ReadFullSector(m_seek_loc.mm, m_seek_loc.ss, m_seek_loc.sect);
			SectorMode2Form1* form1 = std::bit_cast<SectorMode2Form1*>(temp_sector.data());
			std::copy_n(std::bit_cast<u8*>(&form1->header), 0x924, sector.data());
		}
		else {
			sector = m_cdrom->ReadSector(m_seek_loc.mm, m_seek_loc.ss, m_seek_loc.sect);
		}

		bool contains_data_response = false;
		for (auto it = m_response_fifo.begin(); it != m_response_fifo.end(); ++it) {
			if (it->interrupt == CdInterrupt::INT1_DATA_RESPONSE) {
				contains_data_response = true;
				break;
			}
		}

		if (m_response_fifo.full() || m_has_pending_read || contains_data_response) {
			m_has_pending_read = true;
			m_pending_sector = sector;
			m_pending_sector_size = m_mode.read_whole_sector ?
				0x924 : CDROM::SECTOR_SIZE;
			error::DebugBreak();
		}
		else {
			PushResponse(CdInterrupt::INT1_DATA_RESPONSE, { m_stat.reg },
				0);
			m_has_data_to_load = true;
			m_curr_sector_size = m_mode.read_whole_sector ?
				0x924 : CDROM::SECTOR_SIZE;
			m_curr_sector = sector;
		}

		u64 read_time = m_mode.double_speed ? ResponseTimings::READ_DOUBLE_SPEED :
			ResponseTimings::READ;

		m_read_event = m_sys_status->scheduler.Schedule( read_time,
			read_callback, std::bit_cast<void*>(this));

		m_seek_loc++;
	}
#pragma optimize("", on)

	std::string const& CDDrive::GetConsoleRegion() const {
		return m_sys_status->sys_conf->console_region;
	}

	void CDDrive::DeliverInterrupt(u64 cycles_late) {
		auto const& response = m_response_fifo.peek();
		RequestInterrupt(response.interrupt);
	}
}