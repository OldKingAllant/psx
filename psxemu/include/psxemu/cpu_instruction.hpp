#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include <array>
#include <tuple>

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
	/// More specific instruction type
	/// </summary>
	enum class InstructionSubtype : u8 {
		SHIFT_IMM,
		SHIFT_REG,
		JUMP,
		SYS_BRK,
		MUL_DIV,
		ALU_REG,
		BRANCH,
		ALU_IMM,
		LUI_IMM,
		LOAD,
		STORE,
		COP_CMD,
		COP_LOAD,
		COP_STORE,
		NA
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
	static constexpr std::array<std::tuple<InstructionType, Opcode, InstructionSubtype>, 4096> GetInstructionLut() {
		std::array<std::tuple<InstructionType, Opcode, InstructionSubtype>, 4096> lut = {};

		using tuple_ty = std::tuple<InstructionType, Opcode, InstructionSubtype>;

		for (auto& value : lut) {
			value = tuple_ty{ InstructionType::RESERVED, Opcode::NA, InstructionSubtype::NA };
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
					ref = tuple_ty{ InstructionType::ALU, Opcode::SLL, InstructionSubtype::SHIFT_IMM };
					break;
				case 0x02:
					ref = tuple_ty{ InstructionType::ALU, Opcode::SRL, InstructionSubtype::SHIFT_IMM };
					break;
				case 0x03:
					ref = tuple_ty{ InstructionType::ALU, Opcode::SRA, InstructionSubtype::SHIFT_IMM };
					break;
				case 0x04:
					ref = tuple_ty{ InstructionType::ALU, Opcode::SLLV, InstructionSubtype::SHIFT_REG };
					break;
				case 0x06:
					ref = tuple_ty{ InstructionType::ALU, Opcode::SRLV, InstructionSubtype::SHIFT_REG };
					break;
				case 0x07:
					ref = tuple_ty{ InstructionType::ALU, Opcode::SRAV, InstructionSubtype::SHIFT_REG };
					break;
				case 0x08:
					ref = tuple_ty{ InstructionType::BRANCH, Opcode::JR, InstructionSubtype::JUMP };
					break;
				case 0x09:
					ref = tuple_ty{ InstructionType::BRANCH, Opcode::JALR, InstructionSubtype::JUMP };
					break;
				case 0x0C:
					ref = tuple_ty{ InstructionType::BRANCH, Opcode::SYSCALL, InstructionSubtype::SYS_BRK };
					break;
				case 0x0D:
					ref = tuple_ty{ InstructionType::BRANCH, Opcode::BREAK, InstructionSubtype::SYS_BRK };
					break;
				case 0x10:
					ref = tuple_ty{ InstructionType::ALU, Opcode::MFHI, InstructionSubtype::MUL_DIV };
					break;
				case 0x11:
					ref = tuple_ty{ InstructionType::ALU, Opcode::MTHI, InstructionSubtype::MUL_DIV };
					break;
				case 0x12:
					ref = tuple_ty{ InstructionType::ALU, Opcode::MFLO, InstructionSubtype::MUL_DIV };
					break;
				case 0x13:
					ref = tuple_ty{ InstructionType::ALU, Opcode::MTLO, InstructionSubtype::MUL_DIV };
					break;
				case 0x18:
					ref = tuple_ty{ InstructionType::ALU, Opcode::MULT, InstructionSubtype::MUL_DIV };
					break;
				case 0x19:
					ref = tuple_ty{ InstructionType::ALU, Opcode::MULTU, InstructionSubtype::MUL_DIV };
					break;
				case 0x1A:
					ref = tuple_ty{ InstructionType::ALU, Opcode::DIV, InstructionSubtype::MUL_DIV };
					break;
				case 0x1B:
					ref = tuple_ty{ InstructionType::ALU, Opcode::DIVU, InstructionSubtype::MUL_DIV };
					break;
				case 0x20:
					ref = tuple_ty{ InstructionType::ALU, Opcode::ADD, InstructionSubtype::ALU_REG };
					break;
				case 0x21:
					ref = tuple_ty{ InstructionType::ALU, Opcode::ADDU, InstructionSubtype::ALU_REG };
					break;
				case 0x22:
					ref = tuple_ty{ InstructionType::ALU, Opcode::SUB, InstructionSubtype::ALU_REG };
					break;
				case 0x23:
					ref = tuple_ty{ InstructionType::ALU, Opcode::SUBU, InstructionSubtype::ALU_REG };
					break;
				case 0x24:
					ref = tuple_ty{ InstructionType::ALU, Opcode::AND, InstructionSubtype::ALU_REG };
					break;
				case 0x25:
					ref = tuple_ty{ InstructionType::ALU, Opcode::OR, InstructionSubtype::ALU_REG };
					break;
				case 0x26:
					ref = tuple_ty{ InstructionType::ALU, Opcode::XOR, InstructionSubtype::ALU_REG };
					break;
				case 0x27:
					ref = tuple_ty{ InstructionType::ALU, Opcode::NOR, InstructionSubtype::ALU_REG };
					break;
				case 0x2A:
					ref = tuple_ty{ InstructionType::ALU, Opcode::SLT, InstructionSubtype::ALU_REG };
					break;
				case 0x2B:
					ref = tuple_ty{ InstructionType::ALU, Opcode::SLTU, InstructionSubtype::ALU_REG };
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
					ref = tuple_ty{ InstructionType::BRANCH, Opcode::BCONDZ, InstructionSubtype::BRANCH };
					break;
				case 0x2:
					ref = tuple_ty{ InstructionType::BRANCH, Opcode::J, InstructionSubtype::JUMP };
					break;
				case 0x3:
					ref = tuple_ty{ InstructionType::BRANCH, Opcode::JAL, InstructionSubtype::JUMP };
					break;
				case 0x4:
					ref = tuple_ty{ InstructionType::BRANCH, Opcode::BEQ, InstructionSubtype::BRANCH };
					break;
				case 0x5:
					ref = tuple_ty{ InstructionType::BRANCH, Opcode::BNE, InstructionSubtype::BRANCH };
					break;
				case 0x6:
					ref = tuple_ty{ InstructionType::BRANCH, Opcode::BLEZ, InstructionSubtype::BRANCH };
					break;
				case 0x7:
					ref = tuple_ty{ InstructionType::BRANCH, Opcode::BGTZ, InstructionSubtype::BRANCH };
					break;
				case 0x8:
					ref = tuple_ty{ InstructionType::ALU, Opcode::ADDI, InstructionSubtype::ALU_IMM };
					break;
				case 0x9:
					ref = tuple_ty{ InstructionType::ALU, Opcode::ADDIU, InstructionSubtype::ALU_IMM };
					break;
				case 0xA:
					ref = tuple_ty{ InstructionType::ALU, Opcode::SLTI, InstructionSubtype::ALU_IMM };
					break;
				case 0xB:
					ref = tuple_ty{ InstructionType::ALU, Opcode::SLTIU, InstructionSubtype::ALU_IMM };
					break;
				case 0xC:
					ref = tuple_ty{ InstructionType::ALU, Opcode::ANDI, InstructionSubtype::ALU_IMM };
					break;
				case 0xD:
					ref = tuple_ty{ InstructionType::ALU, Opcode::ORI, InstructionSubtype::ALU_IMM };
					break;
				case 0xE:
					ref = tuple_ty{ InstructionType::ALU, Opcode::XORI, InstructionSubtype::ALU_IMM };
					break;
				case 0xF:
					ref = tuple_ty{ InstructionType::ALU, Opcode::LUI, InstructionSubtype::LUI_IMM };
					break;
				case 0x10:
					ref = tuple_ty{ InstructionType::COPROCESSOR, Opcode::COP0, InstructionSubtype::COP_CMD };
					break;
				case 0x11:
					ref = tuple_ty{ InstructionType::COPROCESSOR, Opcode::COP1, InstructionSubtype::COP_CMD };
					break;
				case 0x12:
					ref = tuple_ty{ InstructionType::COPROCESSOR, Opcode::COP2, InstructionSubtype::COP_CMD };
					break;
				case 0x13:
					ref = tuple_ty{ InstructionType::COPROCESSOR, Opcode::COP3, InstructionSubtype::COP_CMD };
					break;
				case 0x20:
					ref = tuple_ty{ InstructionType::LOAD, Opcode::LB, InstructionSubtype::LOAD };
					break;
				case 0x21:
					ref = tuple_ty{ InstructionType::LOAD, Opcode::LH, InstructionSubtype::LOAD };
					break;
				case 0x22:
					ref = tuple_ty{ InstructionType::LOAD, Opcode::LWL, InstructionSubtype::LOAD };
					break;
				case 0x23:
					ref = tuple_ty{ InstructionType::LOAD, Opcode::LW, InstructionSubtype::LOAD };
					break;
				case 0x24:
					ref = tuple_ty{ InstructionType::LOAD, Opcode::LBU, InstructionSubtype::LOAD };
					break;
				case 0x25:
					ref = tuple_ty{ InstructionType::LOAD, Opcode::LHU, InstructionSubtype::LOAD };
					break;
				case 0x26:
					ref = tuple_ty{ InstructionType::LOAD, Opcode::LWR, InstructionSubtype::LOAD };
					break;
				case 0x28:
					ref = tuple_ty{ InstructionType::STORE, Opcode::SB, InstructionSubtype::STORE };
					break;
				case 0x29:
					ref = tuple_ty{ InstructionType::STORE, Opcode::SH, InstructionSubtype::STORE };
					break;
				case 0x2A:
					ref = tuple_ty{ InstructionType::STORE, Opcode::SWL, InstructionSubtype::STORE };
					break;
				case 0x2B:
					ref = tuple_ty{ InstructionType::STORE, Opcode::SW, InstructionSubtype::STORE };
					break;
				case 0x2E:
					ref = tuple_ty{ InstructionType::STORE, Opcode::SWR, InstructionSubtype::STORE };
					break;
				case 0x30:
					ref = tuple_ty{ InstructionType::COPROCESSOR, Opcode::LWC0, InstructionSubtype::COP_LOAD };
					break;
				case 0x31:
					ref = tuple_ty{ InstructionType::COPROCESSOR, Opcode::LWC1, InstructionSubtype::COP_LOAD };
					break;
				case 0x32:
					ref = tuple_ty{ InstructionType::COPROCESSOR, Opcode::LWC2, InstructionSubtype::COP_LOAD };
					break;
				case 0x33:
					ref = tuple_ty{ InstructionType::COPROCESSOR, Opcode::LWC3, InstructionSubtype::COP_LOAD };
					break;
				case 0x38:
					ref = tuple_ty{ InstructionType::COPROCESSOR, Opcode::SWC0, InstructionSubtype::COP_STORE };
					break;
				case 0x39:
					ref = tuple_ty{ InstructionType::COPROCESSOR, Opcode::SWC1, InstructionSubtype::COP_STORE };
					break;
				case 0x3A:
					ref = tuple_ty{ InstructionType::COPROCESSOR, Opcode::SWC2, InstructionSubtype::COP_STORE };
					break;
				case 0x3B:
					ref = tuple_ty{ InstructionType::COPROCESSOR, Opcode::SWC3, InstructionSubtype::COP_STORE };
					break;
				default:
					break;
				}
			}
		}

		return lut;
	}

	static constexpr std::array<std::tuple<InstructionType, Opcode, InstructionSubtype>, 4096> INSTRUCTION_TYPE_LUT = GetInstructionLut();
}