#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <source_location>

// Include the actual headers for config types  
#include "config/config_base.hpp"
#include "config/config_loader_base.hpp"
#include "task/hook_task.hpp"
#include "context/mod_context.hpp"

namespace app_hook::plugin {

/// @brief Plugin API version for compatibility checking
constexpr std::uint32_t PLUGIN_API_VERSION = 1;

/// @brief Plugin information structure
struct PluginInfo {
    std::string name;           ///< Plugin name
    std::string version;        ///< Plugin version
    std::string description;    ///< Plugin description
    std::uint32_t api_version;  ///< Required API version
};

/// @brief Plugin result codes
enum class PluginResult : std::uint8_t {
    Success = 0,
    Failed,
    InvalidVersion,
    InvalidConfig,
    AlreadyLoaded,
    NotFound
};

/// @brief Convert PluginResult to string
[[nodiscard]] constexpr const char* to_string(PluginResult result) noexcept {
    switch (result) {
        case PluginResult::Success:        return "Success";
        case PluginResult::Failed:         return "Failed";
        case PluginResult::InvalidVersion: return "Invalid Version";
        case PluginResult::InvalidConfig:  return "Invalid Config";
        case PluginResult::AlreadyLoaded:  return "Already Loaded";
        case PluginResult::NotFound:       return "Not Found";
        default:                          return "Unknown";
    }
}

// Forward declarations are not needed since we include the actual headers above

/// @brief Plugin host interface - exposed by app_hook to plugins
class IPluginHost {
public:
    virtual ~IPluginHost() = default;

    /// @brief Register a configuration with the host
    /// @param config Configuration to register
    /// @return Success if registered
    virtual PluginResult register_config(std::unique_ptr<::app_hook::config::ConfigBase> config) = 0;

    /// @brief Register a configuration loader with the host
    /// @param loader Configuration loader to register
    /// @return Success if registered
    virtual PluginResult register_config_loader(std::unique_ptr<::app_hook::config::ConfigLoaderBase> loader) = 0;

    /// @brief Register a task creator with the host
    /// @param config_type_name Name of the configuration type
    /// @param creator Function to create tasks from this config type
    /// @return Success if registered
    virtual PluginResult register_task_creator(const std::string& config_type_name, 
                                                std::function<std::unique_ptr<::app_hook::task::IHookTask>(const ::app_hook::config::ConfigBase&)> creator) = 0;

    /// @brief Get the plugin data directory path
    /// @return Path to plugin data directory
    virtual std::string get_plugin_data_path() const = 0;

    /// @brief Log a message through the host's logging system
    /// @param level Log level (0=trace, 1=debug, 2=info, 3=warn, 4=error, 5=critical)
    /// @param message Message to log
    virtual void log_message(int level, const std::string& message) = 0;

    /// @brief Log a message through the host's logging system with source location
    /// @param level Log level (0=trace, 1=debug, 2=info, 3=warn, 4=error, 5=critical)
    /// @param message Message to log
    /// @param location Source location information
    virtual void log_message_with_location(int level, const std::string& message, const std::source_location& location) = 0;

    /// @brief Get current process information
    /// @return Process handle and ID
    virtual std::pair<void*, std::uint32_t> get_process_info() const = 0;
    
    /// @brief Get access to the host's ModContext instance
    /// @return Reference to the host's ModContext
    virtual ::app_hook::context::ModContext& get_mod_context() = 0;
};

/// @brief Plugin interface - implemented by plugins
class IPlugin {
public:
    virtual ~IPlugin() = default;

    /// @brief Get plugin information
    /// @return Plugin info structure
    virtual PluginInfo get_plugin_info() const = 0;

    /// @brief Initialize the plugin
    /// @param host Plugin host interface
    /// @return Success if initialized
    virtual PluginResult initialize(IPluginHost* host) = 0;

    /// @brief Load configuration files for this plugin
    /// @param config_path Base configuration path
    /// @return Success if configurations loaded
    virtual PluginResult load_configurations(const std::string& config_path) = 0;

    /// @brief Shutdown the plugin
    virtual void shutdown() = 0;
};

/// @brief Plugin factory function signature
/// @return Created plugin instance
using CreatePluginFunc = IPlugin*(*)();

/// @brief Plugin destruction function signature
/// @param plugin Plugin to destroy
using DestroyPluginFunc = void(*)(IPlugin*);

} // namespace app_hook::plugin

/// @brief Plugin export macros
#define PLUGIN_EXPORT extern "C" __declspec(dllexport)

/// @brief Standard plugin entry points
#define PLUGIN_CREATE_FUNCTION_NAME "CreatePlugin"
#define PLUGIN_DESTROY_FUNCTION_NAME "DestroyPlugin"

/// @brief Convenience macro for plugin exports
#define EXPORT_PLUGIN(PluginClass) \
    PLUGIN_EXPORT app_hook::plugin::IPlugin* CreatePlugin() { \
        return new PluginClass(); \
    } \
    PLUGIN_EXPORT void DestroyPlugin(app_hook::plugin::IPlugin* plugin) { \
        delete plugin; \
    }

/// @brief Plugin logging macros - use these in plugins to log through host
/// @note These require a 'host_' member variable of type IPluginHost*
#define PLUGIN_LOG_TRACE(...)   if (host_) host_->log_message_with_location(0, std::format(__VA_ARGS__), std::source_location::current())
#define PLUGIN_LOG_DEBUG(...)   if (host_) host_->log_message_with_location(1, std::format(__VA_ARGS__), std::source_location::current())
#define PLUGIN_LOG_INFO(...)    if (host_) host_->log_message_with_location(2, std::format(__VA_ARGS__), std::source_location::current())
#define PLUGIN_LOG_WARN(...)    if (host_) host_->log_message_with_location(3, std::format(__VA_ARGS__), std::source_location::current())
#define PLUGIN_LOG_ERROR(...)   if (host_) host_->log_message_with_location(4, std::format(__VA_ARGS__), std::source_location::current())
#define PLUGIN_LOG_CRITICAL(...) if (host_) host_->log_message_with_location(5, std::format(__VA_ARGS__), std::source_location::current()) 