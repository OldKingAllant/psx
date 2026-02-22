#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include <psxemu/include/psxemu/AbstractMemcard.hpp>
#include <psxemu/include/psxemu/HLEFilesystem.hpp>
#include <psxemu/include/psxemu/MCDescriptors.hpp>

#include <memory>

namespace psx::kernel {
	class MCFs {
	public :
		MCFs();

		FORCE_INLINE void SetMemoryCard(std::shared_ptr<AbstractMemcard> new_card, u32 slot) {
			m_card_slot = slot;
			m_mc.swap(new_card);
			WriteReplacementData();
			TraverseFilesystem();
		}

		void UpdateTree();
		void TraverseFilesystem();

		std::optional<std::shared_ptr<HLEFsEntry>> GetEntry(std::string path);

		std::optional<std::vector<u8>> ReadFileFromEntry(std::shared_ptr<HLEFsEntry> entry);
		std::optional<std::vector<u8>> ReadFileFromEntry(std::shared_ptr<HLEFsEntry> entry, u32 off, u32 len);

		bool IsEntryValid(std::shared_ptr<HLEFsEntry> entry) const;

		FORCE_INLINE std::shared_ptr<AbstractMemcard> const& GetMc() const {
			return m_mc;
		}

		FORCE_INLINE auto const& GetFileList() {
			UpdateTree();
			return m_files;
		}

	private :
		void WriteReplacementData();

	private :
		std::shared_ptr<AbstractMemcard> m_mc;
		u32 m_last_update_seq_number;
		std::vector<std::shared_ptr<HLEFile>> m_files;
		u32 m_card_slot;
	};
}