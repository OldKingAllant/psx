#pragma once

#include <common/Defs.hpp>

namespace psx::kernel {
	enum class AccessMode : u32 {
		NONE = 0,
		READ = 1,
		WRITE = 2,
		NO_WAIT = 4,
		CREATE = (1 << 9),
	};

	struct FileControlBlock {
#pragma pack(push, 4)
		AccessMode status;
		u32 disk_id;
		u32 transfer_address;
		u32 transfer_len;
		u32 curr_pos;
		u32 dev_flags;
		u32 error;
		u32 dcb_pointer;
		u32 fsize;
		u32 lba;
		u32 fcb_num;
#pragma pack(pop)
	};
}