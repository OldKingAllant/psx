#pragma once

#include <common/Defs.hpp>

#include <array>

namespace psx::memory::IO {
	static constexpr u32 LOCKED_GAPS[][2] = {
		{ 0x1F801024, 0x1C }, 
		{ 0x1F801064, 0xC },
		{ 0x1F801078, 0x8 },
		{ 0x1F801140, 0x6C0 },
		{ 0x1F801804, 0xC }, 
		{ 0x1F801818, 0x8 },
		{ 0x1F801828, 0x3D8 }
	};

	static constexpr std::array<bool, region_sizes::PSX_IO_SIZE> LOCKED = []() {
		std::array<bool, region_sizes::PSX_IO_SIZE> table{};

		u32 MASK = region_sizes::PSX_IO_SIZE - 1;

		for (u32 index = 0; index < table.size(); index++) {
			bool exit = false;
			u32 gap_index = 0;
			
			while (gap_index < 7 && !exit) {
				u32 base = LOCKED_GAPS[gap_index][0] & MASK;
				u32 end = base + LOCKED_GAPS[gap_index][1];

				if (index >= base && index < end) {
					table[index] = true;
					exit = true;
				}
				gap_index++;
			}
		}

		return table;
	}();
}