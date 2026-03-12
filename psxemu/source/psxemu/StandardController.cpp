#include <psxemu/include/psxemu/StandardController.hpp>

#include <common/Errors.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <array>

namespace psx {
	StandardController::StandardController() :
		m_status{ControllerStatus::IDLE}, 
		m_mode{ControllerMode::DIGITAL},
		m_response{}, m_in_fifo{},
		m_btn_status {}, m_btn_map{}, m_byte_count{}
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
	}

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
			m_response.deque();
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

		bool ack = m_status == ControllerStatus::IDLE || !m_response.empty();

		if (m_status == ControllerStatus::COMMAND_END) {
			m_status = ControllerStatus::IDLE;
		}

		return ack;
	}

	void StandardController::EnqueueStatus() {
		if (ControllerMode::ANALOG == m_mode) {
			LOG_ERROR("PAD", "[PAD] Analog mode not implemented!");
			error::DebugBreak();
		}

		m_response.queue(u8(DIGITAL_ID));
		m_response.queue(u8(DIGITAL_ID >> 8));
		m_response.queue(u8(m_btn_status.reg & 0xFF));
		m_response.queue(u8((m_btn_status.reg >> 8) & 0xFF));
	}

	void StandardController::IdleProcessByte(u8 value) {
		auto cmd = Command(value);
		switch (cmd)
		{
		case Command::READ:
			m_status = ControllerStatus::READ_STAT;
			EnqueueStatus();
			break;
		case Command::CONFIG_MODE:
			m_status = ControllerStatus::CONFIG;
			EnqueueStatus();
			break;
		default:
			LOG_ERROR("PAD", "[PAD] UNKNOWN COMMAND {:#x}", value);
			error::DebugBreak();
			break;
		}
	}

	void StandardController::ConfigProcessByte(u8 value) {
	}

	void StandardController::Reset() {
		m_response.clear();
		m_status = m_status == ControllerStatus::CONFIG ? m_status : ControllerStatus::IDLE;
		m_byte_count = 0;
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

	StandardController::~StandardController() {}
}