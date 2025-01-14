#include <psxemu/include/psxemu/System.hpp>
#include <psxemu/include/psxemu/psxexe.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/cop0.hpp>
#include <psxemu/include/psxemu/SyscallTables.hpp>
#include <psxemu/include/psxemu/SIOPadMemcardDriver.hpp>
#include <psxemu/include/psxemu/StandardController.hpp>
#include <psxemu/include/psxemu/NullController.hpp>
#include <psxemu/include/psxemu/NullMemcard.hpp>
#include <psxemu/include/psxemu/OfficialMemcard.hpp>

#include <filesystem>
#include <fstream>

#include <fmt/format.h>

namespace psx {
	System::System(std::shared_ptr<SystemConf> config) :
		m_cpu{&m_status}, m_sysbus{&m_status},
		m_status{}, m_hbreaks{}, m_break_enable{false},
		m_hle_enable{ false }, m_kernel_callstack_enable{false},
		m_stopped {true}, m_kernel{&m_status},
		m_silenced_syscalls{}, m_sys_conf{config}
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

				if(!m_silenced_syscalls.contains(function_id))
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

		SilenceSyscallsDefault();
		FollowConfig();
		ResetVector();
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

		fmt::println("[SYSTEM] Loading {}", path);
		fmt::println("         Hooking EnqueueTimerAndBlankIrqs()");

		//Temporarely enable hooks
		bool old_enable = m_kernel.HooksEnabled();
		m_kernel.SetHooksEnable(true);
		
		bool hook_flag = false;
		auto hook = m_kernel.InsertExitHook(0xc00, [this, &hook_flag](psx::u32, psx::u32) {
			SetStopped(true);
			hook_flag = true;
		});

		while (!hook_flag) {
			m_stopped = false;
			RunInterpreterUntilBreakpoint();
		}

		m_kernel.RemoveExitHook(hook);
		m_kernel.SetHooksEnable(old_enable);

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

		fmt::print("[SYSTEM] Successfully loaded executable\n");
		fmt::print("         Destination in memory : 0x{:x}\n", dest);
		fmt::print("         Program counter : 0x{:x}\n", pc);
		fmt::print("         Global pointer : 0x{:x}\n", gp);
		fmt::print("         Stack pointer : 0x{:x}\n", sp);
		fmt::print("         Memfill start : 0x{:x}, Size : 0x{:x}\n", memfill_start, memfill_sz);
		fmt::print("         Size : 0x{:x}\n", exe_size);
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

	bool System::IsSyscallSilent(u32 syscall_num) {
		return m_silenced_syscalls.contains(syscall_num);
	}

	void System::SetSyscallSilent(u32 syscall_num, bool silent) {
		if (m_silenced_syscalls.contains(syscall_num) && silent)
			m_silenced_syscalls.erase(syscall_num);
	}

	void System::SilenceSyscallsDefault() {
		std::array list = { "rand", "TestEvent" };

		for (auto const& name : list) {
			auto ids = GetSyscallIdsByName(name);

			for (u32 id : ids)
				m_silenced_syscalls.insert(id);
		}
	}

	void System::ConnectCard(u32 slot, std::string const& path) {
		if (slot != 0 && slot != 1) {
			fmt::println("[SYSTEM] Invalid mc slot {}", slot);
			return;
		}

		auto fs_path = std::filesystem::path{ path };

		if (!fs_path.has_extension()) {
			fmt::println("[SYSTEM] Missing extension from {}, cannot infer type",
				path);
			return;
		}

		std::unordered_map<std::string, MemcardType> mc_types = {
			{ std::string{".MC"}, MemcardType::OFFICIAL},
			{ std::string{".mc"}, MemcardType::OFFICIAL}
		};

		auto extension = fs_path.extension().string();

		if (!mc_types.contains(extension)) {
			fmt::println("[SYSTEM] Invalid mc extension {}",
				extension);
			return;
		}

		auto type = mc_types[extension];

		if (!std::filesystem::exists(path)) {
			std::fstream create_file{ path, std::ios::out };

			if (!create_file.is_open()) {
				fmt::println("[SYSTEM] Failed creating {}",
					path);
				return;
			}
		}

		std::unique_ptr<AbstractMemcard> memcard{std::make_unique<NullMemcard>()};

		switch (type)
		{
		case MemcardType::OFFICIAL: {
			decltype(memcard) new_card = std::make_unique<OfficialMemcard>();
			memcard.swap(new_card);
		}
			break;
		default:
			break;
		}

		if (!memcard->LoadFile(path)) {
			fmt::println("[SYSTEM] Failed loading {}",
				path);
			return;
		}

		if (slot == 0) {
			dynamic_cast<SIOPadCardDriver*>(m_sysbus.GetSIO0()
				.GetDevice1())->ConnectCard(std::move(memcard));
		}
		else {
			dynamic_cast<SIOPadCardDriver*>(m_sysbus.GetSIO0()
				.GetDevice2())->ConnectCard(std::move(memcard));
		}

		fmt::println("[SYSTEM] Successfully inserted MC in slot {}", slot);
	}

	void System::ConnectController(u32 slot, std::string const& type) {
		if (slot != 0 && slot != 1) {
			fmt::println("[SYSTEM] Invalid controller slot {}", slot);
			return;
		}

		std::unordered_map<std::string, ControllerType> type_map = {
			{ "NONE", ControllerType::NONE },
			{ "STANDARD", ControllerType::STANDARD }
		};

		if (!type_map.contains(type)) {
			fmt::println("[SYSTEM] Invalid controller type {}", type);
			return;
		}

		auto controller_type = type_map[type];

		std::unique_ptr<AbstractController> controller{ std::make_unique<NullController>() };

		switch (controller_type)
		{
		case ControllerType::NONE: {
			decltype(controller) new_controller = std::make_unique<NullController>();
			controller.swap(new_controller);
		}
			break;
		case ControllerType::STANDARD: {
			decltype(controller) new_controller = std::make_unique<StandardController>();
			controller.swap(new_controller);
		}
			break;
		default:
			break;
		}

		if (slot == 0) {
			dynamic_cast<SIOPadCardDriver*>(m_sysbus.GetSIO0()
				.GetDevice1())->ConnectController(std::move(controller));
		}
		else {
			dynamic_cast<SIOPadCardDriver*>(m_sysbus.GetSIO0()
				.GetDevice2())->ConnectController(std::move(controller));
		}
	}

	void System::FollowConfig() {
		auto make_driver = [](std::unique_ptr<AbstractController> controller,
			std::unique_ptr<AbstractMemcard> card) {
				std::unique_ptr<SIOAbstractDevice> driver{ std::make_unique<SIOPadCardDriver>() };
				dynamic_cast<SIOPadCardDriver*>(driver.get())->ConnectController(std::move(controller));
				dynamic_cast<SIOPadCardDriver*>(driver.get())->ConnectCard(std::move(card));
				return driver;
			};

		m_sysbus.GetSIO0()
			.Port1Connect(make_driver(
				std::make_unique<NullController>(),
				std::make_unique<NullMemcard>()
			));
		m_sysbus.GetSIO0()
			.Port2Connect(make_driver(
				std::make_unique<NullController>(),
				std::make_unique<NullMemcard>()
			));

		LoadBios(m_sys_conf->bios_path);

		if (m_sys_conf->mc_1_connected) ConnectCard(0, m_sys_conf->mc_1_file);
		if (m_sys_conf->mc_2_connected) ConnectCard(1, m_sys_conf->mc_2_file);

		if (m_sys_conf->controller_1_connected) ConnectController(0, m_sys_conf->controller_1_type);
		if (m_sys_conf->controller_2_connected) ConnectController(1, m_sys_conf->controller_2_type);

		ToggleBreakpoints(m_sys_conf->advanced_conf.enable_breakpoints);
		SetHleEnable(m_sys_conf->advanced_conf.enable_hle);
		SetEnableKernelCallstack(m_sys_conf->advanced_conf.enable_kernel_callstack);
		m_kernel.SetHooksEnable(m_sys_conf->advanced_conf.enable_syscall_hooks);
	}
}