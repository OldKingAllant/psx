#include <psxemu/include/psxemu/SIOPadMemcardDriver.hpp>

#include <common/Errors.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <fmt/format.h>

namespace psx {
	SIOPadCardDriver::SIOPadCardDriver() 
		: m_selected{SelectedDevice::NONE}, 
		  m_controller{}, m_card{}
	{}

	u8 SIOPadCardDriver::Send(u8 value, bool& has_data) {
		has_data = false;

		if (m_selected == SelectedDevice::NONE) {
			SelectDevice(value);
			return 0xFF;
		}

		if (m_selected == SelectedDevice::PAD) {
			has_data = true;
			return m_controller->Send(value);
		}
		else if(m_selected == SelectedDevice::MEMCARD) {
			has_data = true;
			return m_card->Send(value);
		}

		LOG_ERROR("PAD_CARD", "[PAD/CARD DRIVER] UNREACHABLE");
		return 0xFF;
	}

	bool SIOPadCardDriver::Ack() {
		bool ack = false;

		if (m_selected == SelectedDevice::PAD) {
			ack = m_controller->Ack();
		}
		else if(m_selected == SelectedDevice::MEMCARD) {
			ack = m_card->Ack();
		}

		if (!ack)
			m_selected = SelectedDevice::NONE;

		return ack;
	}

	void SIOPadCardDriver::Unselect() {
		m_selected = SelectedDevice::NONE;
		m_controller->Reset();
		m_card->Reset();
	}

	void SIOPadCardDriver::SelectDevice(u8 address) {
		auto dev_id = SelectedDevice(address);

		switch (dev_id)
		{
		case psx::SelectedDevice::NONE:
		case psx::SelectedDevice::PAD:
		case psx::SelectedDevice::MEMCARD:
			m_selected = dev_id;
			break;
		default: {
			LOG_ERROR("PAD_CARD", "[PAD/CARD DRIVER] Invalid device {:#x}", address);
		}
			break;
		}
	}

	SIOPadCardDriver::~SIOPadCardDriver() {}
}