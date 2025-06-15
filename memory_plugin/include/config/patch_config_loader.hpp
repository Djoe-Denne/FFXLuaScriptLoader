#pragma once

#include <config/config_loader_base.hpp>
#include "patch_config.hpp"
#include <toml++/toml.hpp>
#include <vector>
#include <string>
#include <regex>
#include <algorithm>
#include <optional>

// Forward declaration
namespace app_hook::plugin { class IPluginHost; }

namespace memory_plugin {

/// @brief Patch operations configuration loader
/// @note Implements ConfigLoaderBase to provide patch configuration loading
class PatchConfigLoader : public app_hook::config::ConfigLoaderBase {
public:
    PatchConfigLoader() = default;
    ~PatchConfigLoader() override = default;

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
    /// @brief Load patch configurations from file
    app_hook::config::ConfigResult<std::vector<app_hook::config::ConfigPtr>> 
    load_patch_configs(const std::string& file_path, const std::string& task_name);

    /// @brief Parse a single instruction entry from TOML
    std::optional<app_hook::config::InstructionPatch> 
    parse_single_instruction(const std::string& key_str, const toml::node& value);
    
    /// @brief Parse bytes string like "8D 86 XX XX XX XX" into byte array
    std::vector<std::uint8_t> parse_bytes_string(const std::string& bytes_str);

    /// @brief Parse address string (supports hex format)
    static std::uintptr_t parse_address(const std::string& value);

    /// @brief Parse offset string like "0x2A" or "-0x10"
    static std::int32_t parse_offset(const std::string& offset_str);

    /// @brief Plugin host for logging
    app_hook::plugin::IPluginHost* host_ = nullptr;
};

} // namespace memory_plugin 