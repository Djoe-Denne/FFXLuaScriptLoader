#pragma once

#include <string>
#include <format>

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

/// @brief Logging wrapper functions (implemented in logger.cpp)
void log_trace(const std::string& message);
void log_debug(const std::string& message);
void log_info(const std::string& message);
void log_warn(const std::string& message);
void log_error(const std::string& message);
void log_critical(const std::string& message);

} // namespace app_hook::util

/// @brief Convenient logging macros that don't require spdlog
#define LOG_TRACE(...) app_hook::util::log_trace(std::format(__VA_ARGS__))
#define LOG_DEBUG(...) app_hook::util::log_debug(std::format(__VA_ARGS__))
#define LOG_INFO(...) app_hook::util::log_info(std::format(__VA_ARGS__))
#define LOG_WARN(...) app_hook::util::log_warn(std::format(__VA_ARGS__))
#define LOG_WARNING(...) app_hook::util::log_warn(std::format(__VA_ARGS__))  // Alias for compatibility
#define LOG_ERROR(...) app_hook::util::log_error(std::format(__VA_ARGS__))
#define LOG_CRITICAL(...) app_hook::util::log_critical(std::format(__VA_ARGS__))
