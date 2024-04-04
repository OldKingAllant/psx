#pragma once

#include <common/Defs.hpp>

namespace psx {
	enum class SyncMode : u8 {
		BURST,
		SLICE,
		LINKED,
		RESERVED
	};

	union CHCR {
#pragma pack(push, 1)
		struct {
			bool transfer_dir : 1;
			bool decrement : 1;
			u8 : 6;
			bool chopping : 1;
			SyncMode sync : 2;
			u8 : 5;
			u8 chopping_dma_window : 3;
			u8 : 1;
			u8 chopping_cpu_window : 3;
			u8 : 1;
			bool start_busy : 1;
			u8 : 3;
			bool force_start : 1;
			bool pause : 1;
		};
#pragma pack(pop)

		u32 raw;
	};

	union BurstBlockControl {
		u16 word_count;
		u32 raw;
	};

	union SliceBlockControl {
#pragma pack(push, 1)
		struct {
			u16 blocksize;
			u16 block_count;
		};
#pragma pack(pop)
		u32 raw;
	};

	static constexpr u32 MADR = 0x0;
	static constexpr u32 BLOCK_CONTROL = 0x4;
	static constexpr u32 CHCR_ADD = 0x8;
}