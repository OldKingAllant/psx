#pragma once

#include <common/Macros.hpp>
#include <psxemu/include/psxemu/KernelStructures.hpp>

#include <string>
#include <string_view>
#include <vector>
#include <list>
#include <map>
#include <functional>

namespace psx {
	struct system_status;
}

namespace psx::kernel {
	using ExceptionChain = std::list<ExceptionChainEntry>;

	class Kernel;

	using SyscallHook = std::function<void(u32, u32)>;

	/// <summary>
	/// Member function pointer used for
	/// high level emulation.
	/// Returns true to indicate
	/// that the emulator still needs
	/// to execute the syscall code
	/// </summary>
	using HleHandler = bool(Kernel::*)();

	/// <summary>
	/// For now this is not a real
	/// HLE version of the kernel...
	/// It is simply a collection
	/// of utilities for retrieving
	/// the KERNEL/BIOS status
	/// and informations 
	/// </summary>
	class Kernel {
	public :
		Kernel(system_status* status);

		void SetRomPointer(u8* ptr) { m_rom_pointer = ptr; }
		void SetRamPointer(u8* ptr) { m_ram_pointer = ptr; }

		/// <summary>
		/// [BIOS + 0x108] -> Kernel maker 
		/// Present in the BIOS image as 
		/// pure ASCII. No conversion/allocation
		/// necessary 
		/// </summary>
		/// <returns>String of the kernel maker</returns>
		std::string_view DumpKernelMaker() const;

		/// <summary>
		/// Same principle as the kernel maker
		/// </summary>
		/// <returns>Kernel version string</returns>
		std::string_view DumpKernelVersion() const;

		/// <summary>
		/// This will allocate a new std::string,
		/// since the kernel date is in BCD
		/// format in the BIOS image, and
		/// not in plain ASCII chars
		/// </summary>
		/// <returns></returns>
		std::string DumpKernelBcdDate() const;

		/*
		WARNING:
		All the BIOS structure dump function
		assume that the BIOS/Kernel is already
		initialized (real undefined behaviour
		if not)
		*/

		/// <summary>
		/// For the given priority, dump
		/// the exception priority chain
		/// as string
		/// </summary>
		/// <param name="priority">Wanted priority</param>
		/// <returns>String representation</returns>
		ExceptionChain DumpExceptionPriorityChain(u8 priority) const;

		/// <summary>
		/// As DumpExceptionPriorityChain, 
		/// but all priority chains are dumped
		/// </summary>
		/// <returns></returns>
		std::vector<ExceptionChain> DumpAllExceptionChains() const;

		FORCE_INLINE void SetHooksEnable(bool en) {
			m_enable_hooks = en;
		}

		FORCE_INLINE bool HooksEnabled() const {
			return m_enable_hooks;
		}

		void InsertEnterHook(u32 syscall_id, SyscallHook hook);
		void InsertExitHook(u32 syscall_id, SyscallHook hook);

		bool InsertEnterHook(std::string const& syscall_name, SyscallHook hook);
		bool InsertExitHook(std::string const& syscall_name, SyscallHook hook);

		/// <summary>
		/// Try and handle syscall.
		/// First calls the associated entry
		/// hook (if present), then calls
		/// the handler itself (also if present).
		/// Then, if the handler returns false,
		/// calls also the exit hook
		/// </summary>
		/// <param name="pc">Location of the syscall</param>
		/// <param name="id">ID of the syscall</param>
		/// <returns>True if the emulator should enter the syscall or immediately return to the caller</returns>
		bool Syscall(u32 pc, u32 id);
		
		/// <summary>
		/// Calls associated exit hook. In practice,
		/// understanding when the emulator is
		/// exiting a syscall should be done through
		/// ehuristics, since we cannot really know
		/// (normal syscall do not use processor
		/// exceptions, and waiting for ReturnFromException
		/// will not work)
		/// </summary>
		/// <param name="id">ID of the syscall</param>
		void ExitSyscall(u32 id);


	private :
		void InitHle();

	private :
		u8* m_rom_pointer;
		u8* m_ram_pointer;

		std::map<u32, SyscallHook> m_syscall_entry_hooks;
		std::map<u32, SyscallHook> m_syscall_exit_hooks;
		std::map<u32, HleHandler> m_hle;

		bool m_enable_hooks;

		system_status* m_sys_status;
	};

	std::string FormatExceptionChain(ExceptionChain const& chain);
	std::string FormatExceptionChains(std::vector<ExceptionChain> const& chains);
}