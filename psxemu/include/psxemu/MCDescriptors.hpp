#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

namespace psx::kernel {
	//8KB
	static constexpr u32 BLOCK_SIZE = 8192;
	static constexpr u32 FRAME_SIZE = 128;
	static constexpr u32 FRAMES_PER_BLOCK = BLOCK_SIZE /
		FRAME_SIZE;

	struct MCHeaderFrame {
#pragma pack(push, 1)
		char id[2] = { 'M', 'C' };
		u8 unused[0x7F - 0x2] = {};
		u8 checksum = 0xE;
#pragma pack(pop)
	};

	enum class MCBlockAllocationState : u32 {
		FIRST_BLOCK = 0x51,
		MIDDLE_BLOCK = 0x52,
		LAST_BLOCK = 0x53,
		JUST_FORMATTED = 0xA0,
		DELETED_FIRST_BLOCK =  0xA1,
		DELETED_MIDDLE_BLOCK = 0xA2,
		DELETED_LAST_BLOCK =   0xA3,
	};

	static constexpr u16 INVALID_BLOCK_PTR = 0xFFFF;

#pragma pack(push, 1)
	struct MCDirectoryFrame {
		MCBlockAllocationState block_alloc_state;
		u32 filesize;
		u16 next_block;
		char filename_ascii[0x1F - 0xA];
		u8 unused;
		u8 garbage[0x7F - 0x20];
		u8 checksum;
	};
#pragma pack(pop)

	static_assert(sizeof(MCDirectoryFrame) == FRAME_SIZE);

	static constexpr u32 INVALID_FRAME_PTR = 0xFFFFFFFF;

#pragma pack(push, 1)
	struct MCBrokenFrameDescriptor {
		u32 broken_frame_number;
		u8 garbage[0x7F - 0x4];
		u8 checksum;
	};
#pragma pack(pop)

#pragma pack(push, 1)
	struct MCBrokenFrameReplace {
		u8 data[128];
	};
#pragma pack(pop)

	static constexpr u32 BROKEN_FRAME_LIST_START = 16;
	static constexpr u32 FRAME_REPLACEMENT_START = 36;

	static constexpr u32 BROKEN_FRAME_LIST_LEN = 20;

	enum class MCIconDisplayFlag : u8 {
		SINGLE_FRAME = 0x11,
		TWO_FRAMES = 0x12,
		THREE_FRAMES = 0x13
	};

#pragma pack(push, 1)
	struct MCTitleFrame {
		char id[2];
		MCIconDisplayFlag disp_flag;
		u8 block_number;
		char title_shift_jis[64];
		u8 reserved1[0x50 - 0x44];
		u8 reserved2[0x60 - 0x50];
		u8 icon_clut[0x80 - 0x60];
	};
#pragma pack(pop)

	static constexpr u32 NUM_DIRECTORIES = 16;
	static constexpr u32 FIRST_DIRECTORY_FRAME = 1;
}