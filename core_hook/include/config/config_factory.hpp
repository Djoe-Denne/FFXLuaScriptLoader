#pragma once

#include "config_base.hpp"
#include "config_loader_base.hpp"
#include "config_common.hpp"
#include "../util/logger.hpp"
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

namespace app_hook::config {

/// @brief Factory for creating configuration objects
/// @note Handles polymorphic creation based on configuration type using registered loaders
class ConfigFactory {
public:
    ConfigFactory() = default;
    ~ConfigFactory() = default;

    // Non-copyable but movable (following C++23 best practices)
    ConfigFactory(const ConfigFactory&) = delete;
    ConfigFactory& operator=(const ConfigFactory&) = delete;
    ConfigFactory(ConfigFactory&&) = default;
    ConfigFactory& operator=(ConfigFactory&&) = default;

    /// @brief Register a configuration loader from a plugin
    /// @param loader Configuration loader to register
    /// @return True if registered successfully
    [[nodiscard]] static bool register_loader(ConfigLoaderPtr loader);

    /// @brief Unregister a configuration loader by name
    /// @param loader_name Name of the loader to unregister
    /// @return True if unregistered successfully
    [[nodiscard]] static bool unregister_loader(const std::string& loader_name);

    /// @brief Load configuration from file based on type
    /// @param type Configuration type to load
    /// @param config_file Path to the configuration file
    /// @param task_name Name of the task (for logging and key generation)
    /// @return Vector of configuration objects or error
    [[nodiscard]] static ConfigResult<std::vector<ConfigPtr>> 
    load_configs(ConfigType type, const std::string& config_file, const std::string& task_name);

    /// @brief Get list of registered loader names
    /// @return Vector of loader names
    [[nodiscard]] static std::vector<std::string> get_registered_loaders();

    /// @brief Check if a configuration type is supported
    /// @param type Configuration type to check
    /// @return True if supported by any registered loader
    [[nodiscard]] static bool is_type_supported(ConfigType type);

private:
    /// @brief Map of registered configuration loaders
    static std::unordered_map<std::string, ConfigLoaderPtr> loaders_;

    /// @brief Find loader that supports the given type
    /// @param type Configuration type
    /// @return Pointer to loader or nullptr if not found
    [[nodiscard]] static ConfigLoaderBase* find_loader_for_type(ConfigType type);
};

} // namespace app_hook::config 
