#include "ConfigLoader.hpp"

#include <filesystem>
#include <fstream>

#include <fmt/format.h>

namespace config {
	Config::Config() :
		m_sys_conf{std::make_shared<psx::SystemConf>()},
		m_config_repr{}
	{}

	void Config::LoadFromFile(std::string const& path) {
		if (!std::filesystem::exists(path)) {
			fmt::println("[CONFIG] File {} does not exist", path);
			return;
		}
		
		std::ifstream in_file{ path };

		if (!in_file.is_open()) {
			fmt::println("[CONFIG] Cannot open {}", path);
			return;
		}

		auto file_size = std::filesystem::file_size(path);

		std::string temp{};
		temp.resize(file_size);

		in_file.read(temp.data(), file_size);

		try {
			m_config_repr = json::parse(temp);
			LoadConfig();
		}
		catch (...) {
			fmt::println("[CONFIG] Error while loading {}", path);
		}
	}

	void Config::StoreToFile(std::string const& path) {
		std::ofstream out_file{ path, std::ios::out };

		if (!out_file.is_open()) {
			fmt::println("[CONFIG] Cannot open {}", path);
			return;
		}

		auto data = ConvertConfig();

		out_file << data;
	}

	void Config::LoadConfig() {
		*m_sys_conf = m_config_repr;
	}

	std::string Config::ConvertConfig() {
		return json{ *m_sys_conf }.dump(0);
	}
}