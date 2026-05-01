#pragma once

#include <filesystem>
#include <vector>
#include <optional>

#include "CdLocation.hpp"

namespace psx {
	class CueSheet {
	public :
		CueSheet();

		bool ReadCue(std::filesystem::path const& path);

		enum class TrackType {
			MODE2_2352,
			AUDIO
		};

		struct Index {
			CdLocation position;
			u64 id;
		};

		struct Track {
			u64 track_index;
			TrackType track_type;
			CdLocation pregap;
			CdLocation postgap;
			std::vector<Index> indexes;
			CdLocation begin;
			CdLocation end;
			std::filesystem::path path;
			u64 file_offset;
		};

		std::vector<Track> const& GetTracks() const {
			return m_tracks;
		}

	private :
		bool ReadFileEntry(std::filesystem::path const& cue_path, 
			std::vector<std::string>::iterator base,
			std::vector<std::string>::iterator& iter,
			std::vector<std::string>::iterator end);
		std::optional<Track> ReadTrackEntry(std::vector<std::string>::iterator base,
			std::vector<std::string>::iterator& iter,
			std::vector<std::string>::iterator end);

	private :
		std::vector<Track> m_tracks;
	};
}