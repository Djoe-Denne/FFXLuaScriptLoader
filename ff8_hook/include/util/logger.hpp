#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <filesystem>
#include <memory>

namespace ff8_hook::util {

/// @brief Initialize logging system with file and console output
/// @param log_file_path Path to log file
/// @param level Log level (debug, info, warn, error)
/// @return true if successful
inline bool initialize_logging(const std::string& log_file_path, spdlog::level::level_enum level = spdlog::level::info) {
    try {
        // Create directory if it doesn't exist
        const auto log_path = std::filesystem::path(log_file_path);
        if (const auto parent = log_path.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        
        // Create sinks for file and console
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path, false); // false = don't truncate
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        
        // Configure formatting
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");
        console_sink->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
        
        // Create logger with both sinks
        auto logger = std::make_shared<spdlog::logger>("ff8_hook", spdlog::sinks_init_list{file_sink, console_sink});
        logger->set_level(level);
        logger->flush_on(spdlog::level::info); // Auto-flush on info level and above
        
        // Set as default logger
        spdlog::set_default_logger(logger);
        
        spdlog::info("FF8 Hook logging system initialized");
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

/// @brief Close logging system
inline void shutdown_logging() {
    spdlog::info("FF8 Hook logging system shutting down");
    spdlog::shutdown();
}

} // namespace ff8_hook::util

/// @brief Convenient logging macros using spdlog
#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define LOG_WARNING(...) SPDLOG_WARN(__VA_ARGS__)  // Alias for compatibility
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)
