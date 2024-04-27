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

	std::vector<EventControlBlock> Kernel::DumpEventControlBlocks() const {
		auto table_of_tables = std::bit_cast<TableOfTables*>(m_ram_pointer + TABLE_OF_TABLES_ADDRESS);

		u32 base = table_of_tables->ecbs.address;
		u32 num_evcb = table_of_tables->ecbs.size /
			sizeof(EventControlBlock);

		std::vector<EventControlBlock> dumped{};

		dumped.reserve(num_evcb);

		for (u32 index = 0; index < num_evcb; index++) {
			u32 address = base + index * sizeof(EventControlBlock);
			auto evcb = std::bit_cast<EventControlBlock*>(
				m_ram_pointer + address
			);

			if(evcb->status != EventStatus::FREE)
				dumped.push_back(EventControlBlock{ *evcb });
		}

		return dumped;
	}

	u32 Kernel::GetCurrentThread() const {
		auto table_of_tables = std::bit_cast<TableOfTables*>(m_ram_pointer + TABLE_OF_TABLES_ADDRESS);
		auto address = table_of_tables->pcb.address;
		auto base = table_of_tables->tcbs.address;

		return (*std::bit_cast<u32*>(m_ram_pointer + address) - base) / (u32)sizeof(ThreadControlBlock);
	}

	std::vector<ThreadControlBlock> Kernel::DumpThreadControlBlocks() const {
		auto table_of_tables = std::bit_cast<TableOfTables*>(m_ram_pointer + TABLE_OF_TABLES_ADDRESS);

		auto base = table_of_tables->tcbs.address;
		auto num_tcb = table_of_tables->tcbs.size /
			sizeof(ThreadControlBlock);

		std::vector<ThreadControlBlock> dumped{};
		dumped.reserve(num_tcb);

		for (u32 index = 0; index < num_tcb; index++) {
			u32 address = base + index * sizeof(ThreadControlBlock);
			auto tcb = std::bit_cast<ThreadControlBlock*>(
				m_ram_pointer + address
			);

			if (tcb->status != TCBStatus::FREE)
				dumped.push_back(*tcb);
		}

		return dumped;
	}

	std::vector<DeviceControlBlock> Kernel::DumpDeviceControlBlocks() const {
		auto table_of_tables = std::bit_cast<TableOfTables*>(m_ram_pointer + TABLE_OF_TABLES_ADDRESS);

		auto base = table_of_tables->dcbs.address;
		auto num_dcb = table_of_tables->dcbs.size
			/ sizeof(DeviceControlBlock);

		std::vector<DeviceControlBlock> dumped{};
		dumped.reserve(num_dcb);

		for (u32 index = 0; index < num_dcb; index++) {
			u32 address = base + index * sizeof(DeviceControlBlock);
			auto dcb = std::bit_cast<DeviceControlBlock*>(
				m_ram_pointer + address
			);

			if (dcb->lowercase_name_ptr)
				dumped.push_back(*dcb);
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

	std::string_view EventClassName(EventClass evclass) {
		switch (evclass)
		{
		case psx::kernel::EventClass::VBLANK:
			return "VBLANK";
			break;
		case psx::kernel::EventClass::GPU_IRQ:
			return "GPU-IRQ";
			break;
		case psx::kernel::EventClass::CDROM:
			return "CDROM";
			break;
		case psx::kernel::EventClass::DMA:
			return "DMA";
			break;
		case psx::kernel::EventClass::RTC0:
			return "RTC0";
			break;
		case psx::kernel::EventClass::RTC1:
			return "RTC1";
			break;
		case psx::kernel::EventClass::CONTROLLER:
			return "CONTROLLER";
			break;
		case psx::kernel::EventClass::SPU:
			return "SPU";
			break;
		case psx::kernel::EventClass::PIO:
			return "PIO";
			break;
		case psx::kernel::EventClass::SIO:
			return "SIO";
			break;
		case psx::kernel::EventClass::CPU_EXCEPTION:
			return "CPU-EXCEPTION";
			break;
		case psx::kernel::EventClass::COUNTER0:
			return "COUNTER0";
			break;
		case psx::kernel::EventClass::COUNTER1:
			return "COUNTER1";
			break;
		case psx::kernel::EventClass::COUNTER2:
			return "COUNTER2";
			break;
		case psx::kernel::EventClass::COUNTER3:
			return "COUNTER3";
			break;
		default:
			return "<INVALID/IDK>";
			break;
		}
	}

	std::string_view EventStatusName(EventStatus status) {
		switch (status)
		{
		case psx::kernel::EventStatus::FREE:
			return "FREE";
			break;
		case psx::kernel::EventStatus::DISABLED:
			return "DISABLED";
			break;
		case psx::kernel::EventStatus::BUSY:
			return "BUSY";
			break;
		case psx::kernel::EventStatus::READY:
			return "READY";
			break;
		default:
			return "<INVALID>";
			break;
		}
	}

	std::string_view EventModeName(EventMode mode) {
		switch (mode)
		{
		case psx::kernel::EventMode::EXECUTE:
			return "EXECUTE";
			break;
		case psx::kernel::EventMode::MARK_READY:
			return "MARK_READY";
			break;
		default:
			return "<INVALID>";
			break;
		}
	}

	std::string_view EventSpecName(EventSpec spec) {
		switch (spec)
		{
		case psx::kernel::EventSpec::COUNTER_BECOMES_ZERO:
			return "COUNTER BECOMES 0";
			break;
		case psx::kernel::EventSpec::INTERRUPTED:
			return "INTERRUPTED";
			break;
		case psx::kernel::EventSpec::END_OF_IO:
			return "END OF IO";
			break;
		case psx::kernel::EventSpec::FILE_CLOSED:
			return "FILE CLOSED";
			break;
		case psx::kernel::EventSpec::COMMAND_ACK:
			return "COMMAND ACK";
			break;
		case psx::kernel::EventSpec::COMMAND_COMPLETE:
			return "COMMAND COMPLETE";
			break;
		case psx::kernel::EventSpec::DATA_READY:
			return "DATA READY";
			break;
		case psx::kernel::EventSpec::DATA_END:
			return "DATA END";
			break;
		case psx::kernel::EventSpec::TIMEOUT:
			return "TIMEOUT";
			break;
		case psx::kernel::EventSpec::UNKNOWN_COMMAND:
			return "UNKNOWN COMMAND";
			break;
		case psx::kernel::EventSpec::END_OF_READBUF:
			return "END OF READ BUFFER";
			break;
		case psx::kernel::EventSpec::END_OF_WRITEBUF:
			return "END OF WRITE BUFFER";
			break;
		case psx::kernel::EventSpec::GENERAL_INT:
			return "GENERAL INTERRUPT";
			break;
		case psx::kernel::EventSpec::NEW_DEVICE:
			return "NEW DEVICE";
			break;
		case psx::kernel::EventSpec::SYSCALL:
			return "SYSCALL";
			break;
		case psx::kernel::EventSpec::ERR:
			return "ERROR";
			break;
		default:
			return "<INVALID/IDK>";
			break;
		}
	}
}