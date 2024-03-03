#pragma once

#include <cstdint>

namespace psx {
	using u64 = uint64_t;
	using u32 = uint32_t;
	using u16 = uint16_t;
	using u8 = uint8_t;

	namespace memory {
		namespace region_sizes {
			constexpr u64 PSX_MAIN_RAM_SIZE = 2048 * 1024;
			constexpr u64 PSX_EXPANSION1_SIZE = 8912 * 1024;
			constexpr u64 PSX_SCRATCHPAD_PADDED_SIZE = 4096;
			constexpr u64 PSX_IO_SIZE = 8 * 1024;
			constexpr u64 PSX_EXPANSION2_SIZE = 8 * 1024;
			constexpr u64 PSX_EXPANSION3_SIZE = 2048 * 1024;
			constexpr u64 PSX_BIOS_SIZE = 512 * 1024;
			constexpr u64 PSX_CACHE_CONTROL_SIZE = 512;

			constexpr u64 PSX_EFFECTIVE_MEMORY_SIZE =
				PSX_MAIN_RAM_SIZE + PSX_EXPANSION1_SIZE +
				PSX_SCRATCHPAD_PADDED_SIZE + PSX_EXPANSION3_SIZE +
				PSX_BIOS_SIZE;
		}

		constexpr u64 KUSEG_START = 0x00000000;
		constexpr u64 KSEG0_START = 0x80000000;
		constexpr u64 KSEG1_START = 0xA0000000;

		constexpr u64 SEGMENT_SIZE = 0x20000000;
		constexpr u64 SEGMENT_MASK = 0x1FFFFFFF;

		namespace region_offsets {
			constexpr u64 PSX_MAIN_RAM_OFFSET =    0x0;
			constexpr u64 PSX_EXPANSIONS1_OFFSET = 0x0F000000;
			constexpr u64 PSX_SCRATCHPAD_OFFSET =  0x0F800000;
			constexpr u64 PSX_IO_OFFSET =          0x0F801000;
			constexpr u64 PSX_EXPANSION2_OFFSET =  0x0F802000;
			constexpr u64 PSX_EXPANSION3_OFFSET =  0x0FA00000;
			constexpr u64 PSX_BIOS_OFFSET =		   0x0FC00000;
			constexpr u64 PSX_CACHECNT_POSITION =  0xFFFE0000;
		}

		/*
	KUSEG     KSEG0     KSEG1
  00000000h 80000000h A0000000h  2048K  Main RAM (first 64K reserved for BIOS)
  1F000000h 9F000000h BF000000h  8192K  Expansion Region 1 (ROM/RAM)
  1F800000h 9F800000h    --      1K     Scratchpad (D-Cache used as Fast RAM)
  1F801000h 9F801000h BF801000h  8K     I/O Ports
  1F802000h 9F802000h BF802000h  8K     Expansion Region 2 (I/O Ports)
  1FA00000h 9FA00000h BFA00000h  2048K  Expansion Region 3 (whatever purpose)
  1FC00000h 9FC00000h BFC00000h  512K   BIOS ROM (Kernel) (4096K max)
		FFFE0000h (KSEG2)        0.5K   I/O Ports (Cache Control)
		*/

		namespace region_mappings {
			constexpr u64 PSX_MAIN_RAM_MAPPING = 0x0;
			constexpr u64 PSX_EXPANSION1_MAPPING = region_sizes::PSX_MAIN_RAM_SIZE;
			constexpr u64 PSX_SCRATCHPAD_MAPPING = PSX_EXPANSION1_MAPPING + region_sizes::PSX_EXPANSION1_SIZE;
			constexpr u64 PSX_EXPANSION3_MAPPING = PSX_SCRATCHPAD_MAPPING + region_sizes::PSX_SCRATCHPAD_PADDED_SIZE;
			constexpr u64 PSX_BIOS_MAPPING = PSX_EXPANSION3_MAPPING + region_sizes::PSX_EXPANSION3_SIZE;
		}
	}
}