#include <psxemu/include/psxemu/Kernel.hpp>

#include <bit>
#include <sstream>
#include <iomanip>
#include <fmt/format.h>

namespace psx::kernel {
	ExceptionChain Kernel::DumpExceptionPriorityChain(u8 priority) const {
		auto table_of_tables = std::bit_cast<TableOfTables*>(m_ram_pointer + TABLE_OF_TABLES_ADDRESS);

		u32 num_entries = table_of_tables->excb.size /
			ExCB_SIZE;

		if (priority >= num_entries)
			return {};

		u32 off = table_of_tables->excb.address +
			ExCB_SIZE * (u32)priority;

		auto entry = std::bit_cast<ExceptionControlBlock*>(m_ram_pointer + off);

		ExceptionChain dump{};

		u32 hard_limit = 20;

		auto element_ptr = entry->pointer;

		while (hard_limit-- && element_ptr) {
			auto element = 
				std::bit_cast<ExceptionChainEntry*>(m_ram_pointer + element_ptr);
			dump.push_back(ExceptionChainEntry{ *element });
			element_ptr = element->next;
		}

		return dump;
	}

	std::vector<ExceptionChain> Kernel::DumpAllExceptionChains() const {
		auto table_of_tables = std::bit_cast<TableOfTables*>(m_ram_pointer + TABLE_OF_TABLES_ADDRESS);

		u32 num_entries = table_of_tables->excb.size /
			ExCB_SIZE;

		std::vector<ExceptionChain> dumped{};

		for (u32 entry = 0; entry < num_entries; entry++) {
			dumped.push_back(DumpExceptionPriorityChain(entry));
		}

		return dumped;
	}

	std::string FormatExceptionChain(ExceptionChain const& chain) {
		std::ostringstream dump{};

		dump << "[";

		for (auto const& entry : chain) {
			dump << fmt::format("{}", entry) << ",";
		}

		dump << "]";
		return dump.str();
	}

	std::string FormatExceptionChains(std::vector<ExceptionChain> const& chains) {
		std::ostringstream dump{};

		dump << "{\n";

		for (auto const& chain : chains) {
			dump << FormatExceptionChain(chain) << "\n";
		}

		dump << "}";
		return dump.str();
	}
}