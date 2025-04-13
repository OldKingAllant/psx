#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/Interpreter.hpp>
#include <psxemu/include/psxemu/cpu_instruction.hpp>
#include <common/Errors.hpp>
#include <psxemu/include/psxemu/InstructionHandlers.hpp>

#include <fmt/format.h>

namespace psx::cpu {
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
			case InstructionSubtype::ALU_REG: {
				constexpr auto opcode = std::get<1>(TYPE);

				if constexpr (opcode != Opcode::NA)
					return AluReg<opcode>;
			}
			break;
			case InstructionSubtype::COP_CMD: {
				constexpr auto opcode = std::get<1>(TYPE);

				if constexpr (opcode != Opcode::NA)
					return CopCmd<opcode>;
			}
			break;
			case InstructionSubtype::BRANCH: {
				constexpr auto opcode = std::get<1>(TYPE);

				if constexpr (opcode != Opcode::NA)
					return Branch<opcode>;
			}
			break;
			case InstructionSubtype::LOAD: {
				constexpr auto opcode = std::get<1>(TYPE);

				if constexpr (opcode != Opcode::NA)
					return Load<opcode>;
			}
			break;
			case InstructionSubtype::MUL_DIV: {
				constexpr auto opcode = std::get<1>(TYPE);

				if constexpr (opcode != Opcode::NA)
					return MulDiv<opcode>;
			}
			break;
			case InstructionSubtype::SYS_BRK: {
				constexpr auto opcode = std::get<1>(TYPE);

				if constexpr (opcode != Opcode::NA)
					return SysBreak<opcode>;
			}
			break;
			case InstructionSubtype::SHIFT_REG: {
				constexpr auto opcode = std::get<1>(TYPE);

				if constexpr (opcode != Opcode::NA)
					return ShiftReg<opcode>;
			}
			break;
			case InstructionSubtype::COP_LOAD: {
				constexpr auto opcode = std::get<1>(TYPE);

				if constexpr (opcode != Opcode::NA)
					return CoprocessorLoad<opcode>;
			}
			break;
			case InstructionSubtype::COP_STORE: {
				constexpr auto opcode = std::get<1>(TYPE);

				if constexpr (opcode != Opcode::NA)
					return CoprocessorStore<opcode>;
			}
			break;
			default:
				break;
			}
			return ReservedInstruction;
		}
	};

	struct DecodeHelper {
		template <std::size_t... Seq>
		static constexpr std::array<InstructionHandler, 4096> GetHandlersImpl(std::index_sequence<Seq...>) {
			return {
				InstructionDecoder<Seq>::GetHandler()...
			};
		}
	};

	struct DecodeSeq {
		static constexpr std::array<InstructionHandler, 4096> GetHandlers() {
			return DecodeHelper::GetHandlersImpl(
				std::make_index_sequence<4096>{}
			);
		}
	};

	std::array<InstructionHandler, 4096> MIPS_HANDLERS = DecodeSeq::GetHandlers();

	void InterpretMips(system_status* system, u32 instruction) {
		u16 primary = (instruction >> 26) & 0x3F;
		u16 secondary = instruction & 0x3F;

		u16 hash = (primary << 6) | secondary;

		MIPS_HANDLERS[hash](system, instruction);
	}
}