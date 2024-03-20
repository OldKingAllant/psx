#pragma once

#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/Interpreter.hpp>
#include <psxemu/include/psxemu/cpu_instruction.hpp>
#include <common/Errors.hpp>

#include <fmt/format.h>

namespace psx::cpu {
	//LUI <reg>, <imm16> load immediate value to upper 16
	//bits of registers <reg>
	void LoadUpperImmediate(system_status* status, u32 instruction) {
		u32 dest_reg = (instruction >> 16) & 0x1F;

		if (dest_reg != 0) {
			u32 imm = (u16)instruction;

			u32& reg = status->cpu->GetRegs().array[dest_reg];

			//Clear and assign upper part
			reg &= 0xFFFF0000;
			reg |= (imm << 16);
		}
	}

	void ReservedInstruction(system_status* status, u32 instruction) {
		u16 primary = (instruction >> 26) & 0x3F;
		u16 secondary = instruction & 0x3F;

		u16 hash = (primary << 6) | secondary;

		auto const& type = INSTRUCTION_TYPE_LUT[hash];

		if (std::get<1>(type) == Opcode::NA) {
#ifdef DEBUG_CPU_ERRORS
			fmt::println("Reserved instruction 0x{:x} at 0x{:x}", instruction,
				status->cpu->GetPc());
#endif // DEBUG_CPU_ERRORS

			status->exception = true;
			status->exception_number = Excode::RI;
		}
		else {
			fmt::println("Unimplemented instruction 0x{:x} at 0x{:x}", instruction,
				status->cpu->GetPc());

			error::DebugBreak();
		}
	}

	template <Opcode AluOpcode>
	void AluImmediate(system_status* status, u32 instruction) {
		u32 rs = (instruction >> 21) & 0x1F;
		u32 rt = (instruction >> 16) & 0x1F;
		u32 imm = (u16)instruction;

		auto& regs = status->cpu->GetRegs();

		if constexpr (AluOpcode == Opcode::ORI) {
			regs.array[rt] = regs.array[rs] | imm;
		}
		else if constexpr (AluOpcode == Opcode::ADDIU) {
			regs.array[rt] = regs.array[rs] + imm;
		}
		else {
			fmt::println("Invalid/Unimplemented ALU Immediate Opcode 0x{:x}", (u8)AluOpcode);
			error::DebugBreak();
		}
	}

	template <Opcode ShiftOpcode>
	void ShiftImmediate(system_status* status, u32 instruction) {
		u32 rt = (instruction >> 16) & 0x1F;
		u32 rd = (instruction >> 11) & 0x1F;
		u32 imm = (instruction >> 6) & 0x1F;

		auto& regs = status->cpu->GetRegs();

		if (ShiftOpcode == Opcode::SLL) {
			regs.array[rd] = regs.array[rt] << imm;
		}
		else {
			error::DebugBreak();
		}
	}

	template <Opcode StoreOpcode>
	void Store(system_status* status, u32 instruction) {
		u32 rs = (instruction >> 21) & 0x1F;
		u32 rt = (instruction >> 16) & 0x1F;
		i32 off = (i16)(instruction & 0xFFFF);

		auto& regs = status->cpu->GetRegs();

		u32 base = (rs == 0) ? 0 : regs.array[rs];
		u32 value = (rt == 0) ? 0 : regs.array[rt];

		if constexpr (StoreOpcode == Opcode::SW) {
			//Perform direct write, do not emulate store
			//delay
			status->sysbus->Write<u32, true, true>((u32)(base + off), value);
		}
		else {
			error::DebugBreak();
		}
	}

	template <Opcode JumpOpcode>
	void Jump(system_status* status, u32 instruction) {
		//pc=(pc and F0000000h)+(imm26bit*4)

		if constexpr (JumpOpcode == Opcode::J) {
			u32 offset = (instruction & 0x3FFFFFF) << 2;
			u32 base = status->cpu->GetPc() & 0xF0000000;
			status->Jump(base + offset);
		}
		else {
			error::DebugBreak();
		}
	}

	template <u16 Instruction>
	struct InstructionDecoder {
		static constexpr auto TYPE = INSTRUCTION_TYPE_LUT[Instruction];

		static constexpr InstructionHandler GetHandler() {
			switch (std::get<2>(TYPE)) {
			case InstructionSubtype::LUI_IMM:
				return LoadUpperImmediate;
			case InstructionSubtype::ALU_IMM: {
				constexpr auto opcode = std::get<1>(TYPE);

				if constexpr (opcode != Opcode::NA) 
					return AluImmediate<opcode>;
			}
			break;
			case InstructionSubtype::STORE: {
				constexpr auto opcode = std::get<1>(TYPE);

				if constexpr (opcode != Opcode::NA)
					return Store<opcode>;
			}
			break;
			case InstructionSubtype::SHIFT_IMM: {
				constexpr auto opcode = std::get<1>(TYPE);

				if constexpr (opcode != Opcode::NA)
					return ShiftImmediate<opcode>;
			}
			break;
			case InstructionSubtype::JUMP: {
				constexpr auto opcode = std::get<1>(TYPE);

				if constexpr (opcode != Opcode::NA)
					return Jump<opcode>;
			}
			break;
			default:
				break;
			}
			return ReservedInstruction;
		}
	};
}