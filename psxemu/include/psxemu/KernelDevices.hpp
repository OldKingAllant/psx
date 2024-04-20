#pragma once

#include <common/Defs.hpp>

namespace psx::kernel {
	enum class DeviceFlags : u32 {
		CDROM = 0x14,
		TTY_DUMMY = 0x1,
		TTY_DUART = 0x3
	};

	struct DeviceControlBlock {
#pragma pack(push, 4)
		u32 lowercase_name_ptr;
		DeviceFlags flags;
		u32 sect_size;
		u32 uppercase_name_ptr;
		u32 init;
		u32 open;
		u32 in_out;
		u32 close;
		u32 ioctl;
		u32 read;
		u32 write;
		u32 erase;
		u32 undelete;
		u32 firstfile2;
		u32 nextfile;
		u32 format;
		u32 cd;
		u32 rename;
		u32 remove;
		u32 testdevice;
#pragma pack(pop)
	};
}