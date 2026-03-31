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
				{LogLevel::_DBG, "DEBUG"}
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

	NLOHMANN_JSON_SERIALIZE_ENUM(
		ConsoleVideoMode,
		{
			{ConsoleVideoMode::NTSC, "NTSC"},
			{ConsoleVideoMode::PAL, "PAL"}
		}
	)

	using logger::from_json;
	using logger::to_json;

	struct SystemConfAdvanced {
		bool enable_breakpoints = true;
		bool enable_hle = true;
		bool enable_kernel_callstack = true;
		bool enable_syscall_hooks = true;
		logger::LoggerConfig log_conf = {};
		bool record_gpu_commands = false;
		std::uint32_t recorded_gpu_frames = 0;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(SystemConfAdvanced,
			recorded_gpu_frames,
			record_gpu_commands,
			log_conf,
			enable_breakpoints,
			enable_hle,
			enable_kernel_callstack,
			enable_syscall_hooks)
	};

	struct GpuConf {
		std::uint32_t resolution_multiplier = 1;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(GpuConf,
			resolution_multiplier)
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

		std::string cdrom_file = "";
		std::string console_region = "AMERICA";
		ConsoleVideoMode video_mode = ConsoleVideoMode::NTSC;

		std::string exe_file = "";
		std::string exe_args = "";
		bool patch_load = false;
		bool force_run = false;

		bool enable_exe_patching = false;

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
		GpuConf gpu_conf = {};

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(SystemConf, 
			video_mode,
			gpu_conf,
			enable_exe_patching,
			force_run,
			exe_args,
			exe_file,
			patch_load,
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
			mc_2_file,
			cdrom_file,
			console_region)
	};
}