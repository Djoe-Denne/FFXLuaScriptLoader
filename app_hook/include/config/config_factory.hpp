#pragma once

#include "config_base.hpp"
#include "memory_config.hpp"
#include "patch_config.hpp"
#include "copy_memory_loader.hpp"
#include "patch_loader.hpp"
#include "config_common.hpp"
#include "../util/logger.hpp"
#include <vector>
#include <memory>

namespace app_hook::config {

/// @brief Factory for creating configuration objects
/// @note Handles polymorphic creation based on configuration type
class ConfigFactory {
public:
    ConfigFactory() = default;
    ~ConfigFactory() = default;

    // Non-copyable but movable (following C++23 best practices)
    ConfigFactory(const ConfigFactory&) = delete;
    ConfigFactory& operator=(const ConfigFactory&) = delete;
    ConfigFactory(ConfigFactory&&) = default;
    ConfigFactory& operator=(ConfigFactory&&) = default;

    /// @brief Load configuration from file based on type
    /// @param type Configuration type to load
    /// @param config_file Path to the configuration file
    /// @param task_name Name of the task (for logging and key generation)
    /// @return Vector of configuration objects or error
    [[nodiscard]] static ConfigResult<std::vector<ConfigPtr>> 
    load_configs(ConfigType type, const std::string& config_file, const std::string& task_name);

private:
    /// @brief Load memory configurations from file
    /// @param config_file Path to the configuration file
    /// @param task_name Name of the task
    /// @return Vector of memory configuration objects or error
    [[nodiscard]] static ConfigResult<std::vector<ConfigPtr>> 
    load_memory_configs(const std::string& config_file, const std::string& task_name);

    /// @brief Load patch configurations from file
    /// @param config_file Path to the configuration file
    /// @param task_name Name of the task
    /// @return Vector of patch configuration objects or error
    [[nodiscard]] static ConfigResult<std::vector<ConfigPtr>> 
    load_patch_configs(const std::string& config_file, const std::string& task_name);
};

} // namespace app_hook::config 
