#include <psxemu/include/psxemu/StandardController.hpp>

#include <common/Errors.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <array>
#include <algorithm>

namespace psx {
	StandardController::StandardController() :
		m_status{ControllerStatus::IDLE}, 
		m_conf_status{ConfigStatus::JUST_ENTERED},
		m_mode{ControllerMode::DIGITAL},
		m_response{}, m_in_fifo{},
		m_btn_status{}, m_led_state{},
		m_curr_rumble_protocol{},
		m_btn_map{}
	{
		m_btn_status.reg = 0xFFFF;

		std::array btn_names = {
			"SELECT",
			"L3",
			"R3",
			"START",
			"DPAD-UP",
			"DPAD-RIGHT",
			"DPAD-DOWN",
			"DPAD-LEFT",
			"L2",
			"R2",
			"L1",
			"R1",
			"TRIANGLE",
			"CIRCLE",
			"CROSS", 
			"SQUARE"
		};

		for (u32 idx = 0; auto const& name : btn_names) {
			m_btn_map.insert({ name, idx });
			idx++;
		}

		std::fill_n(m_curr_rumble_protocol.begin(), m_curr_rumble_protocol.size(), 0xFF);
	}

#pragma optimize("", off)
	u8 StandardController::Send(u8 value) {
		switch (m_status)
		{
		case ControllerStatus::IDLE:
			IdleProcessByte(value);
			break;
		case ControllerStatus::READ_STAT:
			break;
		case ControllerStatus::CONFIG:
			ConfigProcessByte(value);
			break;
		case ControllerStatus::COMMAND_END:
			m_status = ControllerStatus::IDLE;
			break;
		default:
			error::Unreachable();
			break;
		}

		u8 resp = 0xff;
		if (!m_response.empty()) {
			resp = m_response.deque();
		}

		if (m_response.empty() && m_status == ControllerStatus::READ_STAT)
			m_status = ControllerStatus::COMMAND_END;

		return resp;
	}

	bool StandardController::Ack() {
		/*switch (m_status)
		{
		case psx::StandardController::ControllerStatus::IDLE:
		case psx::StandardController::ControllerStatus::READ_STAT:
			return true;
			break;
		default:
			break;
		}*/

		bool ack = m_status == ControllerStatus::IDLE || 
			(m_status == ControllerStatus::CONFIG && m_conf_status == ConfigStatus::CONFIG_IDLE) || 
			!m_response.empty();

		if (m_status == ControllerStatus::COMMAND_END) {
			m_status = ControllerStatus::IDLE;
		}
		else if (m_status == ControllerStatus::CONFIG && m_conf_status == ConfigStatus::CONFIG_COMMAND_END) {
			m_conf_status = ConfigStatus::CONFIG_IDLE;
		}

		return ack;
	}

	void StandardController::EnqueueStatus(bool force_analog) {
		if (ControllerMode::ANALOG == m_mode || force_analog) {
			m_response.queue(u8(ANALOG_ID));
			m_response.queue(u8(ANALOG_ID >> 8));
		}
		else {
			m_response.queue(u8(DIGITAL_ID));
			m_response.queue(u8(DIGITAL_ID >> 8));
		}

		m_response.queue(u8(m_btn_status.reg & 0xFF));
		m_response.queue(u8((m_btn_status.reg >> 8) & 0xFF));

		if (ControllerMode::ANALOG == m_mode || force_analog) {
			m_response.queue(0x00);
			m_response.queue(0x00);
			m_response.queue(0x00);
			m_response.queue(0x00);
		}
	}

	void StandardController::IdleProcessByte(u8 value) {
		auto cmd = Command(value);
		switch (cmd)
		{
		case Command::READ:
			m_status = ControllerStatus::READ_STAT;
			EnqueueStatus(false);
			break;
		case Command::CONFIG_MODE:
			m_status = ControllerStatus::CONFIG;
			EnqueueStatus(false);
			m_conf_status = ConfigStatus::JUST_ENTERED;
			break;
		default:
			LOG_ERROR("PAD", "[PAD] UNKNOWN COMMAND {:#x}", value);
			error::DebugBreak();
			break;
		}
	}

	void StandardController::ConfigProcessByte(u8 value) {
		switch (m_conf_status)
		{
		case ConfigStatus::JUST_ENTERED: {
			m_in_fifo.queue(value);
			if (m_in_fifo.len() == (u64)GetTransferLength() - 2) {
				m_in_fifo.deque();
				auto enter_exit = ConfigModeEnterExit(m_in_fifo.deque());
				m_in_fifo.clear();
				if (enter_exit == ConfigModeEnterExit::STAY) {
					m_status = ControllerStatus::COMMAND_END;
					return;
				}
				else {
					m_conf_status = ConfigStatus::CONFIG_COMMAND_END;
				}
			}
		} break;
		case ConfigStatus::CONFIG_IDLE: {
			if (m_in_fifo.empty()) {
				ConfigIdleProcessByte(value);
			}
			else {
				m_in_fifo.queue(value);
				if (m_in_fifo.len() == 3) {
					auto cmd = Command(m_in_fifo.peek());
					auto param = m_in_fifo.back();
					switch (cmd)
					{
					case Command::GET_VARIABLE_RESPONSE_A:
						/*
						 Send  01h 46h 00h | ii  00h 00h 00h 00h 00h
						 Reply Hiz F3h 5Ah | 00h 00h cc  dd  ee  ff
						*/
						m_response.queue(0x0);
						m_response.queue(0x0);
						/*
						When ii=00h --> returns cc,dd,ee,ff = 01h,02h,00h,0ah
						When ii=01h --> returns cc,dd,ee,ff = 01h,01h,01h,14h
						Otherwise --> returns cc,dd,ee,ff = all zeroes
						*/
						switch (param)
						{
						case 0x0:
							m_response.queue(0x1);
							m_response.queue(0x2);
							m_response.queue(0x0);
							m_response.queue(0xA);
							break;
						case 0x1:
							m_response.queue(0x1);
							m_response.queue(0x1);
							m_response.queue(0x1);
							m_response.queue(0x14);
							break;
						default:
							m_response.queue(0x0);
							m_response.queue(0x0);
							m_response.queue(0x0);
							m_response.queue(0x0);
							break;
						}
						break;
					case Command::GET_VARIABLE_RESPONSE_B:
						/*
						Send  01h 4Ch 00h ii  00h 00h 00h 00h 00h
						Reply Hiz F3h 5Ah 00h 00h 00h dd  00h 00h
						*/
						/*
						When ii=00h --> returns dd=04h.
						When ii=01h --> returns dd=07h.
						Otherwise --> returns dd=00h.
						*/
						m_response.queue(0x0);
						m_response.queue(0x0);

						switch (param)
						{
						case 0x0:
							m_response.queue(0x04);
							break;
						case 0x1:
							m_response.queue(0x07);
							break;
						default:
							m_response.queue(0x0);
							break;
						}

						m_response.queue(0x0);
						m_response.queue(0x0);
						m_response.queue(0x0);
						break;
					default:
						break;
					}
				}
				if (m_in_fifo.len() == CONFIG_MODE_TRANSFER_LEN - 1) {
					ConfigCommandEnd();
				}
			}
		} break;
		default:
			LOG_ERROR("PAD", "[PAD] UNREACHABLE CODE: UNHANDLED CONFIG STATUS");
			error::Unreachable();
			break;
		}
	}

	void StandardController::ConfigIdleProcessByte(u8 value) {
		auto cmd = Command(value);
		switch (cmd)
		{
		case Command::SET_LET_STATE:
			error::DebugBreak();
			break;
		case Command::GET_LED_STATE:
			/*
			Send  01h 45h 00h 00h 00h 00h 00h 00h 00h
			Reply HiZ F3h 5Ah Typ 02h Led 02h 01h 00h
			*/
			m_response.queue(u8(CONFIG_ID));
			m_response.queue(u8(CONFIG_ID >> 8));
			m_response.queue(u8(CONTROLLER_TYPE_ANALOG));
			m_response.queue(0x2);
			m_response.queue(u8(m_led_state));
			m_response.queue(0x2);
			m_response.queue(0x1);
			m_response.queue(0x0);
			break;
		case Command::GET_VARIABLE_RESPONSE_A:
		case Command::GET_VARIABLE_RESPONSE_B:
			m_response.queue(u8(CONFIG_ID));
			m_response.queue(u8(CONFIG_ID >> 8));
			break;
		case Command::GET_WHATEVER:
			/*
			Send  01h 47h 00h 00h 00h 00h 00h 00h 00h
			Reply HiZ F3h 5Ah 00h 00h 02h 00h 01h 00h
			*/
			m_response.queue(u8(CONFIG_ID));
			m_response.queue(u8(CONFIG_ID >> 8));
			m_response.queue(0x0);
			m_response.queue(0x0);
			m_response.queue(0x2);
			m_response.queue(0x0);
			m_response.queue(0x1);
			m_response.queue(0x0);
			break;
		case Command::CONFIG_MODE:
			m_response.queue(u8(CONFIG_ID));
			m_response.queue(u8(CONFIG_ID >> 8));
			m_response.queue(0x0);
			m_response.queue(0x0);
			m_response.queue(0x0);
			m_response.queue(0x0);
			m_response.queue(0x0);
			m_response.queue(0x0);
			break;
		case Command::GET_SET_RUMBLE_PROTOCOL:
			m_response.queue(u8(CONFIG_ID));
			m_response.queue(u8(CONFIG_ID >> 8));
			m_response.queue(m_curr_rumble_protocol[0]);
			m_response.queue(m_curr_rumble_protocol[1]);
			m_response.queue(m_curr_rumble_protocol[2]);
			m_response.queue(m_curr_rumble_protocol[3]);
			m_response.queue(m_curr_rumble_protocol[4]);
			m_response.queue(m_curr_rumble_protocol[5]);
			break;
		default:
			LOG_ERROR("PAD", "[PAD] UNREACHABLE CODE: INVALID/UNIMPLEMENTED COMMAND: {:#04x}", value);
			error::Unreachable();
			break;
		}

		m_in_fifo.queue(value);
	}

	void StandardController::ConfigCommandEnd() {
		auto cmd = Command(m_in_fifo.deque());
		switch (cmd)
		{
		case Command::SET_LET_STATE:
			error::DebugBreak();
			break;
		case Command::GET_LED_STATE:
		case Command::GET_VARIABLE_RESPONSE_A:
		case Command::GET_WHATEVER:
		case Command::GET_VARIABLE_RESPONSE_B:
			//nothing to do here
			break;
		case Command::CONFIG_MODE: {
			m_in_fifo.deque(); //drop additional byte
			auto enter_exit = ConfigModeEnterExit(~m_in_fifo.deque() & 1);
			if (enter_exit == ConfigModeEnterExit::STAY) {
				m_conf_status = ConfigStatus::CONFIG_COMMAND_END;
			}
			else {
				m_conf_status = ConfigStatus::JUST_ENTERED;
				m_status = ControllerStatus::COMMAND_END;
			}
		} break;
		case Command::GET_SET_RUMBLE_PROTOCOL: {
			m_in_fifo.deque();
			m_curr_rumble_protocol[0] = m_in_fifo.deque();
			m_curr_rumble_protocol[1] = m_in_fifo.deque();
			m_curr_rumble_protocol[2] = m_in_fifo.deque();
			m_curr_rumble_protocol[3] = m_in_fifo.deque();
			m_curr_rumble_protocol[4] = m_in_fifo.deque();
			m_curr_rumble_protocol[5] = m_in_fifo.deque();
		} break;
		default:
			LOG_ERROR("PAD", "[PAD] UNREACHABLE CODE: COMMAND END NOT IMPLEMENTED: {:#04x}", u8(cmd));
			error::Unreachable();
			break;
		}

		m_in_fifo.clear();
		if (cmd != Command::CONFIG_MODE) {
			m_conf_status = ConfigStatus::CONFIG_COMMAND_END;
		}
	}

	void StandardController::Reset() {
		m_response.clear();
		m_status = m_status == ControllerStatus::CONFIG ? m_status : ControllerStatus::IDLE;
	}

	void StandardController::UpdateStatus(ButtonStatus status) {
		if (m_mode == ControllerMode::ANALOG && status.ty != ButtonType::NORMAL)
			return;

		auto name_as_str{ std::string(status.name) };

		if (m_btn_map.contains(name_as_str)) {
			auto btn_index = m_btn_map[name_as_str];
			m_btn_status.reg &= ~(1 << btn_index);
			m_btn_status.reg |= (u16(!status.normal.pressed) << btn_index);
		}
	}

	u32 StandardController::GetTransferLength() const {
		return m_mode == ControllerMode::ANALOG ? 9 : 5;
	}

	StandardController::~StandardController() {}
#pragma optimize("", on)
}