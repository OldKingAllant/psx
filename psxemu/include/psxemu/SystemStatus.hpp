#pragma once

#include <psxemu/include/psxemu/MIPS1.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>

namespace psx {
	/// <summary>
	/// Describes the cause of a JIT block
	/// exit
	/// </summary>
	enum class ExitCause {
		BLOCK_END,
		JUMP,
		EXCEPTION,
		EVENT
	};

	class SystemBus;

	/// <summary>
	/// This structure contains redunant data,
	/// which is necessary to keep the interpreter
	/// and JIT in sync
	/// </summary>
	struct system_status {
		SystemBus* sysbus;
		cpu::MIPS1* cpu;
		bool branch_delay;
		u32 branch_dest;
		u8 load_delay_countdown;
		u8 load_delay_dest;
		u32 delay_value;
		bool curr_mode;
		u32 jit_pc; //Use with JIT
		u32 badvaddr;
		ExitCause exit_cause;
		cpu::Excode exception_number;
		bool exception;
		bool branch;

		u32 interrupt_mask;
		u32 interrupt_request;

		void Exception(cpu::Excode excode, bool cop0_break) {
			auto& cop0 = cpu->GetCOP0();

			cop0.Exception(excode);
			cop0.registers.cause.branch_delay = branch_delay;
			curr_mode = cop0.registers.sr.current_mode;

			if (branch_delay)
				cop0.registers.epc = cpu->GetPc() - 0x4;
			else
				cop0.registers.epc = cpu->GetPc();

			if (excode == cpu::Excode::ADEL ||
				excode == cpu::Excode::ADES)
				cop0.registers.badvaddr = badvaddr;

			if (excode == cpu::Excode::BP && cop0_break)
				cpu->GetPc() = (u32)cpu::BREAK_VECTORS[cop0.registers.sr.boot_exception_vectors_location];
			else
				cpu->GetPc() = (u32)cpu::GENERAL_VECTORS[cop0.registers.sr.boot_exception_vectors_location];
		}

		void ExitException() {
			auto& cop0 = cpu->GetCOP0();

			cop0.Rfe();
			curr_mode = cop0.registers.sr.current_mode;
		}

		void Jump(u32 destination) {
			branch_dest = destination;
			branch_delay = true;
		}
	};
}