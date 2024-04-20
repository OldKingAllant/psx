#pragma once

#include <common/Defs.hpp>

namespace psx::kernel {
	struct ExceptionControlBlock {
#pragma pack(push, 4)
		u32 pointer;
		u32 _unused;
#pragma pack(pop)
	};

	struct ExceptionChainEntry {
#pragma pack(push, 4)
		u32 next;
		u32 second_function;
		u32 first_function;
		u32 _u0;
#pragma pack(pop)
	};

	static constexpr u32 ExCB_SIZE = sizeof(
		ExceptionControlBlock
	);
}