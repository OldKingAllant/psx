#include <psxemu/include/psxemu/GPU.hpp>
#include <psxemu/include/psxemu/GPUCommands.hpp>

#include <fmt/format.h>

#include <common/Errors.hpp>

namespace psx {

	void Gpu::EnvCommand(u32 cmd) {
		u8 upper_byte = (u8)(cmd >> 24);

		switch (upper_byte)
		{
		case 0xE1:
			Texpage(cmd);
			break;
		default:
			fmt::println("[GPU] Unimplemented ENV command 0x{:x}", (u32)upper_byte);
			error::DebugBreak();
			break;
		}
	}

	void Gpu::Texpage(u32 cmd) {

	}

	void Gpu::CommandStart(u32 cmd) {
		CommandType cmd_type = (CommandType)((cmd >> 29) & 0x7);
	
		switch (cmd_type)
		{
		case psx::CommandType::MISC:
			break;
		case psx::CommandType::POLYGON:
			break;
		case psx::CommandType::LINE:
			break;
		case psx::CommandType::RECTANGLE:
			break;
		case psx::CommandType::VRAM_BLIT:
			break;
		case psx::CommandType::CPU_VRAM_BLIT:
			break;
		case psx::CommandType::VRAM_CPU_BLIT:
			break;
		case psx::CommandType::ENV:
			EnvCommand(cmd);
			break;
		default:
			break;
		}
	}
}