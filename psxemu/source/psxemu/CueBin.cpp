#include <psxemu/include/psxemu/CueBin.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <common/Errors.hpp>
#include <stdexcept>

namespace psx {
	CueBin::CueBin(std::filesystem::path const& path) :
		CDROM(path), m_cue_sheet{}, m_cd_files{} {}

	u64 CueBin::GetTrackNumber(CdLocation loc) const {
		for (size_t curr_index = 0; auto const& track : m_cue_sheet.GetTracks()) {
			if (track.begin > loc) {
				return curr_index - 1;
			}
			curr_index++;
		}
		return m_cue_sheet.GetTracks().size() - 1;
	}

	u64 CueBin::GetTrackNumber(u64 lba) const {
		auto cd_loc = CdLocation::from_lba(lba) + CdLocation(0, 2, 0);
		return GetTrackNumber(cd_loc);
	}

	bool CueBin::Init() {
		if (!m_cue_sheet.ReadCue(m_path)) {
			return false;
		}

		auto const& tracks = m_cue_sheet.GetTracks();
		if (tracks.empty()) {
			LOG_ERROR("CDROM", "[CUE] Malformed .cue");
			return false;
		}

		for (auto const& track : tracks) {
			if (m_cd_files.contains(track.path)) {
				continue;
			}
			m_cd_files[track.path] = std::ifstream{ track.path, std::ios::binary };
			if (!m_cd_files[track.path].is_open()) {
				LOG_ERROR("CDROM", "[CUE] Could not open {}",
					track.path.string());
				return false;
			}
		}

		return true;
	}

	CueBin::~CueBin()
	{}

	std::array<u8, FULL_SECTOR_SIZE> CueBin::ReadSector(CdLocation loc) {
		auto track_index = GetTrackNumber(loc);
		if (m_cue_sheet.GetTracks()[track_index].track_type == CueSheet::TrackType::AUDIO) {
			LOG_ERROR("CDROM", "[CUE] READING TRACK {}, WHICH IS CD-DA, AS CD-XA", track_index);
		}
		auto sect = ReadFullSector(loc);

		std::array<u8, FULL_SECTOR_SIZE> final_sect{};
		SectorMode2Form1* form1 = std::bit_cast<SectorMode2Form1*>(sect.data());

		std::copy_n(form1->data, LOGICAL_SECTOR_SIZE, final_sect.begin());
		return final_sect;
	}

	/*
	((mm * SECONDS_PER_MINUTE)
				* SECTORS_PER_SECOND + 
				(ss * SECTORS_PER_SECOND) + 
				sect) * 0x930;
	*/

	std::array<u8, FULL_SECTOR_SIZE> CueBin::ReadFullSector(CdLocation loc) {
		if (loc.mm == 0 && loc.ss < 2) {
			LOG_ERROR("CDROM", "[CUE] Cannot read sectors 0 and 1");
			error::DebugBreak();
			return {};
		}

		auto track_index = GetTrackNumber(loc);
		auto const& track = m_cue_sheet.GetTracks()[track_index];

		loc = loc - track.begin;
		auto absolute_pos = loc.to_lba();
		absolute_pos += track.file_offset;

		if (absolute_pos >= GetFileSize(track_index)) {
			LOG_ERROR("CDROM", "[CUE] Reading past the end of file of the track {} at mm={}, ss={}, sect={}",
				track_index, loc.mm, loc.ss, loc.sect);
			throw std::out_of_range("Out of bounds file read");
		}

		auto& track_file = m_cd_files[track.path];
		track_file.seekg(absolute_pos, std::ios::beg);

		std::array<u8, FULL_SECTOR_SIZE> sect{};
		track_file.read(std::bit_cast<char*>(sect.data()), FULL_SECTOR_SIZE);
		return sect;
	}

	u64 CueBin::GetFileSize(u64 track_index) const {
		auto const& track = m_cue_sheet.GetTracks()[track_index];
		return (u64)std::filesystem::file_size(track.path);
	}

	CdLocation CueBin::LogicalToPhysical(u64 lba) const {
		auto track_index = GetTrackNumber(lba);
		auto const& tracks = m_cue_sheet.GetTracks();
		switch (tracks[track_index].track_type)
		{
		case CueSheet::TrackType::MODE2_2352: {
			u64 sectors = lba / LOGICAL_SECTOR_SIZE;
			u64 physical_lba = sectors * FULL_SECTOR_SIZE;
			auto loc = CdLocation::from_lba(physical_lba);
			loc.ss += 2;
			return loc;
		} break;
		case CueSheet::TrackType::AUDIO: {
			auto loc = CdLocation::from_lba(lba);
			loc.ss += 2;
			return loc;
		} break;
		default:
			error::DebugBreak();
			break;
		}

		return CdLocation();
	}
}