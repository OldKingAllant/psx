#pragma once

#include "CDROM.hpp"
#include "CueSheet.hpp"

#include <fstream>

namespace psx {
	class CueBin : public CDROM {
	public :
		CueBin(std::filesystem::path const& path);

		std::array<u8, FULL_SECTOR_SIZE> ReadSector(u64 amm, u64 ass, u64 asect) override;
		std::array<u8, FULL_SECTOR_SIZE> ReadFullSector(u64 amm, u64 ass, u64 asect) override;

		bool Init() override;

		~CueBin() override;

	private :
		CueSheet m_cue_sheet;
		std::ifstream m_cd_file;
	};
}