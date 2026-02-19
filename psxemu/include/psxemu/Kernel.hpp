#pragma once

#include <common/Macros.hpp>
#include <psxemu/include/psxemu/KernelStructures.hpp>
#include <psxemu/include/psxemu/CDFilesystem.hpp>
#include <psxemu/include/psxemu/MCFilesystem.hpp>
#include <psxemu/include/psxemu/VirtualAddress.hpp>

#include <string>
#include <string_view>
#include <vector>
#include <list>
#include <unordered_map>
#include <functional>
#include <optional>
#include <span>

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

		void ComputeHash();

		std::optional<std::string> ComputeMemoryHash(u32 start, u32 len);

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

		/// <summary>
		/// Dump all EvCB as an high level view
		/// </summary>
		/// <returns>All ACTIVE control blocks</returns>
		std::vector<EventControlBlock> DumpEventControlBlocks() const;

		/// <summary>
		/// Only one PCB = only
		/// one active thread
		/// </summary>
		/// <returns>ID of current thread</returns>
		u32 GetCurrentThread() const;

		/// <summary>
		/// Dump all TCBs
		/// </summary>
		/// <returns>All ACTIVE TCBs</returns>
		std::vector<ThreadControlBlock> DumpThreadControlBlocks() const;

		/// <summary>
		/// Dump alld DCBs
		/// </summary>
		/// <returns>All ACTIVE DCBs</returns>
		std::vector<DeviceControlBlock> DumpDeviceControlBlocks() const;


		FORCE_INLINE void SetHooksEnable(bool en) {
			m_enable_hooks = en;
		}

		FORCE_INLINE bool HooksEnabled() const {
			return m_enable_hooks;
		}

		/*
		Hook functions return the ID of the
		hook (which is simply an incrementing
		positive number)
		*/

		u64 InsertEnterHook(u32 syscall_id, SyscallHook hook);
		u64 InsertExitHook(u32 syscall_id, SyscallHook hook);

		std::optional<u64> InsertEnterHook(std::string const& syscall_name, SyscallHook hook);
		std::optional<u64> InsertExitHook(std::string const& syscall_name, SyscallHook hook);

		void RemoveEnterHook(u64 hook_id);
		void RemoveExitHook(u64 hook_id);

		/*
		Use these methods for auto-removing
		hooks. Using the normal remove
		methods will result in invalid 
		iterators during hook invocations
		*/

		void ScheduleEnterHookRemoval(u64 hook_id);
		void ScheduleExitHookRemoval(u64 hook_id);

		void CleanupEntryHooks();
		void CleanupExitHooks();

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

		enum class PatchError {
			UNKOWN_BIOS,
			UNKOWN_ADDRESS,
			MMAP_FAILED,
			INVALID_FORMAT
		};

		std::optional<PatchError> PatchInstruction(std::unordered_map<u64, u32> const& bios_version_map, u32 instruction);
		std::optional<PatchError> PatchInstruction(u32 address, u32 instruction);
		std::optional<PatchError> ApplyPatch(std::vector<std::string> pattern, std::vector<u8> const& values);

		/////////////////////////////
		void PatchLoad();
		////////////////////////////

		bool UndefinedInstruction(u32 instruction);

		////////////////////////////
		bool FakeExeLoad();
		bool AfterExeLoad();
		bool NextEventInstruction();
		bool GetVblankCountInstruction(u32 orig_instruction);
		////////////////////////////

		bool LoadExe(std::string const& path, std::optional<std::span<char>> args, u32 path_ptr, u32 headerbuf, bool force_run);
		void LoadTest(VirtualAddress path_ptr, VirtualAddress headerbuf);
		void Load(VirtualAddress path_ptr, VirtualAddress headerbuf);
		void FlushCache();

		FORCE_INLINE CdromFs& GetCdFs() {
			return m_cd_fs;
		}

		FORCE_INLINE MCFs& GetMC0Fs() {
			return m_mc0_fs;
		}

		FORCE_INLINE MCFs& GetMC1Fs() {
			return m_mc1_fs;
		}

		std::optional<std::shared_ptr<HLEFsEntry>> GetFilesystemEntry(std::string path);
		std::optional<std::vector<u8>> ReadFileFromPath(std::string path);
		std::optional<std::vector<u8>> ReadFileFromEntry(std::shared_ptr<HLEFsEntry> entry);
		std::optional<std::vector<u8>> ReadFileFromEntry(std::shared_ptr<HLEFsEntry> entry, u32 off, u32 len);

		static std::optional<std::string> DecodeShiftJIS(std::span<char> data);

	private :
		void InitHle();

	private :
		u8* m_rom_pointer;
		u8* m_ram_pointer;

		std::unordered_multimap<u32, std::pair<u64, SyscallHook>> m_syscall_entry_hooks;
		std::unordered_multimap<u32, std::pair<u64, SyscallHook>> m_syscall_exit_hooks;
		std::unordered_map<u32, HleHandler> m_hle;

		bool m_enable_hooks;

		system_status* m_sys_status;

		u64 m_hook_id;

		std::list<u64> m_entry_hooks_scheduled_for_removal;
		std::list<u64> m_exit_hooks_scheduled_for_removal;

		std::string m_bios_hash;
		std::optional<u64> m_bios_version;

		std::unordered_map<u32, u32> m_patched_values;

		CdromFs m_cd_fs;
		MCFs m_mc0_fs;
		MCFs m_mc1_fs;
	};

	std::string FormatExceptionChain(ExceptionChain const& chain);
	std::string FormatExceptionChains(std::vector<ExceptionChain> const& chains);
	std::string_view EventClassName(EventClass evclass);
	std::string_view EventStatusName(EventStatus status);
	std::string_view EventModeName(EventMode mode);
	std::string_view EventSpecName(EventSpec spec);
}