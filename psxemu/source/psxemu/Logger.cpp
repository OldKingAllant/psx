#pragma once

#include <psxemu/include/psxemu/Logger.hpp>

#include <thirdparty/quill/include/quill/Logger.h>
#include <thirdparty/quill/include/quill/Backend.h>
#include <thirdparty/quill/include/quill/Frontend.h>
#include <thirdparty/quill/include/quill/LogMacros.h>
#include <thirdparty/quill/include/quill/sinks/ConsoleSink.h>
#include <thirdparty/quill/include/quill/sinks/FileSink.h>

namespace psx::logger {

	struct LoggerImpl {
		quill::Logger* logger;
	};

	Logger::Logger() :
		m_log{ new LoggerImpl{} },
		m_conf{},
		m_running{}
	{}

	void Logger::start() {
		quill::BackendOptions backend_opts{};
		backend_opts.enable_yield_when_idle = m_conf.enable_yield;
		backend_opts.wait_for_queues_to_empty_before_exit = true;

		quill::Backend::start(backend_opts);

		auto console = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console_sink");
		auto file = quill::Frontend::create_or_get_sink<quill::FileSink>(m_conf.file_path, []() {
			quill::FileSinkConfig sink_conf{};

			sink_conf.set_open_mode('w');

			return sink_conf;
		}());

		std::vector<std::shared_ptr<quill::Sink>> sinks{};

		if (m_conf.log_to_console)
			sinks.push_back(console);

		if (m_conf.log_to_file)
			sinks.push_back(file);

		auto pattern_opts = quill::PatternFormatterOptions(
			"%(short_source_location:<14) %(log_level:<9) %(message)"
		);

		m_log->logger = quill::Frontend::create_or_get_logger("main_logger", sinks, pattern_opts);
		m_log->logger->set_log_level(quill::LogLevel::TraceL3);

		

		m_running = true;
	}

	void Logger::stop() {
		quill::Backend::stop();
		m_running = false;
		delete m_log;
	}

	bool Logger::is_filtered(std::string_view log_name) {
		return m_conf.filters.contains(std::string(log_name));
	}

	void Logger::log_impl(LogLevel level, std::string const& message) {
		if (!m_conf.enable)
			return;

		if (uint32_t(m_conf.log_level) < uint32_t(level))
			return;

		switch (level)
		{
		case psx::logger::LogLevel::_ERROR:
			LOG_ERROR(m_log->logger, "{}", message);
			break;
		case psx::logger::LogLevel::WARN:
			LOG_WARNING(m_log->logger, "{}", message);
			break;
		case psx::logger::LogLevel::INFO:
			LOG_INFO(m_log->logger, "{}", message);
			break;
		case psx::logger::LogLevel::_DEBUG:
			LOG_DEBUG(m_log->logger, "{}", message);
			break;
		default:
			break;
		}
	}

	Logger& Logger::get() {
		static Logger logger_instance = Logger();

		return logger_instance;
	}

	Logger::~Logger() {
		if (m_running) {
			stop();
		}	
	}
}