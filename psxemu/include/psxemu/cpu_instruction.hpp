#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include <array>

namespace psx::cpu {
	/// <summary>
	/// Named values for bits 26...31 | 0...5
	/// of an instruction
	/// </summary>
	enum class Opcode : u8 {
		BCONDZ,
		J,
		JAL,
		BEQ,
		BNE,
		BLEZ,
		BGTZ,
		ADDI,
		ADDIU,
		SLTI,
		SLTIU,
		ANDI,
		ORI,
		XORI,
		LUI,
		COP0,
		COP1,
		COP2,
		COP3,
		LB,
		LH,
		LWL,
		LW,
		LBU,
		LHU,
		LWR,
		SB,
		SH,
		SWL,
		SW,
		SWR,
		LWC0,
		LWC1,
		LWC2,
		LWC3,
		SWC0,
		SWC1,
		SWC2,
		SWC3,
		SLL,
		SRL,
		SRA,
		SLLV,
		SRLV,
		SRAV,
		JR,
		JALR,
		SYSCALL,
		BREAK,
		MFHI,
		MTHI,
		MFLO,
		MTLO,
		MULT,
		MULTU,
		DIV,
		DIVU,
		ADD,
		ADDU,
		SUB,
		SUBU,
		AND,
		OR,
		XOR,
		NOR,
		SLT,
		SLTU,
		NA //Invalid
	};

	enum class InstructionType : u8 {
		RESERVED,
		LOAD,
		STORE,
		ALU,
		BRANCH,
		COPROCESSOR
	};

	/// <summary>
	/// Registers used by an instruction,
	/// both for read and write.
	/// If the register number is >= than 31,
	/// no register is used for that specific
	/// action
	/// </summary>
	struct InstructionUsedRegs {
		u8 rs;
		u8 rt;
		u8 rd;
	};

	/// <summary>
	/// Creates a static lookup table by performing 
	/// a simple loop in the range [0, 4095] and decoding
	/// the resulting counter value.
	/// 
	/// Bits [0, 5] are the secondary opcode field
	/// Upper bits are the primary opcode
	/// </summary>
	/// <returns>Static instruction lookup table</returns>
	static constexpr std::array<std::pair<InstructionType, Opcode>, 4096> GetInstructionLut() {
		std::array<std::pair<InstructionType, Opcode>, 4096> lut = {};

		for (auto& value : lut) {
			value = std::pair{ InstructionType::RESERVED, Opcode::NA };
		}

		for (u32 index = 0; index < 4096; index++) {
			u32 secondary = index & 0x3F;
			u32 primary = (index >> 6) & 0x3F;

			if (primary == 0x00) {
				auto& ref = lut[index];

				//Special opcode
				switch (secondary)
				{
				case 0x00:
					ref = std::pair{ InstructionType::ALU, Opcode::SLL };
					break;
				case 0x02:
					ref = std::pair{ InstructionType::ALU, Opcode::SRL };
					break;
				case 0x03:
					ref = std::pair{ InstructionType::ALU, Opcode::SRA };
					break;
				case 0x04:
					ref = std::pair{ InstructionType::ALU, Opcode::SLLV };
					break;
				case 0x06:
					ref = std::pair{ InstructionType::ALU, Opcode::SRLV };
					break;
				case 0x07:
					ref = std::pair{ InstructionType::ALU, Opcode::SRAV };
					break;
				case 0x08:
					ref = std::pair{ InstructionType::BRANCH, Opcode::JR };
					break;
				case 0x09:
					ref = std::pair{ InstructionType::BRANCH, Opcode::JALR };
					break;
				case 0x0C:
					ref = std::pair{ InstructionType::BRANCH, Opcode::SYSCALL };
					break;
				case 0x0D:
					ref = std::pair{ InstructionType::BRANCH, Opcode::BREAK };
					break;
				case 0x10:
					ref = std::pair{ InstructionType::ALU, Opcode::MFHI };
					break;
				case 0x11:
					ref = std::pair{ InstructionType::ALU, Opcode::MTHI };
					break;
				case 0x12:
					ref = std::pair{ InstructionType::ALU, Opcode::MFLO };
					break;
				case 0x13:
					ref = std::pair{ InstructionType::ALU, Opcode::MTLO };
					break;
				case 0x18:
					ref = std::pair{ InstructionType::ALU, Opcode::MULT };
					break;
				case 0x19:
					ref = std::pair{ InstructionType::ALU, Opcode::MULTU };
					break;
				case 0x1A:
					ref = std::pair{ InstructionType::ALU, Opcode::DIV };
					break;
				case 0x1B:
					ref = std::pair{ InstructionType::ALU, Opcode::DIVU };
					break;
				case 0x20:
					ref = std::pair{ InstructionType::ALU, Opcode::ADD };
					break;
				case 0x21:
					ref = std::pair{ InstructionType::ALU, Opcode::ADDU };
					break;
				case 0x22:
					ref = std::pair{ InstructionType::ALU, Opcode::SUB };
					break;
				case 0x23:
					ref = std::pair{ InstructionType::ALU, Opcode::SUBU };
					break;
				case 0x24:
					ref = std::pair{ InstructionType::ALU, Opcode::AND };
					break;
				case 0x25:
					ref = std::pair{ InstructionType::ALU, Opcode::OR };
					break;
				case 0x26:
					ref = std::pair{ InstructionType::ALU, Opcode::XOR };
					break;
				case 0x27:
					ref = std::pair{ InstructionType::ALU, Opcode::NOR };
					break;
				case 0x2A:
					ref = std::pair{ InstructionType::ALU, Opcode::SLT };
					break;
				case 0x2B:
					ref = std::pair{ InstructionType::ALU, Opcode::SLTU };
					break;
				default:
					break;
				}
			}
			else {
				auto& ref = lut[index];

				switch (primary)
				{
				case 0x1:
					ref = std::pair{ InstructionType::BRANCH, Opcode::BCONDZ };
					break;
				case 0x2:
					ref = std::pair{ InstructionType::BRANCH, Opcode::J };
					break;
				case 0x3:
					ref = std::pair{ InstructionType::BRANCH, Opcode::JAL };
					break;
				case 0x4:
					ref = std::pair{ InstructionType::BRANCH, Opcode::BEQ };
					break;
				case 0x5:
					ref = std::pair{ InstructionType::BRANCH, Opcode::BNE };
					break;
				case 0x6:
					ref = std::pair{ InstructionType::BRANCH, Opcode::BLEZ };
					break;
				case 0x7:
					ref = std::pair{ InstructionType::BRANCH, Opcode::BGTZ };
					break;
				case 0x8:
					ref = std::pair{ InstructionType::ALU, Opcode::ADDI };
					break;
				case 0x9:
					ref = std::pair{ InstructionType::ALU, Opcode::ADDIU };
					break;
				case 0xA:
					ref = std::pair{ InstructionType::ALU, Opcode::SLTI };
					break;
				case 0xB:
					ref = std::pair{ InstructionType::ALU, Opcode::SLTIU };
					break;
				case 0xC:
					ref = std::pair{ InstructionType::ALU, Opcode::ANDI };
					break;
				case 0xD:
					ref = std::pair{ InstructionType::ALU, Opcode::ORI };
					break;
				case 0xE:
					ref = std::pair{ InstructionType::ALU, Opcode::XORI };
					break;
				case 0xF:
					ref = std::pair{ InstructionType::ALU, Opcode::LUI };
					break;
				case 0x10:
					ref = std::pair{ InstructionType::COPROCESSOR, Opcode::COP0 };
					break;
				case 0x11:
					ref = std::pair{ InstructionType::COPROCESSOR, Opcode::COP1 };
					break;
				case 0x12:
					ref = std::pair{ InstructionType::COPROCESSOR, Opcode::COP2 };
					break;
				case 0x13:
					ref = std::pair{ InstructionType::COPROCESSOR, Opcode::COP3 };
					break;
				case 0x20:
					ref = std::pair{ InstructionType::LOAD, Opcode::LB };
					break;
				case 0x21:
					ref = std::pair{ InstructionType::LOAD, Opcode::LH };
					break;
				case 0x22:
					ref = std::pair{ InstructionType::LOAD, Opcode::LWL };
					break;
				case 0x23:
					ref = std::pair{ InstructionType::LOAD, Opcode::LW };
					break;
				case 0x24:
					ref = std::pair{ InstructionType::LOAD, Opcode::LBU };
					break;
				case 0x25:
					ref = std::pair{ InstructionType::LOAD, Opcode::LHU };
					break;
				case 0x26:
					ref = std::pair{ InstructionType::LOAD, Opcode::LWR };
					break;
				case 0x28:
					ref = std::pair{ InstructionType::STORE, Opcode::SB };
					break;
				case 0x29:
					ref = std::pair{ InstructionType::STORE, Opcode::SH };
					break;
				case 0x2A:
					ref = std::pair{ InstructionType::STORE, Opcode::SWL };
					break;
				case 0x2B:
					ref = std::pair{ InstructionType::STORE, Opcode::SW };
					break;
				case 0x2E:
					ref = std::pair{ InstructionType::STORE, Opcode::SWR };
					break;
				case 0x30:
					ref = std::pair{ InstructionType::COPROCESSOR, Opcode::LWC0 };
					break;
				case 0x31:
					ref = std::pair{ InstructionType::COPROCESSOR, Opcode::LWC1 };
					break;
				case 0x32:
					ref = std::pair{ InstructionType::COPROCESSOR, Opcode::LWC2 };
					break;
				case 0x33:
					ref = std::pair{ InstructionType::COPROCESSOR, Opcode::LWC3 };
					break;
				case 0x38:
					ref = std::pair{ InstructionType::COPROCESSOR, Opcode::SWC0 };
					break;
				case 0x39:
					ref = std::pair{ InstructionType::COPROCESSOR, Opcode::SWC1 };
					break;
				case 0x3A:
					ref = std::pair{ InstructionType::COPROCESSOR, Opcode::SWC2 };
					break;
				case 0x3B:
					ref = std::pair{ InstructionType::COPROCESSOR, Opcode::SWC3 };
					break;
				default:
					break;
				}
			}
		}

		return lut;
	}

	static constexpr std::array<std::pair<InstructionType, Opcode>, 4096> INSTRUCTION_TYPE_LUT = GetInstructionLut();
}