#pragma once

#include <string>
#include <unordered_set>

namespace psx::logger {
	enum class LogLevel {
		_ERROR,
		WARN,
		INFO,
		_DEBUG
	};

	struct LoggerConfig {
		std::string file_path = "psxemu_log.txt";
		bool enable = true;
		bool log_to_file = false;
		bool log_to_console = true;
		bool enable_yield = true;
		LogLevel log_level = LogLevel::_DEBUG;
		std::unordered_set<std::string> filters = {};
		bool log_syscalls = true;
		std::unordered_set<std::string> silence_syscalls = {};
	};
}