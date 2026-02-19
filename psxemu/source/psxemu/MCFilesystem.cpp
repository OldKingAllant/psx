#include <psxemu/include/psxemu/MCFilesystem.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <thirdparty/magic_enum/include/magic_enum/magic_enum.hpp>

#include <algorithm>
#include <bit>
#include <set>
#include <memory>
#include <unordered_map>

namespace psx::kernel {
	MCFs::MCFs() :
		m_mc{}, m_last_update_seq_number{},
		m_card_slot{u32(-1)}
	{}

	void MCFs::UpdateTree() {
		u32 seq_number = m_mc->GetUpdateSequenceNumber();
		if (m_last_update_seq_number == seq_number) {
			return;
		}

		LOG_INFO("KERNEL", "[KERNELFS] MC at slot {} has changed, reading fs", 
			m_card_slot);

		m_last_update_seq_number = seq_number;
		TraverseFilesystem();
	}

	void MCFs::TraverseFilesystem() {
		std::set<u32> visited_frames{};
		for (u32 curr_dir = 0; curr_dir < NUM_DIRECTORIES; curr_dir++) {
			//Frame may be continuation of another file
			if (visited_frames.contains(curr_dir)) {
				continue;
			}

			MCDirectoryFrame curr_entry{};
			auto frame_data = m_mc->ReadFrame(curr_dir + FIRST_DIRECTORY_FRAME).value();
			std::copy_n(frame_data.begin(), sizeof(MCDirectoryFrame),
				std::bit_cast<u8*>(&curr_entry));

			//We want to follow the file descriptor only 
			//if we are at the beginning of the file,
			//such that the frames are in order
			if (curr_entry.block_alloc_state != MCBlockAllocationState::FIRST_BLOCK) {
				continue;
			}

			//Mark first block as visited
			visited_frames.insert(curr_dir);

			//Check if next block pointer is valid
			if (curr_entry.next_block != INVALID_BLOCK_PTR && curr_entry.next_block >= NUM_DIRECTORIES - 1) {
				LOG_ERROR("KERNEL", "[KERNELFS] Next block pointer >= 15");
				continue;
			}

			HLEFile fd{};
			fd.entry_location = EntryLocation::CARD;
			fd.mc_data.first_block = curr_dir;
			fd.mc_data.dir_frames.push_back(curr_entry);
			fd.mc_data.fname = std::string{ std::begin(curr_entry.filename_ascii),
				std::end(curr_entry.filename_ascii) - 1
			};
			fd.mc_data.card_slot = m_card_slot;
			fd.mc_data.mc_version = m_last_update_seq_number;

			bool is_invalid = false;

			//If first block is not also the last, follow
			//the list
			if (curr_entry.next_block != INVALID_BLOCK_PTR) {
				u32 next_frame_ptr = curr_entry.next_block;

				while (true) {
					if (visited_frames.contains(next_frame_ptr)) {
						//This should not be possible, since we
						//are following the list of blocks
						//from the head
						is_invalid = true;
						LOG_ERROR("KERNEL", "[KERNELFS] Circular list of blocks for file {}", 
							fd.mc_data.fname);
						break;
					}
					visited_frames.insert(next_frame_ptr);
					auto frame_data = m_mc->ReadFrame(next_frame_ptr + FIRST_DIRECTORY_FRAME).value();

					MCDirectoryFrame curr_entry{};
					std::copy_n(frame_data.begin(), sizeof(MCDirectoryFrame),
						std::bit_cast<u8*>(&curr_entry));

					if (curr_entry.next_block != INVALID_BLOCK_PTR && curr_entry.next_block >= NUM_DIRECTORIES - 1) {
						LOG_ERROR("KERNEL", "[KERNELFS] Next block pointer >= 15 for file {}",
							fd.mc_data.fname);
						is_invalid = true;
						break;
					}

					switch (curr_entry.block_alloc_state)
					{
					case MCBlockAllocationState::JUST_FORMATTED:
					case MCBlockAllocationState::DELETED_FIRST_BLOCK:
					case MCBlockAllocationState::DELETED_MIDDLE_BLOCK:
					case MCBlockAllocationState::DELETED_LAST_BLOCK: {
						LOG_ERROR("KERNEL", "[KERNELFS] Block marked {} in file {}",
							magic_enum::enum_name(curr_entry.block_alloc_state),
							fd.mc_data.fname);
						is_invalid = true;
						break;
					}
					default:
						break;
					}

					if (is_invalid) {
						break;
					}

					if (curr_entry.block_alloc_state == MCBlockAllocationState::LAST_BLOCK &&
						curr_entry.next_block != INVALID_BLOCK_PTR) {
						LOG_ERROR("KERNEL", "[KERNELFS] Block marked as last but pointer != null for file {}",
							fd.mc_data.fname);
						is_invalid = true;
						break;
					}

					if (curr_entry.block_alloc_state == MCBlockAllocationState::MIDDLE_BLOCK &&
						curr_entry.next_block == INVALID_BLOCK_PTR) {
						LOG_ERROR("KERNEL", "[KERNELFS] Block marked as middle but pointer == null for file {}",
							fd.mc_data.fname);
						is_invalid = true;
						break;
					}

					fd.mc_data.dir_frames.push_back(curr_entry);

					if (curr_entry.block_alloc_state == MCBlockAllocationState::LAST_BLOCK) {
						break;
					}

					next_frame_ptr = curr_entry.next_block;
				}
			}

			u32 filesize_in_blocks = fd.mc_data.dir_frames.begin()->filesize /
				BLOCK_SIZE;
			if (filesize_in_blocks != fd.mc_data.dir_frames.size()) {
				LOG_ERROR("KERNEL", "[KERNELFS] File descriptor for {} declares a size of {:#x} blocks, but found {:#x}",
					fd.mc_data.fname, filesize_in_blocks, fd.mc_data.dir_frames.size());
				is_invalid = true;
			}

			if (!is_invalid) {
				u32 first_block_location = fd.mc_data.first_block + 1;
				auto title_frame_data = m_mc->ReadFrame(first_block_location * FRAMES_PER_BLOCK);
				if (!title_frame_data.has_value()) {
					LOG_ERROR("KERNEL", "[KERNELFS] File descriptor for {} indicates invalid block",
						fd.mc_data.fname);
					continue;
				}

				std::copy_n(title_frame_data.value().cbegin(), sizeof(MCTitleFrame),
					std::bit_cast<u8*>(&fd.mc_data.title_frame));

				m_files.push_back(std::make_shared<HLEFile>(fd));
			}
		}

		LOG_DEBUG("KERNEL", "[KERNELFS] Dump of fs MC slot {}:", m_card_slot);
		for (auto const& fd : m_files) {
			LOG_DEBUG("KERNEL", "           File {}",
				fd->mc_data.fname);
		}
	}

	std::optional<std::shared_ptr<HLEFsEntry>> MCFs::GetEntry(std::string path) {
		UpdateTree();
		//There are no directories, so there is only
		//a single level of depth
		for (auto const& entry : m_files) {
			auto entry_name = entry->mc_data.fname;

			std::transform(entry_name.cbegin(), entry_name.cend(),
				entry_name.begin(), std::toupper);

			if (entry_name == path) {
				return entry;
			}
		}

		return std::nullopt;
	}

	std::optional<std::vector<u8>> MCFs::ReadFileFromEntry(std::shared_ptr<HLEFsEntry> entry) {
		u32 file_seq_update = entry->mc_data.mc_version;
		UpdateTree();
		if (file_seq_update != m_last_update_seq_number) {
			LOG_WARN("KERNEL", "[KERNELFS] File {} has old MC version",
				entry->mc_data.fname);
			return std::nullopt;
		}
		return ReadFileFromEntry(entry, 0, entry->mc_data.dir_frames.begin()->filesize);
	}

	std::optional<std::vector<u8>> MCFs::ReadFileFromEntry(std::shared_ptr<HLEFsEntry> entry, u32 off, u32 len) {
		u32 file_seq_update = entry->mc_data.mc_version;
		UpdateTree();
		if (file_seq_update != m_last_update_seq_number) {
			LOG_WARN("KERNEL", "[KERNELFS] Handle to {} has old MC version",
				entry->mc_data.fname);
			return std::nullopt;
		}
		
		u32 start_frame_inside_file = off / FRAME_SIZE;
		u32 offset_inside_first_frame = off % FRAME_SIZE;

		u32 total_file_size = entry->mc_data.dir_frames.begin()->filesize;
		if (off + len > total_file_size) {
			return std::nullopt;
		}

		std::vector<u8> file_data{};
		file_data.resize(len);

		//Get first block id of file
		u32 curr_block_number = entry->mc_data.first_block + 1;
		//Read first frame taking offset fram in consideration
		auto first_frame = m_mc->ReadFrame(curr_block_number * FRAMES_PER_BLOCK
			+ start_frame_inside_file).value();

		//Compute how many bytes we need/can copy from the
		//first frame
		auto to_copy = (size_t)len >= FRAME_SIZE ? FRAME_SIZE : (size_t)len;

		if (to_copy + offset_inside_first_frame >= FRAME_SIZE) {
			to_copy = u64(FRAME_SIZE) - offset_inside_first_frame;
		}

		len -= u32(to_copy);
		u32 curr_pos = 0;

		std::copy_n(first_frame.cbegin() + offset_inside_first_frame, to_copy, file_data.begin() + curr_pos);
		curr_pos += u32(to_copy);

		//Go to next frame in the block
		u32 curr_frame_in_block = start_frame_inside_file + 1;
		//Pointer to current dir frame
		auto curr_dir_descriptor = entry->mc_data.dir_frames.begin();

		while (len) {
			//Read to the end of the block or until we have read
			//enough bytes
			while (curr_frame_in_block < FRAMES_PER_BLOCK && len) {
				//Read another frame
				auto curr_frame = m_mc->ReadFrame(curr_block_number * FRAMES_PER_BLOCK
					+ curr_frame_in_block).value();

				auto to_copy = (size_t)len >= FRAME_SIZE ? FRAME_SIZE : (size_t)len;
				std::copy_n(curr_frame.cbegin(), to_copy, file_data.begin() + curr_pos);

				len -= u32(to_copy);
				curr_pos += u32(to_copy);
				curr_frame_in_block++;
			}

			if (len == 0) {
				break;
			}

			//Reset frame index in block
			curr_frame_in_block = 0;
			//Read next linked list entry
			curr_block_number = curr_dir_descriptor->next_block + 1;
			curr_dir_descriptor++;
		}

		return file_data;
	}
}