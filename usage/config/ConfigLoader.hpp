#pragma once

#include <psxemu/include/psxemu/SystemConf.hpp>

#include <thirdparty/json/single_include/nlohmann/json.hpp>

#include <string>
#include <memory>

namespace config {
	using json = nlohmann::json;

	class Config {
	public :
		Config();

		auto GetSystemConfig() {
			return m_sys_conf;
		}

		void LoadFromFile(std::string const& path);
		void StoreToFile(std::string const& path);

	private :
		void LoadConfig();
		std::string ConvertConfig();

	private :
		std::shared_ptr<psx::SystemConf> m_sys_conf;
		json m_config_repr;
	};
}