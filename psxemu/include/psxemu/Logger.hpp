#pragma once

#include "LoggerConfig.hpp"

#include <string>
#include <string_view>

#include <fmt/format.h>

namespace psx::logger {
	struct LoggerImpl;

	class Logger {
	public :
		static Logger& get();

		void set_config(LoggerConfig const& conf) {
			m_conf = conf;
		}

		void start();
		void stop();

		bool is_log_enabled() const {
			return m_conf.enable;
		}

		bool is_filtered(std::string_view log_name);

		template <LogLevel level, typename... Args>
		void log(std::string_view log_name, std::string_view format, Args&&... args) {
			if (is_filtered(log_name))
				return;

			if (!m_conf.enable)
				return;

			if (uint32_t(m_conf.log_level) < uint32_t(level))
				return;

			auto formatted_string = fmt::vformat(format, fmt::make_format_args(args...));

			log_impl(level, formatted_string);
		}

		void flush();

		~Logger();

	private :
		Logger();

		void log_impl(LogLevel level, std::string const& message);

	private :
		LoggerImpl* m_log;
		LoggerConfig m_conf;

		bool m_running;
	};
}