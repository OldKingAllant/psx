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