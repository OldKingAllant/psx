#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>
#include <common/Queue.hpp>

namespace psx {
	union IndexRegister {
#pragma pack(push, 1)
		struct {
			u8 index : 2;
			bool xa_adpcm_fifo_empty : 1;
			bool param_fifo_empty : 1;
			bool param_fifo_full : 1;
			bool response_fifo_empty : 1;
			bool data_fifo_empty : 1;
			bool transmission_busy : 1;
		};
#pragma pack(pop)

		u8 reg;
	};

	struct ResponseFifo {
		u8 fifo[16];
		u8 num_bytes;
		u8 curr_index;
	};

	union InterruptEnable {
#pragma pack(push, 1)
		struct {
			u8 enable_bits : 5;
		};
#pragma pack(pop)

		u8 reg;
	};

	enum class CdInterrupt : u8 {
		INT1_DATA_RESPONSE = 1,
		INT2_SECOND_RESPONSE = 2,
		INT3_FIRST_RESPONSE = 3,
		INT4_DATA_END = 4,
		INT5_ERR = 5
	};

	struct Response {
		ResponseFifo fifo;
		CdInterrupt interrupt;
		u64 delay;
		u64 timestamp;
	};

	union InterruptFlag {
#pragma pack(push, 1)
		struct {
			CdInterrupt irq : 3;
			bool : 1;
			bool cmd_start : 1;
		};
#pragma pack(pop)

		u8 reg;
	};

	struct XA_ADPCM_Volume {
		u8 left_to_left;
		u8 left_to_right;
		u8 right_to_right;
		u8 right_to_left;
	};

	union SoundCoding {
#pragma pack(push, 1)
		struct {
			bool stereo : 1;
			bool : 1;
			bool sampler_rate_18900 : 1;
			bool : 1;
			bool bits_per_sample_8 : 1;
			bool : 1;
			bool emphasis : 1;
		};
#pragma pack(pop)

		u8 reg;
	};

	/*
	Timings taken from no$psx documentation
	*/

	struct ResponseTimings {
		static constexpr u32 GETSTAT_STOPPED = 0x0005cf4;
		static constexpr u32 GETSTAT_NORMAL = 0x000c4e1;
		static constexpr u32 INIT = 0x0013cce;
		static constexpr u32 READ_TOC = INIT;
		static constexpr u32 GET_ID = 0x0004a00;
		static constexpr u32 PAUSE = 0x021181c;
		static constexpr u32 PAUSE_PAUSED = 0x0001df2;
		static constexpr u32 STOP = 0x0d38aca;
		static constexpr u32 STOP_STOPPED = 0x0001d7b;
		static constexpr u32 READ = (u32)SYSTEM_CLOCK * 0x930 / 4 / 44100;
	};

	union Mode {
#pragma pack(push, 1)
		struct {
			bool allow_cd_da : 1;
			bool autopause : 1;
			bool report : 1;
			bool xa_filter_enable : 1;

			/// <summary>
			/// Function unknown
			/// </summary>
			bool ignore : 1;

			bool read_whole_sector : 1;
			bool enable_xa_adpcm : 1;
			bool double_speed : 1;
		};
#pragma pack(pop)

		u8 reg;
	};

	enum class CommandError {
		SEEK_FAILED = 0x4,
		DRIVE_DOOR_OPEN = 0x8,
		INVALID_SUB_FUNCTION = 0x10,
		WRONG_NUM_OF_PARAMS = 0x20,
		INVALID_COMMAND = 0x40,
		NOT_READY = 0x80
	};

	union Stat {
		struct {
#pragma pack(push, 1)
			bool err : 1;
			bool motor_on : 1;
			bool seek_err : 1;
			bool id_err : 1;
			bool shell_open : 1;
			bool reading : 1;
			bool seeking : 1;
			bool playing : 1;
#pragma pack(pop)
		};

		u8 reg;
	};
}