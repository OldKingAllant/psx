#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include <intrin.h>
#include <string>

namespace psx::kernel {
	enum class VolumeDescriptorType : u8 {
		PRIMARY = 0x1,
		TERMINATOR = 0xFF
	};

	enum class VolumeDescriptorVersion : u8 {
		STANDARD = 1
	};

	enum class FileStructureVersion : u8 {
		STANDARD = 1
	};

	enum class FileFlags : u8 {
		HIDDEN = 1,
		DIRECTORY = 2,
		IS_ASSOCIATED_FILE = 4,
		RECORD = 8,
		RESTRICTIONS = 16,
		MULTI_EXTENT = (1 << 7)
	};

	enum class FileAttributes : u16 {
		OWNER_READ = 1,
		OWNER_EXECUTE = (1 << 2),
		GROUP_READ = (1 << 4),
		GROUP_EXECUTE = (1 << 6),
		WORLD_READ = (1 << 8),
		WORLD_EXECUTE = (1 << 10),
		IS_MODE2 = (1 << 11),
		IS_MODE2_FORM2 = (1 << 12),
		IS_INTERLEAVED = (1 << 13),
		IS_CDDA = (1 << 14),
		IS_DIRECTORY = (1 << 15)
	};

	/*
	* Most 16bit and 32bit entries are stored both
	* in little and big-endian
	*/

	struct LogicalBlock {
		LogicalBlock(u64 id) {
			this->id = id;
		}

		LogicalBlock() : id{} {}

		u64 id;
	};

	struct LogicalLocation {
		LogicalBlock block;
		u64 logical_block_size;

		LogicalLocation(LogicalBlock block, u64 block_sz) {
			this->block = block;
			this->logical_block_size = block_sz;
		}

		u64 get_absolute_location() const {
			return block.id * logical_block_size;
		}
	};

	struct LogicalBlockExtent {
		LogicalBlockExtent(u64 extent) :
			num_logical_blocks(extent) { }

		u64 num_logical_blocks;
	};

#pragma pack(push, 1)
	struct DirectoryRecord {
		u8 len_dr;
		u8 extended_attribute_record_len;
		u64 logical_block_number;
		u64 data_size;
		char timestamp[7];
		FileFlags file_flags;
		u8 file_unit_size;
		u8 interleave_gap_size;
		u32 volume_sequence_number;
		u8 len_fi;

		LogicalLocation le_logical_block_num(u64 logical_block_size) const {
			return LogicalLocation(LogicalBlock(u32(logical_block_number)), logical_block_size);
		}

		u32 le_data_size_bytes() const {
			return u32(data_size);
		}

		u16 le_volume_seq_number() const {
			return u16(volume_sequence_number);
		}

		u16 padding_len() const {
			return (len_fi & 1) == 0;
		}

		u16 calc_len_su() const {
			return len_dr - (33 + len_fi + padding_len());
		}
	};
#pragma pack(pop)

#pragma pack(push, 1)
	struct DirectoryRecordSystemArea {
		u16 owner_group_id;
		u16 owner_user_id;
		u16 file_attributes;
		char signature[2];
		u8 file_number;
		char _[5];

		FileAttributes le_file_attributes() const {
			u16 le_attributes = _byteswap_ushort(file_attributes);
			return FileAttributes(le_attributes);
		}
	};
#pragma pack(pop)

	struct CompleteDirectoryRecord {
		DirectoryRecord main_record;
		std::string entry_name;
		DirectoryRecordSystemArea sys_area;
	};


#pragma pack(push, 1)
	struct CdVolumeDescriptor {
		VolumeDescriptorType descriptor_type;
		char standard_identifier[5];
		VolumeDescriptorVersion descriptor_version;
		char _1;
		char system_identifier[32];
		char volume_identifier[32];
		char _2[8];
		u64 volume_space_size;
		char _3[32];
		u32 volume_set_size;
		u32 volume_sequence_number;
		u32 logical_block_size;
		u64 path_table_size;
		u32 path_tables_block_numbers[4];
		char root_dir_record[34];
		char volume_set_ident[128];
		char publisher_ident[128];
		char data_prep_ident[128];
		char application_ident[128];
		char copyright_filename[37];
		char abstract_filename[37];
		char biblio_filename[37];
		char volume_creation_timestamp[17];
		char volume_mod_timestamp[17];
		char volume_expire_timestamp[17];
		char volume_eff_timestamp[17];
		FileStructureVersion file_structure_version;
		char _4;
		char app_use_area1[141];
		char cd_xa_signature[8];
		char cd_xa_flags[2];
		char cd_xa_start_dir[8];
		char cd_xa_reserved[8];
		char app_use_area2[345];

		LogicalBlockExtent le_volume_space_size() const {
			return LogicalBlockExtent(u32(volume_space_size));
		}

		u16 le_volume_set_size() const {
			return u16(volume_set_size);
		}

		u16 le_volume_seq_number() const {
			return u16(volume_sequence_number);
		}

		u16 le_logical_block_size() const {
			return u16(logical_block_size);
		}

		u32 le_path_table_size() const {
			return u32(path_table_size);
		}

		LogicalLocation path_table_location(u8 table_num) const {
			if (table_num < 2) {
				return LogicalLocation(path_tables_block_numbers[table_num], le_logical_block_size());
			}
			auto be_block_num = path_tables_block_numbers[table_num];
			auto le_block_num = _byteswap_ulong(be_block_num);
			return LogicalLocation(le_block_num, le_logical_block_size());
		}
	};
#pragma pack(pop)
}