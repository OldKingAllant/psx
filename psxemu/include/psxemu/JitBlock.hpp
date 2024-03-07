#pragma once

#include <common/Defs.hpp>

namespace psx::cpu {
	/// <summary>
	/// Why the blocks ends
	/// after the recorded 
	/// number of instructions
	/// (can be used for opt.)
	/// </summary>
	enum class JitEndType {
		REACHED_BLOCK_MAX_SIZE,
		UNCONDITIONAL_JUMP,
		UNAVOIDABLE_EXCEPTION
	};

	/// <summary>
	/// Function pointer used
	/// to execute a jitted block
	/// </summary>
	using HostCodeFn = u32(__cdecl *)();

	/// <summary>
	/// Maps guest address of an instruction
	/// to the corresponding host address
	/// of the recompiled instruction.
	/// (Since each recompiled instruction
	/// is likely longer than one single
	/// host instruction, the host 
	/// address is in fact the first
	/// opcode that can be associated
	/// with the original instruction)
	/// </summary>
	using GuestHostInstructionPair = std::pair<u64, u64>;

	struct JitBlock {
		u64 guest_base;
		u64 guest_end;
		u64 num_instructions;
		u64 num_recompilations;
		JitEndType end_type;
		HostCodeFn host_fn;
		JitBlock* next;
	};
}