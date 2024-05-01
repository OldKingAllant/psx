#include <psxemu/include/psxemu/CDDrive.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/Interrupts.hpp>

#include <fmt/format.h>

#include <common/Errors.hpp>

namespace psx {
	CDDrive::CDDrive(system_status* sys_status) :
		m_index_reg{}, m_curr_cmd{}, m_new_cmd{}, 
		m_response_fifo{}, m_param_fifo{}, m_int_enable {}, m_int_flag{},
		m_cmd_start_interrupt{false}, m_want_data{false},
		m_volume{}, m_mute_adpcm{true}, m_sound_coding{},
		m_read_paused{false}, m_motor_on{false},
		m_mode{}, m_stat{}, m_sys_status {sys_status},
		m_idle{ true }, m_has_next_cmd{ false }, m_event_id{INVALID_EVENT} {
		m_index_reg.param_fifo_empty = true;

		//Yes, this does not really make sense
		m_index_reg.param_fifo_full = true;
		m_stat.shell_open = false;
		m_motor_on = true;
	}


	u8 CDDrive::Read8(u32 address) {
		switch (address)
		{
		case 0x00: {
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
			fmt::println("Accessing invalid CDROM register {:#x}", address);
			break;
		}

		return 0x00;
	}

	u16 CDDrive::Read16(u32 address) {
		return (u16)Read8(address) |
			((u16)Read8(address + 1) << 8);
	}

	u32 CDDrive::Read32(u32 address) {
		return (u32)Read16(address) |
			((u32)Read16(address + 2) << 16);
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
			fmt::println("Accessing invalid CDROM register {:#x}", address);
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
			fmt::println("[CDROM] Command {:#x}", value);
			if (m_index_reg.transmission_busy) {
				fmt::println("[CDROM] Command transmission busy!");
				return;
			}

			if (m_idle) {
				m_curr_cmd = value;
				m_idle = false;
				CommandExecute();
				m_index_reg.transmission_busy = false;
			}
			else {
				m_new_cmd = value;
				m_has_next_cmd = true;
				m_index_reg.transmission_busy = true;
			}
			break;
		case 1:
			fmt::println("[CDROM] Write do direct ADPCM");
			break;
		case 2: {
			m_sound_coding.reg = value;
			fmt::println("[CDROM] SOUND CODING = {:#x}", value);
			fmt::println("        Stereo              : {}", (bool)m_sound_coding.stereo);
			fmt::println("        Sample rate 18900Hz : {}", (bool)m_sound_coding.sampler_rate_18900);
			fmt::println("        8 bits per sample   : {}", (bool)m_sound_coding.bits_per_sample_8);
			fmt::println("        Emphasis            : {}", (bool)m_sound_coding.emphasis);
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
			fmt::println("[CDROM] Paramater {:#x}", value);
			if (m_param_fifo.full()) {
				fmt::println("[CDROM] But FIFO is full!");
				return;
			}
			m_param_fifo.queue(value);
			m_index_reg.param_fifo_empty = false;
			
			if (m_param_fifo.full())
				m_index_reg.param_fifo_full = false;
			break;
		case 1:
			m_int_enable.enable_bits = value & 0x1F;
			fmt::println("[CDROM] Interrupt enable {:#x}",
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
			m_cmd_start_interrupt = (bool)((value >> 5) & 1);
			break;
		case 1:
			m_int_flag.reg &= ~value;
			
			if ((value >> 6) & 1) {
				fmt::println("[CDROM] Param FIFO reset");
				m_param_fifo.clear();
			}
			InterruptAckd();
			break;
		case 2:
			m_volume.left_to_right = value;
			break;
		case 3: {
			bool old_mute = m_mute_adpcm;
			m_mute_adpcm = (bool)(value & 1);
			if (!old_mute && m_mute_adpcm)
				fmt::println("[CDROM] ADPCM Muted!");
			if ((value >> 5) & 1)
				fmt::println("[CDROM] Audio volume changes \"applied\"!");
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

	u8 CDDrive::ReadReg2() {
		switch (m_index_reg.index)
		{
		case 0:
		case 1:
		case 2:
		case 3:
			fmt::println("[CDROM] Data FIFO read");
			break;
		default:
			error::DebugBreak();
			break;
		}
		return 0;
	}

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
		GETID = 0x1A
	};

	void CDDrive::CommandExecute() {
		if (m_cmd_start_interrupt) {
			m_int_flag.cmd_start = true;
			m_sys_status->Interrupt(u32(Interrupts::CDROM));
		}

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
		default:
			fmt::println("[CDROM] Unknown/invalid command {:#x}",
				m_curr_cmd);
			error::DebugBreak();
			break;
		}

		m_index_reg.param_fifo_empty = m_param_fifo.empty();
		m_index_reg.param_fifo_full = !m_param_fifo.full();
	}

	void CDDrive::RequestInterrupt(CdInterrupt interrupt) {
		m_index_reg.response_fifo_empty = !m_response_fifo.empty();
		m_int_flag.irq = interrupt;

		if ((m_int_enable.enable_bits & u8(interrupt)) != u8(interrupt)) {
			//INT disabled
			return;
		}

		m_sys_status->Interrupt(u32(Interrupts::CDROM));
	}

	void CDDrive::PushResponse(CdInterrupt interrupt, std::initializer_list<u8> args, u64 delay) {
		if (m_response_fifo.full()) {
			fmt::println("[CDROM] Response FIFO is full!");
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

		if (m_response_fifo.len() == 1 && delay == 0) {
			RequestInterrupt(interrupt);
		}

		if (delay != 0)
			ScheduleInterrupt(delay);
	}

	bool CDDrive::ValidateParams(u32 num_params) {
		if (num_params != m_param_fifo.len()) {
			m_stat.err = true;
			PushResponse(CdInterrupt::INT5_ERR, { m_stat.reg, u8(CommandError::WRONG_NUM_OF_PARAMS) }, 0);
			return false;
		}

		return true;
	}

	void CDDrive::InterruptAckd() {
		if (m_response_fifo.empty())
			return;

		(void)m_response_fifo.deque();

		m_index_reg.response_fifo_empty = false;

		if (!m_response_fifo.empty()) {
			auto const& response = m_response_fifo.peek();
			u64 curr_time = m_sys_status->scheduler.GetTimestamp();
			i64 diff = (i64)curr_time - (response.timestamp + response.delay);
			if (response.delay == 0 || diff >= 0) {
				fmt::println("[CDROM] Immediate INT request {:#x}", 
					(u8)response.interrupt);
				RequestInterrupt(response.interrupt);
			}
			else {
				fmt::println("[CDROM] Schedule {} clocks starting from now", 
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
			m_index_reg.response_fifo_empty = false;

		response.fifo.curr_index %= 16;

		return return_value;
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

	void CDDrive::ScheduleInterrupt(u64 cycles) {
		m_event_id = m_sys_status->scheduler.Schedule(cycles,
			event_callback, this);
	}

	void CDDrive::DeliverInterrupt(u64 cycles_late) {
		auto const& response = m_response_fifo.peek();
		RequestInterrupt(response.interrupt);
	}
}