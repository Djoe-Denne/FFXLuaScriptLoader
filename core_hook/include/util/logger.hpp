#pragma once

#include <string>
#include <format>
#include <spdlog/spdlog.h>

namespace app_hook::util {

/// @brief Log levels (matches spdlog levels)
enum class LogLevel : int {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Critical = 5
};

/// @brief Initialize logging system with file and console output
/// @param log_file_path Path to log file
/// @param level Log level (0=trace, 1=debug, 2=info, 3=warn, 4=error, 5=critical)
/// @return true if successful
bool initialize_logging(const std::string& log_file_path, int level = 2);

/// @brief Close logging system
void shutdown_logging();

} // namespace app_hook::util

/// @brief Convenient logging macros that capture source location
#define LOG_TRACE(...) SPDLOG_TRACE(std::format(__VA_ARGS__))
#define LOG_DEBUG(...) SPDLOG_DEBUG(std::format(__VA_ARGS__))
#define LOG_INFO(...) SPDLOG_INFO(std::format(__VA_ARGS__))
#define LOG_WARN(...) SPDLOG_WARN(std::format(__VA_ARGS__))
#define LOG_WARNING(...) SPDLOG_WARN(std::format(__VA_ARGS__))  // Alias for compatibility
#define LOG_ERROR(...) SPDLOG_ERROR(std::format(__VA_ARGS__))
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(std::format(__VA_ARGS__))
