#pragma once

#include <psxemu/include/psxemu/SystemStatus.hpp>

#include <array>

namespace psx::cpu {
	using InstructionHandler = void(*)(system_status*, u32);

	/// <summary>
	/// Statically initialized (at compile time)
	/// </summary>
	extern std::array<InstructionHandler, 4096> MIPS_HANDLERS;

	/// <summary>
	/// Interpret single MIPS instruction. No synchronization
	/// is performed with the other hardware components
	/// </summary>
	/// <param name="system">System context where effects are applied</param>
	/// <param name="instruction">Instruction to interpret</param>
	void InterpretMips(system_status* system, u32 instruction);
}