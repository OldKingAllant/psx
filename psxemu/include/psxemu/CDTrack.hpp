#pragma once

#include "CdLocation.hpp"

#include <filesystem>
#include <vector>

namespace psx {
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
}