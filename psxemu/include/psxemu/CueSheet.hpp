#pragma once

#include <filesystem>
#include <vector>
#include <optional>

#include "CdLocation.hpp"
#include "CDTrack.hpp"

namespace psx {
	class CueSheet {
	public :
		CueSheet();

		bool ReadCue(std::filesystem::path const& path);

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