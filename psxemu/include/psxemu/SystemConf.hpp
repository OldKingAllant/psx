#pragma once

#include <common/Defs.hpp>
#include "LoggerConfig.hpp"

#include <unordered_map>
#include <string>

#include <thirdparty/json/single_include/nlohmann/json.hpp>

namespace psx {
	namespace logger {
		NLOHMANN_JSON_SERIALIZE_ENUM(
			LogLevel,
			{
				{LogLevel::_ERROR, "ERROR"},
				{LogLevel::WARN, "WARN"},
				{LogLevel::INFO, "INFO"},
				{LogLevel::_DEBUG, "DEBUG"}
			}
		)

		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
			LoggerConfig,
			file_path,
			enable,
			log_to_file,
			log_to_console,
			enable_yield,
			log_level,
			filters,
			log_syscalls,
			silence_syscalls
		)
	}

	using logger::from_json;
	using logger::to_json;

	struct SystemConfAdvanced {
		bool enable_breakpoints = true;
		bool enable_hle = true;
		bool enable_kernel_callstack = true;
		bool enable_syscall_hooks = true;
		logger::LoggerConfig log_conf = {};

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(SystemConfAdvanced,
			log_conf,
			enable_breakpoints,
			enable_hle,
			enable_kernel_callstack,
			enable_syscall_hooks)
	};

	struct SystemConf {
		std::string bios_path = "../programs/SCPH1001.BIN";

		bool controller_1_connected = true;
		bool controller_2_connected = false;

		std::string controller_1_type = "STANDARD";
		std::string controller_2_type = "NONE";

		std::unordered_map<std::string, std::string> controller_1_map = {
			{ "UP", "DPAD-UP" },
			{ "RIGHT", "DPAD-RIGHT" },
			{ "LEFT", "DPAD-LEFT" },
			{ "DOWN", "DPAD-DOWN" },
			{ "RETURN", "START" },
			{ "X", "CROSS" }
		};
		std::unordered_map<std::string, std::string> controller_2_map;

		bool mc_1_connected = true;
		bool mc_2_connected = false;

		std::string mc_1_file = "../memcards/mc1.mc";
		std::string mc_2_file = "";

		////////////////////////////////
		//DEBUGGING

		bool enable_gdb_stub = true;
		std::uint16_t gdb_stub_port = 5000;

		bool show_vram_win = true;
		bool show_debug_win = true;

		bool load_renderdoc = true;

		bool show_tty = true;
		std::string tty_program = "../x64/Release/TTY_Console.exe";

		SystemConfAdvanced advanced_conf = {};

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(SystemConf, 
			advanced_conf,
			show_tty,
			tty_program,
			load_renderdoc,
			enable_gdb_stub,
			gdb_stub_port,
			show_vram_win,
			show_debug_win,
			bios_path, 
			controller_1_connected,
			controller_2_connected,
			controller_1_type,
			controller_2_type,
			controller_1_map,
			controller_2_map,
			mc_1_connected,
			mc_2_connected,
			mc_1_file,
			mc_2_file)
	};
}