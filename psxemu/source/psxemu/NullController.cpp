#include <psxemu/include/psxemu/NullController.hpp>

namespace psx {
	NullController::NullController() {}

	u8 NullController::Send(u8 value) { return 0xFF;  }
	bool NullController::Ack() { return false; }
	void NullController::Reset() {}

	NullController::~NullController() {}
}