#include <psxemu/include/psxemu/CueBin.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <common/Errors.hpp>

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

#pragma optimize("", off)
	std::array<u8, CDROM::FULL_SECTOR_SIZE> CueBin::ReadSector(u64 amm, u64 ass, u64 asect) {
		if (ass < 2) {
			LOG_ERROR("CDROM", "[CUE] Cannot read sectors 0 and 1");
			return {};
		}

		auto sect = ReadFullSector(amm, ass, asect);

		std::array<u8, CDROM::FULL_SECTOR_SIZE> final_sect{};
		SectorMode2Form1* form1 = std::bit_cast<SectorMode2Form1*>(sect.data());

		std::copy_n(form1->data, CDROM::SECTOR_SIZE, final_sect.begin());
		return final_sect;
	}

	std::array<u8, CDROM::FULL_SECTOR_SIZE> CueBin::ReadFullSector(u64 amm, u64 ass, u64 asect) {
		if (ass < 2) {
			LOG_ERROR("CDROM", "[CUE] Cannot read sectors 0 and 1");
			return {};
		}

		ass -= 2;

		if (m_cue_sheet.GetFiles()[0].tracks.size() != 1) {
			LOG_ERROR("CDROM", "[CUE] UNIMPLEMENTED: Multiple tracks");
			error::DebugBreak();
		}

		CdLocation loc{};
		loc.mm = amm;
		loc.ss = ass;
		loc.sect = asect;

		auto absolute_pos = loc.to_mode2_absolute();

		m_cd_file.seekg(absolute_pos, std::ios::beg);
		std::array<u8, CDROM::FULL_SECTOR_SIZE> sect{};

		m_cd_file.read(std::bit_cast<char*>(sect.data()), 
			CDROM::FULL_SECTOR_SIZE);

		return sect;
	}
#pragma optimize("", on)
}