#include "../../include/util/logger.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <filesystem>
#include <memory>

namespace app_hook::util {

bool initialize_logging(const std::string& log_file_path, int level) {
    try {
        // Create directory if it doesn't exist
        const auto log_path = std::filesystem::path(log_file_path);
        if (const auto parent = log_path.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        
        // Map our simple int levels to spdlog levels
        spdlog::level::level_enum spdlog_level;
        switch (level) {
            case 0: spdlog_level = spdlog::level::trace; break;
            case 1: spdlog_level = spdlog::level::debug; break;
            case 2: spdlog_level = spdlog::level::info; break;
            case 3: spdlog_level = spdlog::level::warn; break;
            case 4: spdlog_level = spdlog::level::err; break;
            case 5: spdlog_level = spdlog::level::critical; break;
            default: spdlog_level = spdlog::level::info; break;
        }
        
        // Create sinks for file and console
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path, false); // false = don't truncate
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        
        // Configure formatting
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");
        console_sink->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
        
        // Set level for both sinks explicitly
        file_sink->set_level(spdlog_level);
        console_sink->set_level(spdlog_level);
        
        // Create logger with both sinks
        auto logger = std::make_shared<spdlog::logger>("app_hook", spdlog::sinks_init_list{file_sink, console_sink});
        logger->set_level(spdlog_level);
        logger->flush_on(spdlog::level::debug); // Auto-flush on debug level and above
        
        // Set as default logger
        spdlog::set_default_logger(logger);
        
        // Set global log level as well (just to be sure)
        spdlog::set_level(spdlog_level);
        
        spdlog::info("FF8 Hook logging system initialized with level: {}", spdlog::level::to_string_view(spdlog_level));
        spdlog::debug("Debug logging test - this should appear if debug level is active");
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

void shutdown_logging() {
    spdlog::info("FF8 Hook logging system shutting down");
    spdlog::shutdown();
}

// Wrapper functions for logging that don't expose spdlog
void log_trace(const std::string& message) {
    spdlog::trace(message);
}

void log_debug(const std::string& message) {
    spdlog::debug(message);
}

void log_info(const std::string& message) {
    spdlog::info(message);
}

void log_warn(const std::string& message) {
    spdlog::warn(message);
}

void log_error(const std::string& message) {
    spdlog::error(message);
}

void log_critical(const std::string& message) {
    spdlog::critical(message);
}

} // namespace app_hook::util 