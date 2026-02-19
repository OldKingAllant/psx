#include <psxemu/include/psxemu/Kernel.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/MIPS1.hpp>
#include <psxemu/include/psxemu/psxexe.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <array>

namespace psx::kernel {
	void Kernel::LoadTest(VirtualAddress path_ptr, VirtualAddress headerbuf) {
		if (path_ptr.addr == 0x0 || headerbuf.addr == 0x0) {
			m_sys_status->cpu->GetRegs().v0 = 0x0;
			LOG_ERROR("KERNEL", "[KERNELFS] Failed LoadTest({:#010x},{:#010x}): one address in nullptr", 
				path_ptr.addr, headerbuf.addr);
			return;
		}

		auto path = m_sys_status->sysbus->ReadString(path_ptr, 256);
		auto entry = GetFilesystemEntry(path);

		if (!entry.has_value()) {
			m_sys_status->cpu->GetRegs().v0 = 0x0;
			LOG_ERROR("KERNEL", "[KERNELFS] Failed LoadTest({},{:#010x}): cannot find file",
				path, headerbuf.addr);
			return;
		}

		auto the_file = ReadFileFromEntry(entry.value(), 0, 0x800).value();

		u8* header_data = new u8[0x800];
		std::copy_n(the_file.data(), 0x800, header_data);
		
		psxexe exe{ header_data };
		auto header = exe.header();

		auto guest_space = m_sys_status->sysbus->GetGuestBase();

		if (headerbuf.addr != 0) {
			auto headerbuf_ptr = guest_space + headerbuf.addr;
			constexpr auto COPY_SIZE = 0x4B - 0x10 + 1;
			std::copy_n(std::bit_cast<u8*>(header) + 0x10, COPY_SIZE, headerbuf_ptr);
		}

		m_sys_status->cpu->GetRegs().v0 = 0x1;
	}

	void Kernel::Load(VirtualAddress path_ptr, VirtualAddress headerbuf) {
		LoadTest(path_ptr, headerbuf);
		if (path_ptr.addr == 0x0 || headerbuf.addr == 0x0) {
			return;
		}

		auto path = m_sys_status->sysbus->ReadString(path_ptr, 256);
		auto entry = GetFilesystemEntry(path);

		if (!entry.has_value()) {
			m_sys_status->cpu->GetRegs().v0 = 0x0;
			LOG_ERROR("KERNEL", "[KERNELFS] Failed LoadTest({},{:#010x}): cannot find file",
				path, headerbuf.addr);
			return;
		}

		auto the_file = ReadFileFromEntry(entry.value()).value();

		u8* header_data = new u8[0x800];
		std::copy_n(the_file.data(), 0x800, header_data);

		psxexe exe{ header_data };
		auto header = exe.header();
		auto dest = header->dest_address;
		auto exe_size = header->filesize;

		auto pc = header->start_pc;
		auto gp = header->start_gp;
		auto sp = exe.initial_sp();

		if (sp == 0x0) {
			sp = psxexe::DEFAULT_SP;
		}

		auto memfill_start = header->memfill_start;
		auto memfill_sz = header->memfill_size;

		auto guest_space = m_sys_status->sysbus->GetGuestBase();

		if (memfill_sz != 0) {
			std::memset(guest_space + memfill_start, 0x0, memfill_sz);
		}

		m_sys_status->sysbus->CopyRaw(the_file.data() + 0x800, dest, exe_size);

		FlushCache();

		m_sys_status->cpu->GetRegs().v0 = 0x1;
	}
}