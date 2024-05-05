#include <psxemu/include/psxemu/StandardController.hpp>

#include <common/Errors.hpp>

#include <fmt/format.h>

namespace psx {
	StandardController::StandardController() :
		m_status{ControllerStatus::IDLE}, 
		m_mode{ControllerMode::DIGITAL},
		m_response{}
	{}

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
		m_response.queue(0xFF);
		m_response.queue(0xFF);
	}

	void StandardController::Reset() {
		m_response.clear();
		m_status = ControllerStatus::IDLE;
	}

	StandardController::~StandardController() {}
}