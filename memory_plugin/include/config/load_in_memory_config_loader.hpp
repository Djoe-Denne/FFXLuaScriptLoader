#pragma once

#include <config/config_loader_base.hpp>
#include "load_in_memory_config.hpp"
#include <memory>
#include <optional>
#include <toml++/toml.hpp>

// Forward declaration
namespace app_hook::plugin { class IPluginHost; }

namespace memory_plugin {

/// @brief Load in memory operations configuration loader
/// @note Implements ConfigLoaderBase to provide load in memory configuration loading only
class LoadInMemoryConfigLoader : public app_hook::config::ConfigLoaderBase {
public:
    LoadInMemoryConfigLoader() = default;
    ~LoadInMemoryConfigLoader() override = default;

    /// @brief Set the plugin host for logging
    void setHost(app_hook::plugin::IPluginHost* host) { host_ = host; }

    // ConfigLoaderBase interface
    std::vector<app_hook::config::ConfigType> supported_types() const override;
    
    app_hook::config::ConfigResult<std::vector<app_hook::config::ConfigPtr>> load_configs(
        app_hook::config::ConfigType type, 
        const std::string& file_path, 
        const std::string& task_name
    ) override;

    std::string get_name() const override;
    std::string get_version() const override;

private:
    /// @brief Load load in memory configurations from file
    app_hook::config::ConfigResult<std::vector<app_hook::config::ConfigPtr>> 
    load_load_in_memory_configs(const std::string& file_path, const std::string& task_name);

    /// @brief Parse a single load operation from TOML
    app_hook::config::ConfigPtr parse_load_operation(const toml::node& op, const std::string& task_name, const std::string& config_name = "");

    /// @brief Parse address string (supports hex format)
    static std::uintptr_t parse_address(const std::string& value);

    /// @brief Plugin host for logging
    app_hook::plugin::IPluginHost* host_ = nullptr;
};

} // namespace memory_plugin 