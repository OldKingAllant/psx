#pragma once

#include "MIPS1.hpp"
#include "SystemBus.hpp"
#include "SystemStatus.hpp"

#include <string>
#include <span>
#include <optional>

namespace psx {
	class System {
	public :
		System();

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
		/// Reset status to the reset vector
		/// </summary>
		void ResetVector();

	private :
		cpu::MIPS1 m_cpu;
		SystemBus m_sysbus;
		system_status m_status;
	};
}