#pragma once

#include <common/Defs.hpp>

namespace psx::kernel {
	enum class EventStatus : u32 {
		FREE = 0,
		DISABLED = 0x1000,
		BUSY = 0x2000,
		READY = 0x4000
	};

	enum class EventClass : u32 {
		VBLANK = 0xF0000001,
		GPU_IRQ = 0xF0000002,
		CDROM = 0xF0000003,
		DMA = 0xF0000004,
		RTC0 = 0xF0000005,
		RTC1 = 0xF0000006,
		CONTROLLER = 0xF0000008,
		SPU = 0xF0000009,
		PIO = 0xF000000A,
		SIO = 0xF000000B,
		CPU_EXCEPTION = 0xF0000010,
		COUNTER0 = 0xF2000000,
		COUNTER1 = 0xF2000001,
		COUNTER2 = 0xF2000002,
		COUNTER3 = 0xF2000003
	};

	enum class EventMode : u32 {
		EXECUTE = 0x1000,
		MARK_READY = 0x2000
	};

	enum class EventSpec : u32 {
		COUNTER_BECOMES_ZERO = 0x1,
		INTERRUPTED = 0x2,
		END_OF_IO = 0x4,
		FILE_CLOSED = 0x8,
		COMMAND_ACK = 0x10,
		COMMAND_COMPLETE = 0x20,
		DATA_READY = 0x40,
		DATA_END = 0x80,
		TIMEOUT = 0x100,
		UNKNOWN_COMMAND = 0x200,
		END_OF_READBUF = 0x400,
		END_OF_WRITEBUF = 0x800,
		GENERAL_INT = 0x1000,
		NEW_DEVICE = 0x2000,
		SYSCALL = 0x4000,
		ERR = 0x8000
	};

	struct EventControlBlock {
#pragma pack(push, 4)
		EventClass ev_class;
		EventStatus status;
		EventSpec spec;
		EventMode mode;
		u32 func_pointer;
		u32 _u0, _u1;
#pragma pack(pop)
	};
}