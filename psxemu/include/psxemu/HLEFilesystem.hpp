#pragma once

#include "CdDescriptors.hpp"
#include "MCDescriptors.hpp"

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <list>

namespace psx::kernel {
	enum class EntryLocation {
		CDROM,
		CARD,
		HOST
	};

	struct HLEFsEntry {
		CompleteDirectoryRecord cd_record;
		
		struct {
			u32 first_block;
			std::list<MCDirectoryFrame> dir_frames{};
			MCTitleFrame title_frame;
			std::string fname;
			u32 card_slot = u32(-1);
			u32 mc_version = u32(-1);
		} mc_data = {};

		EntryLocation entry_location;
		HLEFsEntry() : cd_record{}, entry_location{} {}
		virtual ~HLEFsEntry() {}
		virtual void DepthVisit(std::function<void(std::shared_ptr<HLEFsEntry>)> visit_function) = 0;
	};

	struct HLEDirectory : public HLEFsEntry {
		std::vector<std::shared_ptr<HLEFsEntry>> entries;
		~HLEDirectory() override {}
		void DepthVisit(std::function<void(std::shared_ptr<HLEFsEntry>)> visit_function) override;
	};

	struct HLEFile : public HLEFsEntry {
		~HLEFile() override {}
		void DepthVisit(std::function<void(std::shared_ptr<HLEFsEntry>)> visit_function) override;
	};
}