#include <psxemu/include/psxemu/NullMemcard.hpp>

namespace psx {
	NullMemcard::NullMemcard() {}

	u8 NullMemcard::Send(u8 value) { return 0xFF; }
	bool NullMemcard::Ack() { return false; }
	void NullMemcard::Reset() {}
	bool NullMemcard::LoadFile(std::string const& path) { return false; }

	NullMemcard::~NullMemcard() {}
}