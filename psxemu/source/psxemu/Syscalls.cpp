#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/SyscallTables.hpp>

#include <fmt/format.h>
#include <thirdparty/magic_enum/include/magic_enum/magic_enum.hpp>

#include <sstream>
#include <algorithm>
#include <ranges>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

namespace psx {
	void InsertSyscall(u32 id, std::string_view signature, std::unordered_map<u32, Syscall>& table) {
		if (signature.empty())
			return;

		/*
		Format:
		return_type Name(param_name=type,[param2_name=type,])
		*/

		auto space_pos = signature.find_first_of(' ', 0);

		if (space_pos == std::string_view::npos)
			return;

		auto return_type = signature.substr(0, space_pos);
		(void)return_type;

		auto open_bracket_pos = signature.find_first_of('(', 0);

		if (open_bracket_pos == std::string_view::npos)
			return;

		auto name = signature.substr(space_pos + 1, open_bracket_pos - (space_pos + 1));

		signature = signature.substr(open_bracket_pos + 1);

		ParamList params{};

		auto curr_pos = signature.find_first_of(',');

		while (curr_pos != std::string_view::npos) {
			auto param = signature.substr(0, curr_pos);

			auto sep_pos = param.find_first_of('=');

			if (sep_pos != std::string_view::npos) {
				auto param_name = param.substr(0, sep_pos);
				auto type = param.substr(sep_pos + 1);

				auto converted_type = magic_enum::enum_cast<SyscallParamType>(type,
					magic_enum::case_insensitive);

				if (converted_type.has_value()) {
					params.push_back({ std::string(param_name), converted_type.value() });
				}
			}

			signature = signature.substr(curr_pos + 1);
			curr_pos = signature.find_first_of(',');
		}

		table.insert({ id, { std::string(name), params } });
	}

	std::unordered_map<u32, Syscall> InitSyscallTable() {
		std::unordered_map<u32, Syscall> the_table{};

		const std::vector syscalls = {
			std::pair{ u32(0xB4A), std::string("void InitCARD2(pad_enable=BOOL,)") },
			{ 0xB20, "void UnDeliverEvent(class=EVENT_CLASS,spec=UINT,)" },
			{ 0xB50, "void _new_card()" },
			{ 0xAA7, "void bufs_cb_0()" },
			{ 0xB58, "void _card_chan()" },
			{ 0xAAD, "void _card_auto(flag=BOOL,)" },
			{ 0xB45, "void erase(filename=CHAR_PTR,)" },
			{ 0xC1D, "bool get_card_find_mode()" },
			{ 0xC1A, "void set_card_find_mode(mode=BOOL,)" },
			{ 0xAAB, "void _card_info(port=UINT,)" },
			{ 0xAA9, "void bufs_cb_2()" },
			{ 0xB4D, "void _card_info_subfunc(port=UINT,)" },
			{ 0xA70, "void _bu_init()"},
			{ 0xB5B, "void ChangeClearPAD(int=INT,)" },
			{ 0xB4B, "void StartCARD2()" },
			{ 0xA00, "uint open(filename=CHAR_PTR,accessmode=ACCESS_MODE,)" },
			{ 0xB32, "uint open(filename=CHAR_PTR,accessmode=ACCESS_MODE,)" },
			{ 0xA01, "void lseek(fd=INT,offset=INT,seekmode=SEEK_MODE,)" },
			{ 0xB33, "void lseek(fd=INT,offset=INT,seekmode=SEEK_MODE,)" },
			{ 0xA02, "void read(fd=INT,dst=VOID_PTR,len=UINT,)" },
			{ 0xB34, "void read(fd=INT,dst=VOID_PTR,len=UINT,)" },
			{ 0xA03, "void write(fd=INT,src=VOID_PTR,len=UINT,)" },
			{ 0xB35, "void write(fd=INT,src=VOID_PTR,len=UINT,)" },
			{ 0xA04, "void close(fd=INT,)" },
			{ 0xB36, "void close(fd=INT,)" },
			{ 0xA05, "void ioctl(fd=INT,cmd=INT,arg=INT,)" },
			{ 0xB37, "void ioctl(fd=INT,cmd=INT,arg=INT,)" },
			{ 0xA06, "void exit(exitcode=INT,)" },
			{ 0xB38, "void exit(exitcode=INT,)" },
			{ 0xA07, "bool isatty(fd=INT,)" },
			{ 0xB39, "bool isatty(fd=INT,)" },
			{ 0xA08, "char getc(fd=INT,)" },
			{ 0xB3A, "char getc(fd=INT,)" },
			{ 0xA09, "void putch(char=CHAR,fd=INT,)" },
			{ 0xB3B, "void putch(char=CHAR,fd=INT,)" },
			{ 0xA39, "void InitHeap(addr=VOID_PTR,size=UINT,)" }, 
			{ 0xA3B, "char getchar()" },
			{ 0xB3C, "char getchar()" },
			{ 0xA3C, "void putchar(char=CHAR,)" },
			{ 0xB3D, "void putchar(char=CHAR,)" },
			{ 0xC08, "void SysInitMemory(addr=VOID_PTR,size=UINT,)" },
			{ 0xA3F, "void printf(str=CHAR_PTR,)" },
			{ 0xC1C, "void AdjustA0Table()" },
			{ 0xC07, "void InstallExceptionHandlers()" },
			{ 0xA44, "void FlushCache()" },
			{ 0xB18, "void ResetEntryInt()" },
			{ 0xC12, "void InstallDevices(ttyflag=UINT,)" },
			{ 0xA99, "void add_nullcon_driver()" },
			{ 0xB47, "void AddDrv(dev_info=UINT,)" },
			{ 0xA96, "void AddCDROMDevice()" },
			{ 0xA97, "void AddMemcardDevice()" },
			{ 0xB00, "void* alloc_kernel_memory(size=UINT,)" },
			{ 0xC00, "void EnqueueTimerAndVBlankIrqs(priority=UINT,)" },
			{ 0xC01, "void EnqueueSyscallHandler(priority=UINT,)" },
			{ 0xC0C, "void InitDefInt(priority=UINT,)" },
			{ 0xB09, "void CloseEvent(event=UINT,)" },
			{ 0xAA3, "void DequeueCdIntr()" },
			{ 0xC03, "void SysDeqIntRP(priority=UINT,struc=VOID_PTR,)" },
			{ 0xB08, "uint OpenEvent(class=EVENT_CLASS,spec=UINT,mode=EVENT_MODE,func=VOID_PTR,)" },
			{ 0xB0C, "void EnableEvent(event=UINT,)" },
			{ 0xB19, "void HookEntryInt(addr=VOID_PTR,)" },
			{ 0xC0A, "void ChangeClearRCnt(timer=UINT,flag=UINT,)" },
			{ 0xA49, "void GPU_cw(cmd=UINT,)" },
			{ 0xB17, "[noreturn] ReturnFromException()" },
			{ 0xA33, "void* malloc(size=UINT,)" },
			{ 0xC02, "void SysEnqIntRP(priority=UINT,struc=VOID_PTR,)" },
			{ 0xB0B, "bool TestEvent(event=UINT,)" },
			{ 0xA2F, "uint rand()" },
			{ 0xB07, "void DeliverEvent(class=EVENT_CLASS,spec=UINT,)" },
			{ 0xB13, "void StartPAD2()" },
			{ 0xB12, "void InitPAD2(buf1=VOID_PTR,size1=UINT,buf2=VOID_PTR,size2=UINT,)" },
			{ 0xB4E, "void _card_write(port=UINT,sector=UINT,src=VOID_PTR,)" },
			{ 0xB4F, "void _card_read(port=UINT,sector=UINT,dst=VOID_PTR,)" },
			{ 0xB5C, "mc_status _card_status(port=UINT,)" },
			{ 0xB5D, "mc_status _card_wait(port=UINT,)" },
			{ 0xAA1, "void SystemError(type=CHAR,code=UINT,)" },
			{ 0xA42, "bool Load(filename=CHAR_PTR,headerbuf=VOID_PTR,)" },
			{ 0xA43, "void Exec(headerbuf=VOID_PTR,param1=UINT,param2=UINT,)" },
			{ 0xB0E, "uint OpenThread(pc=UINT,SP=UINT,GP=UINT,)" },
			{ 0xB0F, "uint CloseThread(handle=UINT,)" },
			{ 0xB10, "uint ChangeThread(handle=UINT,)" },
			{ 0xB0A, "uint WaitEvent(event=UINT,)" },
			{ 0xB3F, "void puts(str=CHAR_PTR,)" }
		};

		for (auto const& [id, syscall] : syscalls) {
			InsertSyscall(id, syscall, the_table);
		}

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

	std::string LogParameter(SyscallParam const& param, u32 param_value, system_status* status) {
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

			if (address == 0x0) {
				out << "NULL";
			}
			else {
				out << "\"";

				while ((the_char = (char)bus->Read<u8, false, false>(address)) && depth--) {
					if (MustEscapeChar(the_char))
						out << EscapeChar(the_char);
					else
						out << the_char;
					address++;
				}

				out << "\"";
			}
		}
		break;
		case SyscallParamType::ACCESS_MODE: {
			if (param_value & 1)
				out << "READ|";

			if (param_value & 2)
				out << "WRITE|";

			if ((param_value >> 9) & 1)
				out << "CREATE|";
			else
				out << "OPEN_EXISTING|";

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
				case 0xF0000011:
				case 0xF4000001:
					out << "MEMORY_CARD";
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
		case SyscallParamType::BOOL:
			out << (param_value ? "true" : "false");
		break;
		default:
			break;
		}

		out << ",";

		return out.str();
	}

	FORCE_INLINE u32 GetParam(u32 num, system_status* status) {
		if (num < 4)
			return status->cpu->GetRegs().array[num + 4];

		u32 offset = 0x4 * (num - 4) + 0x10;

		return status->sysbus->Read<u32, false, false>(
			status->cpu->GetRegs().sp + offset
		);
	}

	void LogSyscall(u32 syscall_num, SyscallLogMode log_mode, system_status* status) {
		if (log_mode == SyscallLogMode::NUMBER) {
			LOG_DEBUG("SYSCALL", "[SYSCALL] Number 0x{:x}", syscall_num);
			return;
		}

		auto const& syscall_desc = GetSyscallDescriptor(syscall_num);

		std::ostringstream log_message{};

		log_message << fmt::vformat("[SYSCALL] 0x{:x}:{}", fmt::make_format_args(syscall_num, syscall_desc.first));

		if (log_mode == SyscallLogMode::NAME) {
			LOG_DEBUG("SYSCALL", log_message.str());
			return;
		}

		u32 param_pos = 0;

		log_message << '(';

		for (auto const& param : syscall_desc.second) {
			u32 param_val = GetParam(param_pos, status);
			log_message << LogParameter(param, param_val, status);
			param_pos++;
		}

		log_message << ')';
		LOG_DEBUG("SYSCALL", log_message.str());
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