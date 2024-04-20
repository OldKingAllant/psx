#include <psxemu/include/psxemu/Kernel.hpp>

#include <fmt/format.h>

/// Used for BIOS/KERNEL informations
/// and strings dumping

namespace psx::kernel {
	/*
	* NO$PSX:
	 BFC00100h Kernel BCD date  (YYYYMMDDh)
	 BFC00104h Console Type     (see Port 1F802030h, Secondary IRQ10 Controller)
	 BFC00108h Kernel Maker/Version Strings (separated by one or more 00h bytes)
	 BFC7FF32h GUI Version/Copyright Strings (if any) (separated by one 00h byte)
	*/

	/// <summary>
	/// Offset from BIOS base
	/// </summary>
	static constexpr u32 KERNEL_BCD_LOC = 0x100;
	static constexpr u32 CONSOLE_TYPE_LOC = 0x104;
	static constexpr u32 KERNEL_STRINGS_LOC = 0x108;

	std::string_view Kernel::DumpKernelMaker() const {
		u32 index{ KERNEL_STRINGS_LOC };

		//Do not use strlen here since
		//MSVC brags about it being unsafe.
		//In response to that I would use
		//strnlen, however, also THAT
		//is unsafe and I would need
		//to use strnlen_s, which
		//does not exist in other
		//implementations of the std library
		while (m_rom_pointer[index++]) {}

		return std::string_view{ (char*)m_rom_pointer + KERNEL_STRINGS_LOC, 
		index - KERNEL_STRINGS_LOC - 1 };
	}
	 
	std::string_view Kernel::DumpKernelVersion() const {
		auto maker_string = DumpKernelMaker();
		u32 index = (u32)( maker_string.size() + KERNEL_STRINGS_LOC );

		//Skip an unknown amount of \0 (at least 1) 
		while (!m_rom_pointer[index++]) {}

		u32 middle_index = index - 1;

		while (m_rom_pointer[index++]) {}

		return std::string_view{
			(char*)m_rom_pointer + middle_index,
			(index - middle_index - 1)
		};
	}

	std::string Kernel::DumpKernelBcdDate() const {
		constexpr u32 BCD_SIZE = 8;
		constexpr u32 BCD_END = KERNEL_BCD_LOC + BCD_SIZE / 2;

		char converted_date[BCD_SIZE] = {};

		u8 converted_pos{ 0 };

		for (u32 index = BCD_END - 1; index >= KERNEL_BCD_LOC; index -= 1) {
			u8 byteval = m_rom_pointer[index];
			char low_nibble = (byteval & 0xF) + '0';
			char high_nibble = ((byteval >> 4) & 0xF) + '0';
			converted_date[converted_pos++] = high_nibble;
			converted_date[converted_pos++] = low_nibble;
		}

		auto date = std::string{ converted_date, converted_date + 4 }
			+ "/" + std::string{converted_date + 4, converted_date + 6}
			+ "/" + std::string{converted_date + 6, std::end(converted_date)};

		return date;
	}
}