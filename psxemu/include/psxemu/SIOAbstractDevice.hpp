#pragma once

#include <common/Defs.hpp>

namespace psx {
	enum class SIODeviceType {
		PAD_MEMCARD_DRIVER
	};

	class SIOAbstractDevice {
	public :
		SIOAbstractDevice() {}

		virtual SIODeviceType GetDeviceType() const = 0;
		virtual u8 Send(u8 value, bool& has_data) = 0;
		virtual bool Ack() = 0;
		virtual void Unselect() = 0;

		virtual ~SIOAbstractDevice() {}
	};
}