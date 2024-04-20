#include <psxemu/include/psxemu/Kernel.hpp>

#include <bit>
#include <sstream>
#include <fmt/format.h>

namespace psx::kernel {
	std::string Kernel::DumpExceptionPriorityChain(u8 priority) const {
		auto table_of_tables = std::bit_cast<TableOfTables*>(m_ram_pointer + TABLE_OF_TABLES_ADDRESS);

		u32 num_entries = table_of_tables->excb.size /
			ExCB_SIZE;

		if (priority >= num_entries)
			return "INVALID_PRIORITY";

		u32 off = table_of_tables->excb.address +
			ExCB_SIZE * (u32)priority;

		auto entry = std::bit_cast<ExceptionControlBlock*>(m_ram_pointer + off);

		return fmt::format("Entry address : 0x{:x}", off);
	}

	std::string Kernel::DumpAllExceptionChains() const {
		auto table_of_tables = std::bit_cast<TableOfTables*>(m_ram_pointer + TABLE_OF_TABLES_ADDRESS);

		u32 num_entries = table_of_tables->excb.size /
			ExCB_SIZE;

		std::ostringstream dumped{};

		for (u32 entry = 0; entry < num_entries; entry++) {
			dumped << "Exception priority " << entry << "\n";
			dumped << DumpExceptionPriorityChain(entry);
		}

		return dumped.str();
	}
}