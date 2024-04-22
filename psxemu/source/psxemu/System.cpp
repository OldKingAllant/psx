#include <psxemu/include/psxemu/System.hpp>
#include <psxemu/include/psxemu/psxexe.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/cop0.hpp>
#include <psxemu/include/psxemu/SyscallTables.hpp>

#include <filesystem>
#include <fstream>

#include <fmt/format.h>

namespace psx {
	System::System() :
		m_cpu{&m_status}, m_sysbus{&m_status},
		m_status{}, m_hbreaks{}, m_break_enable{false},
		m_hle_enable{ false }, m_kernel_callstack_enable{false},
		m_stopped {true}, m_kernel{&m_status}
	{
		m_status.cpu = &m_cpu;
		m_status.sysbus = &m_sysbus;
		m_status.sysbus->GetGPU().InitEvents();

		m_cpu.SetHLEHandler([this](u32 address, bool enter) {
			if (!m_hle_enable)
				return true;

			if (enter) {
				u32 r9 = m_cpu.GetRegs().array[9];
				u32 function_id = (address << 4) | r9;
				LogSyscall(function_id, SyscallLogMode::PARAMETERS, &m_status);
				return m_kernel.Syscall(m_cpu.GetRegs().ra - 0x8, function_id);
			}
			else {
				m_kernel.ExitSyscall(address);
			}

			return true;
		});

		m_kernel.SetRamPointer(
			m_sysbus.GetGuestBase()
		);

		m_kernel.SetRomPointer(
			m_sysbus.GetGuestBase() +
			memory::region_offsets::PSX_BIOS_OFFSET
		);
	}

	void System::LoadExe(std::string const& path, ExecArgs args) {
		namespace fs = std::filesystem;

		if (!fs::exists(path) || !fs::is_regular_file(path))
			throw std::exception("Invalid path!");

		std::ifstream file(path, std::ios::binary | std::ios::in);

		if(!file.is_open())
			throw std::exception("Read failed!");

		file.seekg(0, std::ios::end);
		auto size = file.tellg();
		file.seekg(0, std::ios::beg);

		if(size <= 0x800)
			throw std::exception("Invalid executable!");

		u8* buf = new u8[size];

		file.read(std::bit_cast<char*>(buf), size);

		psxexe exe{ buf };

		auto header = exe.header();

		auto dest = header->dest_address;

		auto exe_size = header->filesize;

		if((std::streampos)((u64)exe_size + 0x800) > size)
			throw std::exception("Header size does not match!");

		auto pc = header->start_pc;
		auto gp = header->start_gp;
		auto sp = exe.initial_sp();

		auto memfill_start = header->memfill_start;
		auto memfill_sz = header->memfill_size;

		constexpr u32 HEADER_END = 0x800;

		auto guest_space = m_sysbus.GetGuestBase();

		if (memfill_sz != 0) {
			std::memset(guest_space + memfill_start, 0x0, memfill_sz);
		}

		m_cpu.GetPc() = pc;
		
		auto& regs = m_cpu.GetRegs();

		regs.gp = gp;
		regs.sp = sp;
		regs.fp = sp;

		m_sysbus.CopyRaw(buf + 0x800, dest, exe_size);

		constexpr u32 ARGS_DEST = 0x180;

		if (args.has_value()) {
			auto const& data = args.value();

			m_sysbus.CopyRaw(data.data(), ARGS_DEST, (u32)data.size());
		}

		fmt::print("Successfully loaded executable\n");
		fmt::print("Destination in memory : 0x{:x}\n", dest);
		fmt::print("Program counter : 0x{:x}\n", pc);
		fmt::print("Global pointer : 0x{:x}\n", gp);
		fmt::print("Stack pointer : 0x{:x}\n", sp);
		fmt::print("Memfill start : 0x{:x}, Size : 0x{:x}\n", memfill_start, memfill_sz);
		fmt::print("Size : 0x{:x}\n", exe_size);
	}

	void System::LoadBios(std::string const& path) {
		namespace fs = std::filesystem;

		if (!fs::exists(path) || !fs::is_regular_file(path))
			throw std::exception("Invalid path!");

		std::ifstream file(path, std::ios::binary | std::ios::in);

		if (!file.is_open())
			throw std::exception("Read failed!");

		file.seekg(0, std::ios::end);
		auto size = file.tellg();
		file.seekg(0, std::ios::beg);

		u8* buf = new u8[size];

		file.read(std::bit_cast<char*>(buf), size);

		m_sysbus.LoadBios(buf, (u32)size);

		delete[] buf;

		fmt::println("Kernel BCD date : {}",
			m_kernel.DumpKernelBcdDate());
		fmt::println("Kernel Maker : {}",
			m_kernel.DumpKernelMaker());
		fmt::println("Kernel version : {}",
			m_kernel.DumpKernelVersion());
	}

	void System::InterpreterSingleStep() {
		if (m_status.sysbus->GetDMAControl().HasActiveTransfer()) {
			m_status.sysbus->GetDMAControl().AdvanceTransfer();
		} else
			m_cpu.StepInstruction();

		auto num_cycles = m_sysbus.m_curr_cycles;
		m_sysbus.m_curr_cycles = 0;

		m_status.scheduler.Advance(num_cycles);
	}

	void System::RunInterpreter(u32 num_instruction) {
		while (num_instruction--) {
			InterpreterSingleStep();
		}
	}

	bool System::RunInterpreterUntilBreakpoint() {
		if (!m_break_enable)
			return false;

		bool exit = false;

		while (!exit && !m_stopped) {
			InterpreterSingleStep();

			u32 curr_pc = m_cpu.GetPc();

			exit = std::find(m_hbreaks.begin(), m_hbreaks.end(), curr_pc) !=
				m_hbreaks.end();

			if (m_status.vblank) {
				m_status.vblank = false;
				break;
			}
		}

		return exit;
	}

	void System::ResetVector() {
		fmt::println("Resetting system to the reset vector");

		m_cpu.GetPc() = cpu::RESET_VECTOR;
	}

	void System::AddHardwareBreak(u32 address) {
		auto pos = std::find(m_hbreaks.begin(),
			m_hbreaks.end(), address);

		if (pos == m_hbreaks.end())
			m_hbreaks.push_back(address);
	}

	void System::RemoveHardwareBreak(u32 address) {
		auto pos = std::find(m_hbreaks.begin(),
			m_hbreaks.end(), address);

		if (pos != m_hbreaks.end())
			m_hbreaks.erase(pos);
	}
}