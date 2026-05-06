#pragma once

#include "CDROM.hpp"
#include "CueSheet.hpp"

#include <fstream>
#include <unordered_map>

namespace psx {
	class CueBin : public CDROM {
	public :
		CueBin(std::filesystem::path const& path);

		u64 GetTrackNumber(CdLocation loc) const override;
		u64 GetTrackNumber(u64 lba) const override;

		std::array<u8, FULL_SECTOR_SIZE> ReadSector(CdLocation loc) override;
		void ReadSector(CdLocation loc, u8* data) override;

		std::array<u8, FULL_SECTOR_SIZE> ReadSectorData(CdLocation loc) override;
		void ReadSectorData(CdLocation loc, u8* data) override;

		Track const& GetTrack(u64 id) const override;

		u64 GetFileSize(u64 track) const override;
		CdLocation LogicalToPhysical(u64 lba) const override;

		bool Init() override;

		~CueBin() override;

	private :
		CueSheet                                                 m_cue_sheet;
		std::unordered_map<std::filesystem::path, std::ifstream> m_cd_files;
	};
}