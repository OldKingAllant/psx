#pragma once

#include <map>
#include <string>
#include <vector>

#include <common/Defs.hpp>
#include <common/Macros.hpp>

namespace psx {
	struct system_status;

	enum class SyscallParamType {
		CHAR,
		INT,
		UINT,
		CHAR_PTR,
		VOID_PTR,
		ACCESS_MODE,
		SEEK_MODE,
		EVENT_CLASS, 
		EVENT_MODE
	};

	using SyscallParam = std::pair<std::string, SyscallParamType>;

	enum class SyscallLogMode {
		NUMBER,
		NAME,
		PARAMETERS
	};

	using ParamList = std::vector<SyscallParam>;
	using Syscall = std::pair<std::string, std::vector<SyscallParam>>;

	Syscall const& GetSyscallDescriptor(u32 syscall_num);
	void LogSyscall(u32 syscall_num, SyscallLogMode log_mode, system_status* status);
}