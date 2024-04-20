#pragma once

#include <psxemu/include/psxemu/KernelStructures.hpp>

#include <string>
#include <string_view>

namespace psx::kernel {
	/// <summary>
	/// For now this is not a real
	/// HLE version of the kernel...
	/// It is simply a collection
	/// of utilities for retrieving
	/// the KERNEL/BIOS status
	/// and informations 
	/// </summary>
	class Kernel {
	public :
		Kernel();

		void SetRomPointer(u8* ptr) { m_rom_pointer = ptr; }
		void SetRamPointer(u8* ptr) { m_ram_pointer = ptr; }

		/// <summary>
		/// [BIOS + 0x108] -> Kernel maker 
		/// Present in the BIOS image as 
		/// pure ASCII. No conversion/allocation
		/// necessary 
		/// </summary>
		/// <returns>String of the kernel maker</returns>
		std::string_view DumpKernelMaker() const;

		/// <summary>
		/// Same principle as the kernel maker
		/// </summary>
		/// <returns>Kernel version string</returns>
		std::string_view DumpKernelVersion() const;

		/// <summary>
		/// This will allocate a new std::string,
		/// since the kernel date is in BCD
		/// format in the BIOS image, and
		/// not in plain ASCII chars
		/// </summary>
		/// <returns></returns>
		std::string DumpKernelBcdDate() const;

		/*
		WARNING:
		All the BIOS structure dump function
		assume that the BIOS/Kernel is already
		initialized (real undefined behaviour
		if not)
		*/

		/// <summary>
		/// For the given priority, dump
		/// the exception priority chain
		/// as string
		/// </summary>
		/// <param name="priority">Wanted priority</param>
		/// <returns>String representation</returns>
		std::string DumpExceptionPriorityChain(u8 priority) const;

		/// <summary>
		/// As DumpExceptionPriorityChain, 
		/// but all priority chains are dumped
		/// </summary>
		/// <returns></returns>
		std::string DumpAllExceptionChains() const;

	private :
		u8* m_rom_pointer;
		u8* m_ram_pointer;
	};
}