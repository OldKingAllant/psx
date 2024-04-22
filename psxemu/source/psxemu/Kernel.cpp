#include <psxemu/include/psxemu/Kernel.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>

namespace psx::kernel {
	Kernel::Kernel(system_status* status) :
		m_rom_pointer{ nullptr },
		m_ram_pointer{ nullptr },
		m_syscall_entry_hooks{},
		m_syscall_exit_hooks{},
		m_hle{}, m_enable_hooks{}, 
		m_sys_status{status} {}
}