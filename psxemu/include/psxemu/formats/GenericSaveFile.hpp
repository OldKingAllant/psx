#pragma once

#include <psxemu/include/psxemu/MCFilesystem.hpp>

#include <string>
#include <optional>
#include <vector>

namespace psx::kernel {
	class GenericSaveFile {
	public :
		static std::optional<std::string> ComputeHeaderHash(std::string const& fname, MCFs* fs);
		static std::optional<GenericSaveFile> CreateSavefileHandle(std::string const& fname, MCFs* fs);

		bool TryUpdateHandle();

		std::shared_ptr<HLEFsEntry> GetFsEntry() {
			return m_fs_entry;
		}

		std::string const& GetTitle() const {
			return m_decoded_title;
		}

		std::string const& GetSavedName() const {
			return m_saved_name;
		}

		std::vector<std::vector<u32>> const& GetIconFrames() const {
			return m_decoded_icon_frames;
		}

		static constexpr u32 ICON_SIZE_PIXELS = 256;
		static constexpr u32 ICON_X_SIZE = 16;
		static constexpr u32 ICON_Y_SIZE = 16;

		GenericSaveFile& operator=(GenericSaveFile const& other) {
			m_saved_name = other.m_saved_name;
			m_header_hash = other.m_header_hash;
			m_fs = other.m_fs;
			m_fs_entry = other.m_fs_entry;
			m_decoded_icon_frames = other.m_decoded_icon_frames;
			m_decoded_title = other.m_decoded_title;
			return *this;
		}

		GenericSaveFile() = default;

	private :
		GenericSaveFile(std::string const& saved_name, 
			std::string const& header_hash, 
			MCFs* mcfs, 
			std::shared_ptr<HLEFsEntry> fsentry);

		void DecodeIcons();

	private :
		std::string m_saved_name;
		std::string m_header_hash;
		MCFs* m_fs;
		std::shared_ptr<HLEFsEntry> m_fs_entry;
		std::vector<std::vector<u32>> m_decoded_icon_frames;
		std::string m_decoded_title;
	};
}