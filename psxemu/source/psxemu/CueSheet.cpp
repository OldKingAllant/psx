#include <psxemu/include/psxemu/CueSheet.hpp>

#include <fstream>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

namespace psx {
	CueSheet::CueSheet() : 
		m_tracks{}
	{}

	bool CueSheet::ReadCue(std::filesystem::path const& path) {
		std::ifstream file{ path };

		if (!file.is_open()) {
			LOG_ERROR("CDROM", "[CDROM] Could not open file");
			return false;
		}

		std::vector<std::string> lines{};
		lines.reserve(20);

		std::string curr_line{};
		while (std::getline(file, curr_line)) {
			auto skip_leading_spaces = curr_line.find_first_not_of(' ');
			auto stripped = curr_line.substr(skip_leading_spaces);
			lines.push_back(stripped);
		}

		auto iter = lines.begin();
		while (iter != lines.end()) {
			if (iter->contains("REM") ||
				iter->contains("PERFORMER") ||
				iter->contains("TITLE")) {
				iter++;
			}
			if (!ReadFileEntry(path, lines.begin(), iter, 
				lines.end())) {
				return false;
			}
		}
		return true;
	}

	bool CueSheet::ReadFileEntry(std::filesystem::path const& cue_path, 
		std::vector<std::string>::iterator base,
		std::vector<std::string>::iterator& iter,
		std::vector<std::string>::iterator end) {
		auto curr_line = std::distance(base, iter);

		auto space_pos = iter->find_first_of(' ');
		auto entry = iter->substr(0, space_pos);

		if (entry != "FILE") {
			LOG_ERROR("CDROM", "[CUE] Unexpected entry {} on line {}, expected FILE",
				entry, curr_line);
			return false;
		}

		auto path_begin = iter->find_first_of('\"');
		auto path_end = iter->find_last_of('\"');

		auto distance = path_end - path_begin;

		if (distance <= 1) {
			LOG_ERROR("CDROM", "[CUE] Missing filepath on line {}",
				curr_line);
			return false;
		}

		auto path = iter->substr(path_begin + 1, distance - 1);

		auto dir_name = cue_path.parent_path();
		auto complete_path = dir_name / path;

		if (!std::filesystem::exists(complete_path) ||
			!std::filesystem::is_regular_file(complete_path)) {
			LOG_ERROR("CDROM", "[CUE] Invalid file {} on line {}",
				path, curr_line);
			return false;
		}

		auto format = iter->substr(path_end + 2);

		if (format != "BINARY") {
			LOG_ERROR("CDROM", "[CUE] Unexpected format {} on line {}",
				format, curr_line);
			return false;
		}

		if (++iter == end) {
			LOG_ERROR("CDROM", "[CUE] Incomplete FILE entry, missing track");
			return false;
		}
		
		auto all_offsets = CdLocation(0, 2, 0); //pregaps and postgaps are accumulated
		if (!m_tracks.empty()) { //2-second pregap is only for the first track
			all_offsets = CdLocation{};
		}
		while (auto maybe_track = ReadTrackEntry(base, iter, end)) {
			auto track = maybe_track.value();
			track.path = complete_path;
			if (m_tracks.empty()) {
				track.pregap = track.pregap + all_offsets;
				track.begin = track.pregap;
				track.file_offset = 0;
			}
			else {
				all_offsets = all_offsets + track.pregap; //add another pregap to all the offsets
				auto index = std::find_if(track.indexes.cbegin(), track.indexes.cend(),
					[](Index const& i) { return i.id == 1; }); //INDEX 01 is required, gives file position
				if (index == track.indexes.cend()) {
					LOG_ERROR("CDROM", "[CUE] Incomplete TRACK entry, missing INDEX 01");
					continue;
				}
				track.begin = index->position + all_offsets; //index position if offset by all previous gaps
				if (m_tracks.back().path != complete_path) {
					track.file_offset = 0;
				}
				else {
					track.file_offset = index->position.to_lba();
				}
				all_offsets = all_offsets + track.postgap;
			}
			m_tracks.push_back(track);
		}

		return !m_tracks.empty();
	}

	std::optional<Track> CueSheet::ReadTrackEntry(std::vector<std::string>::iterator base, std::vector<std::string>::iterator& iter, std::vector<std::string>::iterator end)
	{
		if (iter == end) {
			return std::nullopt;
		}
		auto curr_line = std::distance(base, iter);

		auto space_pos = iter->find_first_of(' ');
		auto entry = iter->substr(0, space_pos);

		if (entry != "TRACK") {
			LOG_ERROR("CDROM", "[CUE] Unexpected entry {} on line {}, expected TRACK",
				entry, curr_line);
			return std::nullopt;
		}

		auto next_space_pos = iter->find_first_of(' ', space_pos + 1);

		auto distance = next_space_pos - space_pos;
		if (distance <= 1) {
			LOG_ERROR("CDROM", "[CUE] Missing track index on line {}",
				curr_line);
			return std::nullopt;
		}
		auto track_index_string = iter->substr(space_pos + 1, distance - 1);

		std::uint64_t track_index = {};
		try {
			track_index = std::stoi(track_index_string);
		}
		catch (...) {
			LOG_ERROR("CDROM", "[CUE] Invalid track index on line {}",
				curr_line);
			return std::nullopt;
		}

		auto mode = iter->substr(next_space_pos + 1);

		if (mode != "MODE2/2352" && mode != "AUDIO") {
			LOG_ERROR("CDROM", "[CUE] Unsupported track mode {} on line {}",
				mode, curr_line);
			return std::nullopt;
		}

		Track track{};
		track.track_index = track_index;

		if (mode == "MODE2/2352") {
			track.track_type = TrackType::MODE2_2352;
		}
		else if(mode == "AUDIO") {
			track.track_type = TrackType::AUDIO;
		}

		if (++iter == end) {
			LOG_ERROR("CDROM", "[CUE] Truncated track");
			return {};
		}

		bool pregap_found = false;
		bool postgap_found = false;

		auto get_cd_position = [&](std::size_t start) -> std::optional<CdLocation> {
			auto mm_end = iter->find_first_of(':', start);
			auto ss_end = iter->find_first_of(':', mm_end + 1);

			if (mm_end == std::string::npos ||
				ss_end == std::string::npos) {
				return {};
			}

			auto mm_string = iter->substr(start, mm_end - start);
			auto ss_string = iter->substr(mm_end + 1, ss_end - (mm_end + 1));
			auto ff_string = iter->substr(ss_end + 1);

			CdLocation pos{};

			try {
				pos.mm = std::stoi(mm_string);
			}
			catch (...) { return {}; }

			try {
				pos.ss = std::stoi(ss_string);
			}
			catch (...) { return {}; }

			try {
				pos.sect = std::stoi(ff_string);
			}
			catch (...) { return {}; }

			return pos;
		};

		while (true) {
			curr_line = std::distance(base, iter);
			auto space_pos = iter->find_first_of(' ');
			auto entry = iter->substr(0, space_pos);

			if (entry != "INDEX" && entry != "PREGAP" && entry != "POSTGAP")
				break;

			if (entry == "PREGAP") {
				if (!pregap_found) {
					auto pos = get_cd_position(space_pos + 1);
					if (!pos) {
						LOG_ERROR("CDROM", "[CUE] Invalid pregap size on line {}",
							curr_line);
						return {};
					}
					track.pregap = pos.value();
				}
				else {
					LOG_WARN("CDROM", "[CUE] Duplicate pregap on line {}",
						curr_line);
				}
				pregap_found = true;
			}
			else if (entry == "POSTGAP") {
				if (!postgap_found) {
					auto pos = get_cd_position(space_pos + 1);
					if (!pos) {
						LOG_ERROR("CDROM", "[CUE] Invalid postgap size on line {}",
							curr_line);
						return {};
					}
					track.postgap = pos.value();
				}
				else {
					LOG_WARN("CDROM", "[CUE] Duplicate postgap on line {}",
						curr_line);
				}
				postgap_found = true;
			}
			else {
				auto next_space_pos = iter->find_first_of(' ', space_pos + 1);

				auto distance = next_space_pos - space_pos;
				if (distance <= 1) {
					LOG_ERROR("CDROM", "[CUE] Missing index id on line {}",
						curr_line);
					return {};
				}
				auto index_id_string = iter->substr(space_pos + 1, distance - 1);

				std::uint64_t index_id = {};
				try {
					index_id = std::stoi(index_id_string);
				}
				catch (...) {
					LOG_ERROR("CDROM", "[CUE] Invalid index id on line {}",
						curr_line);
					return {};
				}

				auto pos = get_cd_position(next_space_pos + 1);
				if (!pos) {
					LOG_ERROR("CDROM", "[CUE] Invalid index position on line {}",
						curr_line);
					return {};
				}
				
				Index index{};
				index.id = index_id;
				index.position = pos.value();
				track.indexes.push_back(index);
			}

			if (++iter == end) {
				break;
			}
		}

		if (track.indexes.empty()) {
			curr_line = std::distance(base, iter);
			LOG_ERROR("CDROM", "[CUE] Incomplete track ending on line {}",
				curr_line);
			return {};
		}

		return track;
	}
}