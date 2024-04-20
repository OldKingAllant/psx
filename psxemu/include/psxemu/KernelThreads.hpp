#pragma once

#include <common/Defs.hpp>

namespace psx::kernel {
	struct PCB {
#pragma pack(push, 4)
		u32 curr_tcb;
#pragma pack(pop)
	};

	enum class TCBStatus : u32 {
		FREE = 0x1000,
		USED = 0x4000
	};


	struct ThreadControlBlock {
#pragma pack(push, 4)
		TCBStatus status;
		u32 _u0;
		u32 regs[32];
		u32 epc;
		u32 hi, lo;
		u32 sr;
		u32 cause;
		u32 _u1[9];
#pragma pack(pop)
	};
}