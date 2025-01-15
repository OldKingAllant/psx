#pragma once

#define LOG_DEBUG(filter, message, ...) logger::Logger::get().log<::psx::logger::LogLevel::_DEBUG>(filter, message, __VA_ARGS__)
#define LOG_INFO(filter, message, ...) logger::Logger::get().log<::psx::LogLevel::INFO>(filter, message, __VA_ARGS__)
#define LOG_WARN(filter, message, ...) logger::Logger::get().log<::psx::LogLevel::WARNING>(filter, message, __VA_ARGS__)
#define LOG_ERROR(filter, message, ...) logger::Logger::get().log<::psx::LogLevel::_ERROR>(filter, message, __VA_ARGS__)