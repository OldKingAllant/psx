#include <psxemu/include/psxemu/formats/GenericSaveFile.hpp>
#include <psxemu/include/psxemu/Kernel.hpp>

#include <thirdparty/hash/sha256.h>
#include <bit>

namespace psx::kernel {
	GenericSaveFile::GenericSaveFile(
		std::string const& saved_name,
		std::string const& header_hash, 
		MCFs* mcfs, 
		std::shared_ptr<HLEFsEntry> fsentry) :
		m_saved_name{saved_name},
		m_header_hash{header_hash},
		m_fs{mcfs},
		m_fs_entry{fsentry},
		m_decoded_icon_frames{},
		m_decoded_title{"<invalid>"} {
		if (auto maybe_decoded = Kernel::DecodeShiftJIS(std::span{ m_fs_entry->mc_data.title_frame.title_shift_jis })) {
			m_decoded_title = maybe_decoded.value();
		}
		
		DecodeIcons();
	}

	void GenericSaveFile::DecodeIcons() {
		u32 icon_frame_count = {};
		switch (m_fs_entry->mc_data.title_frame.disp_flag)
		{
		case MCIconDisplayFlag::SINGLE_FRAME:
			icon_frame_count = 1;
			break;
		case MCIconDisplayFlag::TWO_FRAMES:
			icon_frame_count = 2;
			break;
		case MCIconDisplayFlag::THREE_FRAMES:
			icon_frame_count = 3;
			break;
		default:
			return;
		}

		u16* clut_ptr = std::bit_cast<u16*>(&m_fs_entry->mc_data.title_frame.icon_clut[0]);

		for (u32 curr_icon = 0; curr_icon < icon_frame_count; curr_icon++) {
			std::vector<u32> icon{};
			icon.reserve(ICON_SIZE_PIXELS);

			auto encoded_icon = m_fs->ReadFileFromEntry(m_fs_entry, FRAME_SIZE * (curr_icon + 1),
				FRAME_SIZE).value();

			//4 bit color depth for each pixel. Color value is index inside clut
			for (u32 curr_index = 0; curr_index < ICON_SIZE_PIXELS; curr_index++) {
				u8 clut_index = (encoded_icon[curr_index >> 1] >> (4 * (curr_index & 1))) & 0xF;
				u16 clut_value = clut_ptr[clut_index];
				u32 r = (clut_value & 0x1F) << 3;
				u32 g = ((clut_value >> 5) & 0x1F) << 11;
				u32 b = ((clut_value >> 10) & 0x1F) << 19;
				u32 a = (((clut_value >> 15) & 1) * 255) << 24;
				u32 final_color = r | g | b | a;
				icon.push_back(final_color);
			}

			m_decoded_icon_frames.emplace_back(icon);
		}
	}

	std::optional<std::string> GenericSaveFile::ComputeHeaderHash(std::string const& fname, MCFs* fs) {
		auto maybe_fs_entry = fs->GetEntry(fname);
		if (!maybe_fs_entry.has_value()) {
			return std::nullopt;
		}

		auto fs_entry = maybe_fs_entry.value();

		SHA256 sha_obj{};
		sha_obj.add(std::bit_cast<void*>(fname.data()), fname.size());
		sha_obj.add(std::bit_cast<void*>(&fs_entry->mc_data.title_frame),
			sizeof(MCTitleFrame));

		//Assume icon bitmaps remain the same

		return sha_obj.getHash();
	}

	std::optional<GenericSaveFile> GenericSaveFile::CreateSavefileHandle(std::string const& fname, MCFs* fs) {
		auto maybe_hash = ComputeHeaderHash(fname, fs);
		if (!maybe_hash.has_value()) {
			return std::nullopt;
		}
		auto fsentry = fs->GetEntry(fname).value();
		return GenericSaveFile(fname, maybe_hash.value(), fs, fsentry);
	}

	bool GenericSaveFile::TryUpdateHandle() {
		if (m_fs->IsEntryValid(m_fs_entry)) {
			return true;
		}
		//Some sectors in the MC have been overwritten,
		//try to compute the header hash in the MC, if
		//it computation fails or the new hash
		//is different, consider the file handle as invalid
		auto maybe_new_hash = ComputeHeaderHash(m_saved_name, m_fs);
		if (!maybe_new_hash.has_value()) {
			//File may have been deleted
			return false;
		}

		auto hash = maybe_new_hash.value();
		if (hash != m_header_hash) {
			//File has changed
			return false;
		}

		m_fs_entry = m_fs->GetEntry(m_saved_name).value();
		m_header_hash = hash;
		return true;
	}
}