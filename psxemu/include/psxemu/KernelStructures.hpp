#pragma once

#include <common/Defs.hpp>

#include <psxemu/include/psxemu/KernelEvents.hpp>
#include <psxemu/include/psxemu/KernelExceptions.hpp>
#include <psxemu/include/psxemu/KernelThreads.hpp>
#include <psxemu/include/psxemu/KernelDevices.hpp>
#include <psxemu/include/psxemu/KernelFiles.hpp>

#include <fmt/core.h>
#include <fmt/format.h>
#include <string>

namespace psx::kernel {
	static constexpr u32 TABLE_OF_TABLES_ADDRESS = 0x100;

	union TableOfTablesEntry {
#pragma pack(push, 1)
		struct {
			u32 address;
			u32 size;
		};
#pragma pack(pop)

		u64 raw_entry;
	};

	struct TableOfTables {
#pragma pack(push, 8)
		TableOfTablesEntry excb;
		TableOfTablesEntry pcb;
		TableOfTablesEntry tcbs;
		TableOfTablesEntry _unused0;
		TableOfTablesEntry ecbs;
		TableOfTablesEntry _unused1;
		TableOfTablesEntry _unused2;
		TableOfTablesEntry _unused3;
		TableOfTablesEntry fcbs;
		TableOfTablesEntry _unused4;
		TableOfTablesEntry dcbs;
#pragma pack(pop)
	};
}

template <>
struct fmt::formatter<psx::kernel::ExceptionChainEntry> {
	constexpr auto parse(format_parse_context& ctx)
		-> format_parse_context::iterator {
		return ctx.begin();
	}
	
	auto format(psx::kernel::ExceptionChainEntry const& entry, fmt::format_context& ctx) const {
		return fmt::format_to(ctx.out(), "(first = {:#x}, second = {:#x}, next = {:#x})",
			entry.first_function, entry.second_function, entry.next);
	}
};