#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

namespace psx::cpu {
	/// <summary>
	/// Hardware exception ID
	/// </summary>
	enum class Excode : u8 {
		INT, //Interrupt
		MOD,
		TLBL,
		TLBS,
		ADEL, //Invalid load/fetch
		ADES, //Invalid store
		IBE,  //Load Bus error
		DBE,  //Store Bus error
		SYSCALL, 
		BP, //Breakpoint instruction
		RI, //Reserved instruction
		COU, //Unusable coprocessor
		OV //Arithmetic overflow
	};

	constexpr u64 RESET_VECTOR = 0xBFC00000;
	constexpr u64 BREAK_VECTORS[] = { 0x80000040, 0xBFC00140 };
	constexpr u64 GENERAL_VECTORS[] = { 0x80000080, 0xBFC00180 };

	/// <summary>
	/// The CAUSE register bit 
	/// layout
	/// </summary>
	union CAUSE {
#pragma pack(push, 1)
		struct {
			bool : 2;
			Excode exception_code : 5;
			bool : 1;
			/// <summary>
			/// Bit 8/9 are for manual int requests
			/// and bit 10 is a latch
			/// </summary>
			u8 interrupt_pending : 8;
			u8 : 8;
			u8 : 0;
			u8 : 4;
			u8 cop_number : 2;
			bool : 1;
			bool branch_delay : 1; //Set if exception happens in BD
		};
#pragma pack(pop)

		u32 reg;
	};

	/// <summary>
	/// System Status register layout
	/// </summary>
	union SystemStatus {
#pragma pack(push, 1)
		struct {
			bool curr_int_enable : 1;
			bool current_mode : 1;
			bool prev_int_enable : 1;
			bool prev_mode : 1;
			bool old_int_enable : 1;
			bool old_mode : 1;
			u8 : 2;

			//Only bits [8, 10] are considered
			u8 int_mask : 8;

			bool isolate_cache : 1; //Use only data cache for load/stores
			bool swap_cache : 1;
			bool cache_zero_parity : 1;
			bool isolated_load_successfull : 1;
			bool cache_parity_error : 1;
			bool tlb_error : 1;
			bool boot_exception_vectors_location : 1;
			bool : 1;

			bool : 1;
			bool reverse_endiannes : 1;
			bool : 1;
			bool : 1;
			bool cop0_enable : 1;
			bool : 1;
			bool cop2_enable : 1;
			bool : 1;
		};
#pragma pack(pop)

		u32 reg;
	};

	/// <summary>
	/// The break control register
	/// </summary>
	union BreakControl {
#pragma pack(push, 1)
		struct {
			//Set automatically on any break
			bool did_break : 1;
			//Set on BPC break
			bool code_break : 1;
			//Set on data break
			bool data_break : 1;
			//Set on data read break
			bool data_rd_break : 1;
			//Set on data write break
			bool data_wr_break : 1;
			//Set on jump break
			bool jump_break : 1;
			u8 : 2;

			u8 : 4;
			u8 jump_redirect : 2;
			u8 : 2;

			u8 : 7;
			bool master_enable_1 : 1;

			bool enable_exec_break : 1;
			bool enable_data_break : 1;
			bool enable_read_break : 1;
			bool enable_write_break : 1;
			bool enable_jump_break : 1;
			bool enable_bit28 : 1;
			bool enable_bits_24_27 : 1;
			bool master_enable_2 : 1;
		};
#pragma pack(pop)

		u32 reg;
	};

	/// <summary>
	/// Strcture used for accessing
	/// cop0 registers, by name 
	/// or by index
	/// </summary>
	union cop0_registers {
#pragma pack(push, 1)
		struct {
			//Unused registers
			u32 r0;
			u32 r1;
			u32 r2;
			//Breakpoint On Execute
			u32 bpc;
			//Unused
			u32 r4;
			//Breakpoint On Data Access
			u32 bda;
			//Random jump address (unused)
			u32 jumpdest;
			//Break control
			BreakControl dcic;
			//Bad virtual address
			u32 badvaddr;
			//Data access breakpoint mask
			u32 bdam;
			//Unused
			u32 r10;
			//Execute breakpoint mask
			u32 bpcm;
			//System status
			SystemStatus sr;
			//CAUSE
			CAUSE cause;
			//Return address from trap
			u32 epc;
			//Processor ID
			u32 prid;
		};
#pragma pack(pop)

		u32 regs[16];
	};


	constexpr bool READABLE_COP0_REGS[16] = {
		false,
		false, 
		false,
		true,
		false,
		true,
		true,
		true,
		true,
		true,
		false,
		true,
		true,
		true,
		true,
		true
	};

	constexpr bool WRITEABLE_COP0_REGS[16] = {
		false,
		false,
		false,
		true,
		false,
		true,
		false,
		true,
		false,
		true,
		false,
		true,
		true,
		true,
		false,
		false
	};

	/// <summary>
	/// Coprocessor number 0 (System Status)
	/// </summary>
	struct cop0 {
		cop0_registers registers;

		cop0() : registers{} {
			registers.prid = 0x2;
			registers.sr.current_mode = false;
			registers.sr.curr_int_enable = false;
			registers.sr.boot_exception_vectors_location = true;
		}

		/// <summary>
		/// Return from exception opcode
		/// </summary>
		void Rfe();

		/// <summary>
		/// Performs the necessary actions
		/// when an exception uccurs
		/// </summary>
		/// <param name="exception_code"></param>
		void Exception(Excode exception_code);
	};

	/*
	Interrupt execution procedure:
	- If I_STAT AND I_MASK != Zero, continue procedure
	- Set CAUSE bit 10 to 1
	- If SystemStatus current_int_enable is true and
	  SystemStatus bit 10 is set, continue procedure
	- Set CAUSE except_code to INT
	- Perform mode/int_enable actions
	- Set CAUSE EPC to the current PC
	- Check BEV flag (bit22 of SR):
	1. If 0, find exception vector in KSEG0 (80000080h)
	2. If 1, in KSEG1 (BFC00180h)
	- Depending on address found in BEV, goto that address
	*/
}