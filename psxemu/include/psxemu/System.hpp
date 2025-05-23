#pragma once

#include "MIPS1.hpp"
#include "SystemBus.hpp"
#include "SystemStatus.hpp"
#include "Kernel.hpp"
#include "SystemConf.hpp"
#include "Logger.hpp"

#include <string>
#include <span>
#include <optional>
#include <vector>
#include <set>
#include <functional>
#include <memory>

namespace psx {
	struct LoggerInit {
		LoggerInit(std::shared_ptr<SystemConf> const& config);
	};

	class System {
	public :
		System(std::shared_ptr<SystemConf> config);

		~System();

		FORCE_INLINE cpu::MIPS1& GetCPU() { return m_cpu; }
		FORCE_INLINE system_status& GetStatus() { return m_status; }

		using ExecArgs = std::optional<std::span<u8>>;

		/// <summary>
		/// Load and execute psx executable
		/// </summary>
		/// <param name="path">Path to executable</param>
		/// <param name="args">Optional arguments of the executable</param>
		void LoadExe(std::string const& path, ExecArgs args);

		/// <summary>
		/// Load bios file
		/// </summary>
		/// <param name="path">BIOS path</param>
		void LoadBios(std::string const& path);

		/// <summary>
		/// Run with the interpreter
		/// </summary>
		/// <param name="num_instructions">Number of instructions to execute</param>
		void RunInterpreter(u32 num_instructions);

		/// <summary>
		/// Run with the interpreter until some
		/// kind of breakpoint has been hit
		/// (or VBlank)
		/// </summary>
		/// <returns>True if a breakpoint has been hit</returns>
		bool RunInterpreterUntilBreakpoint();

		/// <summary>
		/// Reset status to the reset vector
		/// </summary>
		void ResetVector();

		/// <summary>
		/// Add an hardware breakpoint (no memory
		/// is modified)
		/// </summary>
		/// <param name="address">PC value of the breakpoint</param>
		void AddHardwareBreak(u32 address);

		/// <summary>
		/// Undo what AddHardwareBreak did.
		/// No errors are returned if no 
		/// breakpoint at address is found
		/// </summary>
		/// <param name="address"></param>
		void RemoveHardwareBreak(u32 address);

		/// <summary>
		/// Enables or disables breakpoints
		/// (all types)
		/// </summary>
		/// <param name="enable"></param>
		FORCE_INLINE void ToggleBreakpoints(bool enable) {
			m_break_enable = enable;
		}

		/// <summary>
		/// Enable or disable HLE BIOS emulation
		/// </summary>
		FORCE_INLINE void SetHleEnable(bool enable) {
			m_hle_enable = enable;
		}

		FORCE_INLINE void SetStopped(bool stop) {
			m_stopped = stop;
		}

		FORCE_INLINE bool Stopped() const {
			return m_stopped;
		}

		FORCE_INLINE kernel::Kernel& GetKernel() {
			return m_kernel;
		}

		FORCE_INLINE void SetEnableKernelCallstack(bool en) {
			m_kernel_callstack_enable = en;
		}

		/// <summary>
		/// Returns true if logging is disabled
		/// for the requested syscall
		/// </summary>
		/// <param name="syscall_num"></param>
		/// <returns></returns>
		bool IsSyscallSilent(u32 syscall_num);

		/// <summary>
		/// Prevent spam on console when some
		/// syscalls are used too much 
		/// </summary>
		/// <param name="syscall_num">Syscall to silent</param>
		/// <param name="silent">Enable/disable log</param>
		void SetSyscallSilent(u32 syscall_num, bool silent);

		/// <summary>
		/// Connects a memory card to the selected slot
		/// </summary>
		/// <param name="slot">0 or 1, in which slot the card should be inserted</param>
		/// <param name="path">path to the memory card, creates the file if not exists</param>
		void ConnectCard(u32 slot, std::string const& path);

		/// <summary>
		/// Connects a controller to the selected slot
		/// </summary>
		/// <param name="slot">0 or 1, in which slot the controller should be inserted</param>
		/// <param name="type">Controller type</param>
		void ConnectController(u32 slot, std::string const& type);

		bool InsertDisc(std::filesystem::path const& path);

	private :
		void InterpreterSingleStep();
		void SilenceSyscallsDefault();

		void FollowConfig();

	private :
		LoggerInit m_log_init;
		cpu::MIPS1 m_cpu;
		SystemBus m_sysbus;
		system_status m_status;
		std::vector<u32> m_hbreaks;
		bool m_break_enable;
		bool m_hle_enable;
		bool m_kernel_callstack_enable;

		bool m_stopped;

		kernel::Kernel m_kernel;
		std::set<u32> m_silenced_syscalls;

		std::shared_ptr<SystemConf> m_sys_conf;
	};
}