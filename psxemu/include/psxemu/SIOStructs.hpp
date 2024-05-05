#pragma once

#include <common/Defs.hpp>

namespace psx {
	struct SIOFifo {
		u8 entries[8];
		u8 num_bytes;
	};

	union SIOStat {
#pragma pack(push, 1)
		struct {
			bool tx_not_full : 1;
			bool rx_not_empty : 1;
			bool tx_idle : 1;
			bool rx_parity_err : 1;
			bool rx_buffer_overrun : 1;
			bool rx_bad_stop : 1;
			bool rx_input_level : 1;
			bool dsr_input_level : 1;
			bool cts_input_level : 1;
			bool int_req : 1;
		};
#pragma pack(pop)

		u32 reg;
	};

	enum class CharLength : u8  {
		BITS5 = 0,
		BITS6 = 1,
		BITS7 = 2,
		BITS8 = 3
	};

	enum class Parity : u8 {
		EVEN = 0,
		ODD = 1
	};

	enum class StopBit : u8 {
		RESERVED_ONE = 0,
		ONE = 1,
		ONE_HALF = 2,
		TWO = 3
	};

	enum class ClockPolarity : u8 {
		IDLE_HIGH = 0,
		IDLE_LOW = 1
	};

	union SIOMode {
#pragma pack(push, 1)
		struct {
			u8 baudrate_reload_factor : 2;
			CharLength char_len : 2;
			bool parity_enable : 1;
			Parity parity_type : 1;
			StopBit stop_bit_len : 2;
			ClockPolarity clock_polarity : 1;
		};
#pragma pack(pop)

		u16 reg;
	};

	enum class PortSelect : u8 {
		PORT1 = 0,
		PORT2 = 1
	};

	union SIOControl {
#pragma pack(push, 1)
		struct {
			bool tx_enable : 1;
			bool dtr_output_level : 1;
			bool rx_enable : 1;
			bool invert_tx_output_level : 1;
			bool : 1;
			bool rts_output_level : 1;
			bool : 1;
			bool : 1;
			u8 rx_irq_after_n_bytes : 2;
			bool tx_irq_enable : 1;
			bool rx_int_enable : 1;
			bool dsr_int_enable : 1;
			PortSelect port_select : 1;
		};
#pragma pack(pop)

		u16 reg;
	};
}