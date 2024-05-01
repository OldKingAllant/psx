#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/SyscallTables.hpp>

#include <fmt/format.h>

#include <sstream>
#include <algorithm>
#include <ranges>

namespace psx {
	std::unordered_map<u32, Syscall> InitSyscallTable() {
		std::unordered_map<u32, Syscall> the_table{};

		//using SyscallParam = std::pair<std::string, SyscallParamType>;
		//using ParamList = std::vector<SyscallParam>

		the_table.insert(std::pair{ 0xA00, 
			std::pair{ "open", ParamList{
				std::pair{ "filename", SyscallParamType::CHAR_PTR },
				std::pair{ "accessmode", SyscallParamType::ACCESS_MODE }
			} }
		});

		the_table.insert(std::pair{ 0xB32,
			std::pair{ "open", ParamList{
				std::pair{ "filename", SyscallParamType::CHAR_PTR },
				std::pair{ "accessmode", SyscallParamType::ACCESS_MODE }
			} }
		});

		the_table.insert(std::pair{ 0xA01,
			std::pair{ "lseek", ParamList{
				std::pair{ "fd", SyscallParamType::INT },
				std::pair{ "offset", SyscallParamType::INT },
				std::pair{ "seektype", SyscallParamType::SEEK_MODE }
			} }
		});

		the_table.insert(std::pair{ 0xB33,
			std::pair{ "lseek", ParamList{
				std::pair{ "fd", SyscallParamType::INT },
				std::pair{ "offset", SyscallParamType::INT },
				std::pair{ "seektype", SyscallParamType::SEEK_MODE }
			} }
		});

		the_table.insert(std::pair{ 0xA02,
			std::pair{ "read", ParamList{
				std::pair{ "fd", SyscallParamType::INT },
				std::pair{ "dst", SyscallParamType::VOID_PTR },
				std::pair{ "len", SyscallParamType::UINT }
			} }
		});

		the_table.insert(std::pair{ 0xB34,
			std::pair{ "read", ParamList{
				std::pair{ "fd", SyscallParamType::INT },
				std::pair{ "dst", SyscallParamType::VOID_PTR },
				std::pair{ "len", SyscallParamType::UINT }
			} }
		});

		the_table.insert(std::pair{ 0xA03,
			std::pair{ "write", ParamList{
				std::pair{ "fd", SyscallParamType::INT },
				std::pair{ "src", SyscallParamType::VOID_PTR },
				std::pair{ "len", SyscallParamType::UINT }
			} }
		});

		the_table.insert(std::pair{ 0xB35,
			std::pair{ "write", ParamList{
				std::pair{ "fd", SyscallParamType::INT },
				std::pair{ "src", SyscallParamType::VOID_PTR },
				std::pair{ "len", SyscallParamType::UINT }
			} }
		});

		the_table.insert(std::pair{ 0xA04,
			std::pair{ "close", ParamList{
				std::pair{ "fd", SyscallParamType::INT },
			} }
		});

		the_table.insert(std::pair{ 0xB36,
			std::pair{ "close", ParamList{
				std::pair{ "fd", SyscallParamType::INT },
			} }
		});

		the_table.insert(std::pair{ 0xA05,
			std::pair{ "ioctl", ParamList{
				std::pair{ "fd", SyscallParamType::INT },
				std::pair{ "cmd", SyscallParamType::INT },
				std::pair{ "arg", SyscallParamType::INT }
			} }
		});

		the_table.insert(std::pair{ 0xB37,
			std::pair{ "ioctl", ParamList{
				std::pair{ "fd", SyscallParamType::INT },
				std::pair{ "cmd", SyscallParamType::INT },
				std::pair{ "arg", SyscallParamType::INT }
			} }
		});

		the_table.insert(std::pair{ 0xA06,
			std::pair{ "exit", ParamList{
				std::pair{ "exitcode", SyscallParamType::INT },
			} }
		});

		the_table.insert(std::pair{ 0xB38,
			std::pair{ "exit", ParamList{
				std::pair{ "exitcode", SyscallParamType::INT },
			} }
		});

		the_table.insert(std::pair{ 0xA07,
			std::pair{ "isatty", ParamList{
				std::pair{ "fd", SyscallParamType::INT },
			} }
		});

		the_table.insert(std::pair{ 0xB39,
			std::pair{ "isatty", ParamList{
				std::pair{ "fd", SyscallParamType::INT },
			} }
		});

		the_table.insert(std::pair{ 0xA08,
			std::pair{ "getc", ParamList{
				std::pair{ "fd", SyscallParamType::INT },
			} }
		});

		the_table.insert(std::pair{ 0xB3A,
			std::pair{ "getc", ParamList{
				std::pair{ "fd", SyscallParamType::INT },
			} }
		});

		the_table.insert(std::pair{ 0xA09,
			std::pair{ "putch", ParamList{
				std::pair{ "char", SyscallParamType::CHAR },
				std::pair{ "fd", SyscallParamType::INT }
			} }
		});

		the_table.insert(std::pair{ 0xB3B,
			std::pair{ "putch", ParamList{
				std::pair{ "char", SyscallParamType::CHAR },
				std::pair{ "fd", SyscallParamType::INT }
			} }
		});

		the_table.insert(std::pair{ 0xA39,
			std::pair{ "InitHeap", ParamList{
				std::pair{ "addr", SyscallParamType::VOID_PTR },
				std::pair{ "size", SyscallParamType::UINT }
			} }
		});

		the_table.insert(std::pair{ 0xA3B,
			std::pair{ "getchar", ParamList{
			} }
		});

		the_table.insert(std::pair{ 0xB3C,
			std::pair{ "getchar", ParamList{
			} }
		});

		the_table.insert(std::pair{ 0xA3C,
			std::pair{ "putchar", ParamList{
				std::pair{ "char", SyscallParamType::CHAR }
			} }
		});

		the_table.insert(std::pair{ 0xB3D,
			std::pair{ "putchar", ParamList{
				std::pair{ "char", SyscallParamType::CHAR }
			} }
		});

		the_table.insert(std::pair{ 0xC08,
			std::pair{ "SysInitMemory", ParamList{
				std::pair{ "addr", SyscallParamType::VOID_PTR },
				std::pair{ "size", SyscallParamType::UINT }
			} }
		});

		the_table.insert(std::pair{ 0xA3F,
			std::pair{ "printf", ParamList{
				std::pair{ "str", SyscallParamType::CHAR_PTR },
			} }
		});

		the_table.insert(std::pair{ 0xC1C,
			std::pair{ "AdjustA0Table", ParamList{} }
		});

		the_table.insert(std::pair{ 0xC07,
			std::pair{ "InstallExceptionHandlers", ParamList{} }
		});

		the_table.insert(std::pair{ 0xA44,
			std::pair{ "FlushCache", ParamList{} }
		});

		the_table.insert(std::pair{ 0xB18,
			std::pair{ "ResetEntryInt", ParamList{} }
		});

		the_table.insert(std::pair{ 0xC12,
			std::pair{ "InstallDevices", ParamList{
				std::pair{ "ttyflag", SyscallParamType::UINT },
			} }
		});

		the_table.insert(std::pair{ 0xA99,
			std::pair{ "add_nullcon_driver", ParamList{} }
		});

		the_table.insert(std::pair{ 0xB47,
			std::pair{ "AddDrv", ParamList{
				std::pair{ "dev_info", SyscallParamType::UINT },
			} }
		});

		the_table.insert(std::pair{ 0xA96,
			std::pair{ "AddCDROMDevice", ParamList{} }
		});

		the_table.insert(std::pair{ 0xA97,
			std::pair{ "AddMemcardDevice", ParamList{} }
		});

		the_table.insert(std::pair{ 0xB00,
			std::pair{ "alloc_kernel_memory", ParamList{
				std::pair{ "size", SyscallParamType::UINT },
			} }
		});

		the_table.insert(std::pair{ 0xC00,
			std::pair{ "EnqueueTimerAndBlankIrqs", ParamList{
				std::pair{ "priority", SyscallParamType::UINT },
			} }
		});

		the_table.insert(std::pair{ 0xC01,
			std::pair{ "EnqueueSyscallHandler", ParamList{
				std::pair{ "priority", SyscallParamType::UINT },
			} }
		});

		the_table.insert(std::pair{ 0xC0C,
			std::pair{ "InitDefInt", ParamList{
				std::pair{ "priority", SyscallParamType::UINT },
			} }
		});

		the_table.insert(std::pair{ 0xB09,
			std::pair{ "CloseEvent", ParamList{
				std::pair{ "event", SyscallParamType::UINT },
			} }
		});

		the_table.insert(std::pair{ 0xAA3,
			std::pair{ "DequeueCdIntr", ParamList{} }
		});

		the_table.insert(std::pair{ 0xC03,
			std::pair{ "SysDeqIntRP", ParamList{
				std::pair{ "priority", SyscallParamType::UINT },
				std::pair{ "struc", SyscallParamType::VOID_PTR }
			} }
		});

		the_table.insert(std::pair{ 0xB08,
			std::pair{ "OpenEvent", ParamList{
				std::pair{ "class", SyscallParamType::EVENT_CLASS },
				std::pair{ "spec", SyscallParamType::UINT },
				std::pair{ "mode", SyscallParamType::EVENT_MODE },
				std::pair{ "func", SyscallParamType::VOID_PTR },
			} }
		});

		the_table.insert(std::pair{ 0xB0C,
			std::pair{ "EnableEvent", ParamList{
				std::pair{ "event", SyscallParamType::UINT },
			} }
		});

		the_table.insert(std::pair{ 0xB19,
			std::pair{ "HookEntryInt", ParamList{
				std::pair{ "addr", SyscallParamType::VOID_PTR },
			} }
		});

		the_table.insert(std::pair{ 0xC0A,
			std::pair{ "ChangeClearRCnt", ParamList{
				std::pair{ "timer", SyscallParamType::UINT },
				std::pair{ "flag", SyscallParamType::UINT },
			} }
		});

		the_table.insert(std::pair{ 0xA49,
			std::pair{ "GPU_cw", ParamList{
				std::pair{ "cmd", SyscallParamType::UINT },
			} }
		});

		the_table.insert(std::pair{ 0xB17,
			std::pair{ "ReturnFromException", ParamList{} }
		});

		the_table.insert(std::pair{ 0xA33,
			std::pair{ "malloc", ParamList{
				std::pair{ "size", SyscallParamType::UINT },
			} }
		});

		the_table.insert(std::pair{ 0xC02,
			std::pair{ "SysEnqIntRP", ParamList{
				std::pair{ "priority", SyscallParamType::UINT },
				std::pair{ "struc", SyscallParamType::VOID_PTR }
			} }
		});

		the_table.insert(std::pair{ 0xB0B,
			std::pair{ "TestEvent", ParamList{
				std::pair{ "event", SyscallParamType::UINT },
			} }
		});

		the_table.insert(
			std::pair{ 0xA2F, 
				std::pair{ "rand", ParamList{} }
			}
		);

		the_table.insert(
			std::pair{ 0xB07, std::pair{
				"DeliverEvent",
				ParamList{
					std::pair{ "class", SyscallParamType::EVENT_CLASS },
					std::pair{ "spec", SyscallParamType::UINT }
				}
			} }
		);

		the_table.insert(
			std::pair{ 0xB13, std::pair{
				"StartPAD2",
				ParamList{}
			} }
		);

		the_table.insert(
			std::pair{ 0xB12, std::pair{
				"InitPAD2",
				ParamList{
					std::pair{ "buf1", SyscallParamType::VOID_PTR },
					std::pair{ "size1", SyscallParamType::UINT },
					std::pair{ "buf2", SyscallParamType::VOID_PTR },
					std::pair{ "size2", SyscallParamType::UINT }
				}
			} }
		);

		return the_table;
	}

	Syscall UNKNOWN_SYSCALL = {
		std::string{ "UNKNOWN" },
		{}
	};

	static const std::unordered_map<u32, Syscall> table = InitSyscallTable();

	Syscall const& GetSyscallDescriptor(u32 syscall_num) {
		if (!table.contains(syscall_num))
			return UNKNOWN_SYSCALL;

		return table.find(syscall_num)->second;
	}

	FORCE_INLINE bool MustEscapeChar(char ch) {
		switch (ch)
		{
		case '\n':
		case '\t':
		case '\b':
		case '\v':
		case '\r':
		case '\f':
			return true;
		default:
			break;
		}

		return false;
	}

	FORCE_INLINE const char* EscapeChar(char ch) {
		switch (ch)
		{
		case '\n':
			return "\\n";
		case '\t':
			return "\\t";
		case '\b':
			return "\\b";
		case '\v':
			return "\\v";
		case '\r':
			return "\\r";
		case '\f':
			return "\\f";
		default:
			break;
		}

		return "";
	}

	void LogParameter(SyscallParam const& param, u32 param_value, system_status* status) {
		std::ostringstream out = {};

		out << param.first << " = ";

		switch (param.second)
		{
		case SyscallParamType::CHAR:
			if(MustEscapeChar((char)param_value))
				out << '\'' << EscapeChar((char)param_value) << '\'';
			else
				out << '\'' << (char)param_value << '\'';
			break;
		case SyscallParamType::INT:
			out << (int)param_value;
			break;
		case SyscallParamType::UINT:
			out << "0x" << std::hex << param_value;
			break;
		case SyscallParamType::VOID_PTR:
			out << "(void*)0x" << std::hex << param_value;
			break;
		case SyscallParamType::CHAR_PTR: {
			u32 address = param_value;
			char the_char = '\0';

			auto bus = status->sysbus;

			u32 depth = 256;

			out << "\"";

			while ((the_char = (char)bus->Read<u8, false, false>(address)) && depth--) {
				if (MustEscapeChar(the_char))
					out << EscapeChar( the_char );
				else
					out << the_char;
				address++;
			}

			out << "\"";
		}
		break;
		case SyscallParamType::ACCESS_MODE: {
			if (param_value & 1)
				out << "READ|";

			if (param_value & 2)
				out << "WRITE|";

			if ((param_value >> 9) & 1)
				out << "OPEN_EXISTING|";
			else
				out << "CREATE|";

			if ((param_value >> 15) & 1)
				out << "ASYNC";
			else
				out << "SYNC";
		}
		break;
		case SyscallParamType::SEEK_MODE: {
			switch (param_value)
			{
			case 0:
				out << "SEEK_BEG";
				break;
			case 1:
				out << "SEEK_CURR";
				break;
			case 2:
				out << "SEEK_END";
				break;
			default:
				out << "INVALID_SEEK";
				break;
			}
		}
		break;
		case SyscallParamType::EVENT_MODE: {
			if (param_value == 0x1000)
				out << "STAY_BUSY";
			else if (param_value == 0x2000)
				out << "MARK_READY";
			else
				out << "INVALID";
		}
		break;
		case SyscallParamType::EVENT_CLASS: {
			if (param_value >= 0 && param_value <= 0xF)
				out << "MEMORY_CARD";
			else {
				switch (param_value)
				{
				case 0xF0000001:
					out << "IRQ0_VBLANK";
					break;
				case 0xF0000002:
					out << "IRQ1_GPU";
					break;
				case 0xF0000003:
					out << "IRQ2_CDROM";
					break;
				case 0xF0000004:
					out << "IRQ3_DMA";
					break;
				case 0xF0000005:
					out << "IRQ4_RTC0";
					break;
				case 0xF0000006:
					out << "IRQ5/6_RTC1";
					break;
				case 0xF0000008:
					out << "IRQ7_CONTROLLER";
					break;
				case 0xF0000009:
					out << "IRQ9_SPU";
					break;
				case 0xF000000A:
					out << "IRQ10_PIO";
					break;
				case 0xF000000B:
					out << "IRQ8_SIO";
					break;
				case 0xF0000010:
					out << "CPU_EXCEPTION";
					break;
				case 0xF2000000:
					out << "ROOT_COUNTER0";
					break;
				case 0xF2000001:
					out << "ROOT_COUNTER1";
					break;
				case 0xF2000002:
					out << "ROOT_COUNTER2";
					break;
				case 0xF2000003:
					out << "ROOT_COUNTER3";
					break;
				default:
					out << "UNKNOWN";
					break;
				}
			}
		}
		break;
		default:
			break;
		}

		out << ",";

		fmt::vprint(out.str(), fmt::make_format_args());
	}

	FORCE_INLINE u32 GetParam(u32 num, system_status* status) {
		if (num < 4)
			return status->cpu->GetRegs().array[num + 4];

		u32 offset = (num - 4) + 0x10;

		return status->sysbus->Read<u32, false, false>(
			status->cpu->GetRegs().sp + offset
		);
	}

	void LogSyscall(u32 syscall_num, SyscallLogMode log_mode, system_status* status) {
		if (log_mode == SyscallLogMode::NUMBER) {
			fmt::println("[SYSCALL] Number 0x{:x}", syscall_num);
			return;
		}

		auto const& syscall_desc = GetSyscallDescriptor(syscall_num);

		fmt::vprint("[SYSCALL] 0x{:x}:{}", fmt::make_format_args(syscall_num, syscall_desc.first));

		if (log_mode == SyscallLogMode::NAME) {
			fmt::println("");
			return;
		}

		u32 param_pos = 0;

		fmt::print("(");

		for (auto const& param : syscall_desc.second) {
			u32 param_val = GetParam(param_pos, status);
			LogParameter(param, param_val, status);
			param_pos++;
		}

		fmt::println(")");
	}

	std::vector<u32> GetSyscallIdsByName(std::string const& name) {
		auto descriptor_iter = table |
			std::views::filter([&name](std::pair<u32, Syscall> const& syscall) {
				return syscall.second.first == name;
			});

		return descriptor_iter |
			std::views::transform([](std::pair<u32, Syscall> const& syscall) { return syscall.first; }) |
			std::ranges::to<std::vector<u32>>();
	}
}