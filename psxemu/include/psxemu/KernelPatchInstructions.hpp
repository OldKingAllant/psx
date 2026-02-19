#pragma once

#include <common/Defs.hpp>

#include <vector>
#include <unordered_map>
#include <string>

namespace psx::kernel {
	constexpr u32 FAKE_EXE_LOAD_INSTRUCTION = 0xFC0010AD;
	constexpr u32 AFTER_EXE_LOAD_INSTRUCTION = 0xFC0A10AD;
	constexpr u32 NEXT_EVENT_INSTRUCTION =     0xFC0E4E47;
	constexpr u32 GET_VBLANK_COUNT_INSTRUCTION = 0xFC04BA48;

	static const auto PATCH_INSTRUCTIONS = std::vector{
		FAKE_EXE_LOAD_INSTRUCTION,
		AFTER_EXE_LOAD_INSTRUCTION,
		NEXT_EVENT_INSTRUCTION,
		GET_VBLANK_COUNT_INSTRUCTION
	};

	struct PatchPattern {
		std::vector<std::string> pattern;
		std::vector<u8> patch_instructions;
	};

	static const auto PSYQ_VSYNC_PATTERNS = std::unordered_map{
		std::pair{
			std::string("3.5.0"), PatchPattern{
				{
					"e0 ff bd 27",
					"c0 2b 05 00", 
					"10 00 a5 af",
					"?? ?? ?? ??",
					"?? ?? ?? ??",
					"00 00 00 00",
					"2a 10 44 00",
					"?? ?? ?? ??",
					"18 00 bf af",
					"ff ff 03 24",
					"10 00 a2 8f",
					"00 00 00 00",
					"!! !! !! !!",
					"10 00 a2 af",
					"10 00 a2 8f"
				}, 
				{
					NEXT_EVENT_INSTRUCTION & 0xFF, 
					(NEXT_EVENT_INSTRUCTION >> 8) & 0xFF, 
					(NEXT_EVENT_INSTRUCTION >> 16) & 0xFF,
					(NEXT_EVENT_INSTRUCTION >> 24) & 0xFF,
				}
			}
		}
	};
}