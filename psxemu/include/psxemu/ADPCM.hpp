#pragma once

#include "SPUStructs.hpp"

#include <array>

namespace psx {
	std::array<i16, 28> DecodeADPCMBlock(SPU_ADPCM_Block block, i16& old, i16& older);
	i16 InterpolateSamples(std::array<i16, 3> old, i16 new_sample, u16 index);
	u32 DecodeXAADPCMSector(const u8* sector, i16* left, i16* right);
}