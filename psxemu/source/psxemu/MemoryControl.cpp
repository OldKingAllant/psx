#include <psxemu/include/psxemu/SystemBus.hpp>

namespace psx {
	void SystemBus::WriteMemControl(u32 address, u32 value) {
		if (address >= memory::IO::BIOS_CONFIG_CONTROL && address < 
			memory::IO::BIOS_CONFIG_CONTROL + 4) {
			ReconfigureBIOS(value);
			return;
		}

#ifdef DEBUG_IO
		fmt::println("Write to invalid/unused/unimplemented mem control 0x{:x}", address);
#endif // DEBUG_IO
	}
}