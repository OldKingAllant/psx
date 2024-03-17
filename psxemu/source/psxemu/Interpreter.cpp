#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/Interpreter.hpp>
#include <psxemu/include/psxemu/cpu_instruction.hpp>
#include <common/Errors.hpp>
#include <psxemu/include/psxemu/InstructionHandlers.hpp>

#include <fmt/format.h>

namespace psx::cpu {
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