#pragma once

#include <config/config_loader_base.hpp>
#include "memory_config.hpp"
#include <memory>
#include <optional>
#include <toml++/toml.hpp>

namespace memory_plugin {

/// @brief Memory operations configuration loader
/// @note Implements ConfigLoaderBase to provide memory configuration loading only
class MemoryConfigLoader : public app_hook::config::ConfigLoaderBase {
public:
    MemoryConfigLoader() = default;
    ~MemoryConfigLoader() override = default;

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
    /// @brief Load memory configurations from file
    app_hook::config::ConfigResult<std::vector<app_hook::config::ConfigPtr>> 
    load_memory_configs(const std::string& file_path, const std::string& task_name);

    /// @brief Parse a single memory operation from TOML
    app_hook::config::ConfigPtr parse_memory_operation(const toml::node& op, const std::string& task_name, const std::string& config_name = "");

    /// @brief Parse address string (supports hex format)
    static std::uintptr_t parse_address(const std::string& value);
};

} // namespace memory_plugin 