#include <psxemu/include/psxemu/Kernel.hpp>
#include <psxemu/include/psxemu/SyscallTables.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>

#include <iostream>

namespace psx::kernel {
	void Kernel::InsertEnterHook(u32 syscall_id, SyscallHook hook) {
		m_syscall_entry_hooks.insert(std::pair{ syscall_id, hook });
	}

	void Kernel::InsertExitHook(u32 syscall_id, SyscallHook hook) {
		m_syscall_exit_hooks.insert(std::pair{ syscall_id, hook });
	}

	bool Kernel::InsertEnterHook(std::string const& syscall_name, SyscallHook hook) {
		auto ids = GetSyscallIdsByName(syscall_name);
		for(u32 id : ids)
			InsertEnterHook(id, hook);
		return true;
	}

	bool Kernel::InsertExitHook(std::string const& syscall_name, SyscallHook hook) {
		auto ids = GetSyscallIdsByName(syscall_name);
		for (u32 id : ids)
			InsertExitHook(id, hook);
		return true;
	}
	
	bool Kernel::Syscall(u32 pc, u32 id) {
		if (m_enable_hooks && m_syscall_entry_hooks.contains(id)) {
			auto const& hook = m_syscall_entry_hooks[id];
			hook(pc, id);
		}

		if (!m_hle.contains(id))
			return true;

		auto const& hle = m_hle[id];

		bool enter = std::invoke(hle, this);

		if (!enter) {
			if (m_enable_hooks && m_syscall_exit_hooks.contains(id)) {
				auto const& hook = m_syscall_exit_hooks[id];
				hook(pc, id);
			}
		}

		return enter;
	}

	void Kernel::ExitSyscall(u32 id) {
		if (m_enable_hooks && m_syscall_exit_hooks.contains(id)) {
			auto const& hook = m_syscall_exit_hooks[id];
			u32 pc = m_sys_status->cpu->GetPc();
			hook(pc, id);
		}
	}

	void Kernel::InitHle() {}
}