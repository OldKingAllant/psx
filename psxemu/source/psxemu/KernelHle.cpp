#include <psxemu/include/psxemu/Kernel.hpp>
#include <psxemu/include/psxemu/SyscallTables.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>

#include <iostream>

namespace psx::kernel {
	u64 Kernel::InsertEnterHook(u32 syscall_id, SyscallHook hook) {
		m_syscall_entry_hooks.insert(std::pair{ syscall_id, std::pair{ m_hook_id, hook } });
		return m_hook_id++;
	}

	u64 Kernel::InsertExitHook(u32 syscall_id, SyscallHook hook) {
		m_syscall_exit_hooks.insert(std::pair{ syscall_id, std::pair{ m_hook_id, hook } });
		return m_hook_id++;
	}

	std::optional<u64> Kernel::InsertEnterHook(std::string const& syscall_name, SyscallHook hook) {
		auto ids = GetSyscallIdsByName(syscall_name);

		if (ids.empty()) return std::nullopt;

		for(u32 id : ids)
			m_syscall_entry_hooks.insert(std::pair{ id, std::pair{ m_hook_id, hook } });

		return m_hook_id++;
	}

	std::optional<u64> Kernel::InsertExitHook(std::string const& syscall_name, SyscallHook hook) {
		auto ids = GetSyscallIdsByName(syscall_name);

		if (ids.empty()) return std::nullopt;

		for (u32 id : ids)
			m_syscall_exit_hooks.insert(std::pair{ id, std::pair{ m_hook_id, hook } });

		return m_hook_id++;
	}

	void Kernel::RemoveEnterHook(u64 hook_id) {
		std::erase_if(
			m_syscall_entry_hooks,
			[hook_id](auto const& entry) {
				return entry.second.first ==
					hook_id;
			}
		);
	}

	void Kernel::RemoveExitHook(u64 hook_id) {
		std::erase_if(
			m_syscall_exit_hooks,
			[hook_id](auto const& entry) {
				return entry.second.first ==
					hook_id;
			}
		);
	}
	
	bool Kernel::Syscall(u32 pc, u32 id) {
		if (m_enable_hooks) {
			auto range = m_syscall_entry_hooks.equal_range(id);

			for (auto& it = range.first; it != range.second; it++)
				it->second.second(pc, id);

			CleanupEntryHooks();
		}

		if (!m_hle.contains(id))
			return true;

		auto const& hle = m_hle[id];

		bool enter = std::invoke(hle, this);

		if (!enter) {
			if (m_enable_hooks) {
				auto range = m_syscall_exit_hooks.equal_range(id);

				for (auto& it = range.first; it != range.second; it++)
					it->second.second(pc, id);

				CleanupExitHooks();
			}
		}

		return enter;
	}

	void Kernel::ExitSyscall(u32 id) {
		u32 pc = m_sys_status->cpu->GetPc();

		if (m_enable_hooks) {
			auto range = m_syscall_exit_hooks.equal_range(id);

			for (auto& it = range.first; it != range.second; it++)
				it->second.second(pc, id);

			CleanupExitHooks();
		}
	}

	void Kernel::ScheduleEnterHookRemoval(u64 hook_id) {
		m_entry_hooks_scheduled_for_removal.push_back(hook_id);
	}

	void Kernel::ScheduleExitHookRemoval(u64 hook_id) {
		m_exit_hooks_scheduled_for_removal.push_back(hook_id);
	}

	void Kernel::CleanupEntryHooks() {
		for (u64 id : m_entry_hooks_scheduled_for_removal)
			RemoveEnterHook(id);
		m_entry_hooks_scheduled_for_removal.clear();
	}

	void Kernel::CleanupExitHooks() {
		for (u64 id : m_exit_hooks_scheduled_for_removal)
			RemoveExitHook(id);
		m_exit_hooks_scheduled_for_removal.clear();
	}

	void Kernel::InitHle() {}
}