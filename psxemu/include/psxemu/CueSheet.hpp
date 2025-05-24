#pragma once

#include <filesystem>
#include <vector>
#include <optional>

namespace psx {
	class CueSheet {
	public :
		CueSheet();

		bool ReadCue(std::filesystem::path const& path);

		enum class TrackType {
			MODE2_2352
		};

		struct Position {
			std::uint64_t mm;
			std::uint64_t ss;
			std::uint64_t ff;
		};

		struct Index {
			Position position;
			std::uint64_t id;
		};

		struct Track {
			std::uint64_t track_index;
			TrackType track_type;
			Position pregap;
			Position postgap;
			std::vector<Index> indexes;
		};

		struct FileEntry {
			std::filesystem::path relative_path;
			std::vector<Track> tracks;
		};

		std::vector<FileEntry> const& GetFiles() const {
			return m_files;
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
		std::vector<FileEntry> m_files;
	};
}