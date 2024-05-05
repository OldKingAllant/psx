#include <psxemu/include/psxemu/SIOPadMemcardDriver.hpp>

#include <common/Errors.hpp>

#include <fmt/format.h>

namespace psx {
	SIOPadCardDriver::SIOPadCardDriver() 
		: m_selected{SelectedDevice::NONE}, 
		  m_controller{}
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
			fmt::println("[PAD/CARD DRIVER] Memory card not implemented");
			return 0xFF;
		}

		fmt::println("[PAD/CARD DRIVER] UNREACHABLE");
		error::DebugBreak();
		return 0xFF;
	}

	bool SIOPadCardDriver::Ack() {
		bool ack = false;

		if (m_selected == SelectedDevice::PAD) {
			ack = m_controller->Ack();
		}

		if (!ack)
			m_selected = SelectedDevice::NONE;

		return ack;
	}

	void SIOPadCardDriver::Unselect() {
		m_selected = SelectedDevice::NONE;
		m_controller->Reset();
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
			fmt::println("[PAD/CARD DRIVER] Invalid device {:#x}", address);
		}
			break;
		}
	}

	SIOPadCardDriver::~SIOPadCardDriver() {}
}