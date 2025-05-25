#include <psxemu/include/psxemu/CueBin.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

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

	std::array<u8, CDROM::FULL_SECTOR_SIZE> CueBin::ReadSector(u64 amm, u64 ass, u64 asect) {
		return {};
	}

	std::array<u8, CDROM::FULL_SECTOR_SIZE> CueBin::ReadFullSector(u64 amm, u64 ass, u64 asect) {
		return {};
	}
}