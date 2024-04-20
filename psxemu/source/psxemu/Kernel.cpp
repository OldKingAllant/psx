#include <psxemu/include/psxemu/Kernel.hpp>

namespace psx::kernel {
	Kernel::Kernel() :
		m_rom_pointer{nullptr},
		m_ram_pointer{nullptr} {}
}