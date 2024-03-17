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
#ifdef DEBUG_CPU_ERRORS
		u16 primary = (instruction >> 26) & 0x3F;
		u16 secondary = instruction & 0x3F;

		u16 hash = (primary << 6) | secondary;

		auto type = INSTRUCTION_TYPE_LUT[hash];

		fmt::println("Reserved instruction 0x{:x} at 0x{:x}", instruction,
			status->cpu->GetPc());
#endif // DEBUG_CPU_ERRORS
		error::DebugBreak();
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
		else {
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
		u32 off = (u16)instruction;

		auto& regs = status->cpu->GetRegs();

		if constexpr (StoreOpcode == Opcode::SW) {
			//Perform direct write, do not emulate store
			//delay
			status->sysbus->Write<u32, true, true>(regs.array[rs] + off, regs.array[rt]);
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
			default:
				break;
			}
			return ReservedInstruction;
		}
	};
}