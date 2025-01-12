#include <psxemu/include/psxemu/StandardController.hpp>

#include <common/Errors.hpp>

#include <fmt/format.h>
#include <array>

namespace psx {
	StandardController::StandardController() :
		m_status{ControllerStatus::IDLE}, 
		m_mode{ControllerMode::DIGITAL},
		m_response{}, m_btn_status{},
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
	}

	u8 StandardController::Send(u8 value) {
		if (m_status == ControllerStatus::IDLE) {
			if (value != 0x42) {
				fmt::println("[PAD] UNKNOWN COMMAND {:#x}", value);
				error::DebugBreak();
			}

			m_status = ControllerStatus::READ_STAT;
			EnqueueStatus();
		}

		if (m_response.empty()) {
			fmt::println("[PAD] Response queue is empty!");
			error::DebugBreak();
		}

		u8 resp = m_response.deque();

		if (m_response.empty())
			m_status = ControllerStatus::COMMAND_END;

		return resp;
	}

	bool StandardController::Ack() {
		switch (m_status)
		{
		case psx::StandardController::ControllerStatus::IDLE:
		case psx::StandardController::ControllerStatus::READ_STAT:
			return true;
			break;
		default:
			break;
		}

		if (m_status == ControllerStatus::COMMAND_END) {
			m_status = ControllerStatus::IDLE;
		}

		return false;
	}

	void StandardController::EnqueueStatus() {
		if (ControllerMode::ANALOG == m_mode) {
			fmt::println("[PAD] Analog mode not implemented!");
			error::DebugBreak();
		}

		m_response.queue(u8(DIGITAL_ID));
		m_response.queue(u8(DIGITAL_ID >> 8));
		m_response.queue(u8(m_btn_status.reg & 0xFF));
		m_response.queue(u8((m_btn_status.reg >> 8) & 0xFF));
	}

	void StandardController::Reset() {
		m_response.clear();
		m_status = ControllerStatus::IDLE;
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