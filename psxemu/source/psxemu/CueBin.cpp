#include <psxemu/include/psxemu/CueBin.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <common/Errors.hpp>
#include <stdexcept>

namespace psx {
	CueBin::CueBin(std::filesystem::path const& path) :
		CDROM(path), m_cue_sheet{}, m_cd_file{} {}

	bool CueBin::Init() {
		if (!m_cue_sheet.ReadCue(m_path)) {
			return false;
		}

		auto const& files = m_cue_sheet.GetFiles();
		if (files.size() != 1) {
			LOG_ERROR("CDROM", "[CUE] Unsupported: multi-file disc");
			return false;
		}

		m_cd_file.open(files[0].relative_path, std::ios::binary);
		if (!m_cd_file.is_open()) {
			LOG_ERROR("CDROM", "[CUE] Could not open {}", 
				files[0].relative_path.string());
			return false;
		}

		return true;
	}

	CueBin::~CueBin()
	{}

	std::array<u8, CDROM::FULL_SECTOR_SIZE> CueBin::ReadSector(u64 amm, u64 ass, u64 asect) {
		auto sect = ReadFullSector(amm, ass, asect);

		std::array<u8, CDROM::FULL_SECTOR_SIZE> final_sect{};
		SectorMode2Form1* form1 = std::bit_cast<SectorMode2Form1*>(sect.data());

		std::copy_n(form1->data, CDROM::SECTOR_SIZE, final_sect.begin());
		return final_sect;
	}

	/*
	((mm * SECONDS_PER_MINUTE)
				* SECTORS_PER_SECOND + 
				(ss * SECTORS_PER_SECOND) + 
				sect) * 0x930;
	*/

	std::array<u8, CDROM::FULL_SECTOR_SIZE> CueBin::ReadFullSector(u64 amm, u64 ass, u64 asect) {
		if (amm == 0 && ass < 2) {
			LOG_ERROR("CDROM", "[CUE] Cannot read sectors 0 and 1");
			error::DebugBreak();
			return {};
		}

		if (m_cue_sheet.GetFiles()[0].tracks.size() != 1) {
			LOG_ERROR("CDROM", "[CUE] UNIMPLEMENTED: Multiple tracks");
			error::DebugBreak();
		}

		CdLocation loc{};
		loc.mm = amm;
		loc.ss = ass;
		loc.sect = asect;

		auto absolute_pos = loc.to_mode2_absolute();

		absolute_pos -= 2 * SECTORS_PER_SECOND * 0x930;

		if (absolute_pos >= GetFileSize(0)) {
			LOG_ERROR("CDROM", "[CUE] Reading past the end of file at mm={}, ss={}, sect={}",
				amm, ass, asect);
			throw std::out_of_range("Out of bounds file read");
		}

		m_cd_file.seekg(absolute_pos, std::ios::beg);
		std::array<u8, CDROM::FULL_SECTOR_SIZE> sect{};

		m_cd_file.read(std::bit_cast<char*>(sect.data()), 
			CDROM::FULL_SECTOR_SIZE);

		return sect;
	}

	u64 CueBin::GetFileSize(u64 session) const {
		auto const& files = m_cue_sheet.GetFiles();
		if (session >= files.size()) {
			return (u64)-1;
		}

		return (u64)std::filesystem::file_size(files[session].relative_path);
	}

	CdLocation CueBin::LogicalToPhysical(u64 session, u64 track, 
		u64 lba, u64 block_size) const {
		auto const& files = m_cue_sheet.GetFiles();
		switch (files[session].tracks[track].track_type) {
		case CueSheet::TrackType::MODE2_2352: {
			u64 sectors = lba / block_size;
			u64 phisical_lba = sectors * 0x930;
			auto loc = CdLocation::lba_to_sect(phisical_lba);
			loc.ss += 2;
			return loc;
		}
		default:
			error::DebugBreak();
			break;
		}
		return CdLocation();
	}
}