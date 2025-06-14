#pragma once

#include "config_base.hpp"
#include "config_common.hpp"
#include <vector>
#include <string>
#include <memory>

namespace app_hook::config {

/// @brief Base interface for configuration loaders
/// @note Plugins implement this interface to provide configuration loading capabilities
class ConfigLoaderBase {
public:
    virtual ~ConfigLoaderBase() = default;

    /// @brief Get the configuration types this loader supports
    /// @return Vector of supported configuration types
    virtual std::vector<ConfigType> supported_types() const = 0;

    /// @brief Load configurations from a file
    /// @param type Configuration type to load
    /// @param file_path Path to the configuration file
    /// @param task_name Name of the task (for logging and key generation)
    /// @return Vector of configuration objects or error
    virtual ConfigResult<std::vector<ConfigPtr>> load_configs(
        ConfigType type, 
        const std::string& file_path, 
        const std::string& task_name
    ) = 0;

    /// @brief Get loader name for identification
    /// @return Name of this loader
    virtual std::string get_name() const = 0;

    /// @brief Get loader version
    /// @return Version string
    virtual std::string get_version() const = 0;
};

/// @brief Smart pointer type for config loaders
using ConfigLoaderPtr = std::unique_ptr<ConfigLoaderBase>;

} // namespace app_hook::config 