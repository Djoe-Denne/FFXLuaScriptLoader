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

namespace ff8_hook::config {

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
    load_configs(ConfigType type, const std::string& config_file, const std::string& task_name) {
        LOG_INFO("Loading {} configs from file: {} for task: {}", 
                 to_string(type), config_file, task_name);

        switch (type) {
            case ConfigType::Memory:
                return load_memory_configs(config_file, task_name);
            
            case ConfigType::Patch:
                return load_patch_configs(config_file, task_name);
            
            case ConfigType::Script:
            case ConfigType::Audio:
            case ConfigType::Graphics:
                LOG_WARNING("Configuration type '{}' is not yet implemented", to_string(type));
                return std::vector<ConfigPtr>{};
            
            case ConfigType::Unknown:
            default:
                LOG_ERROR("Unknown configuration type: {}", static_cast<int>(type));
                return std::unexpected(ConfigError::invalid_format);
        }
    }

private:
    /// @brief Load memory configurations from file
    /// @param config_file Path to the configuration file
    /// @param task_name Name of the task
    /// @return Vector of memory configuration objects or error
    [[nodiscard]] static ConfigResult<std::vector<ConfigPtr>> 
    load_memory_configs(const std::string& config_file, const std::string& task_name) {
        auto memory_configs_result = CopyMemoryLoader::load_memory_configs(config_file);
        if (!memory_configs_result) {
            return std::unexpected(memory_configs_result.error());
        }

        std::vector<ConfigPtr> configs;
        configs.reserve(memory_configs_result->size());

        for (const auto& memory_config : *memory_configs_result) {
            // The CopyMemoryLoader now returns CopyMemoryConfig objects directly
            auto config = std::make_unique<CopyMemoryConfig>(memory_config);
            
            if (config->is_valid()) {
                configs.push_back(std::move(config));
                LOG_DEBUG("Created valid memory config: {}", config->debug_string());
            } else {
                LOG_WARNING("Invalid memory config for key: {}", memory_config.key());
            }
        }

        LOG_INFO("Successfully created {} memory configuration object(s)", configs.size());
        return configs;
    }

    /// @brief Load patch configurations from file
    /// @param config_file Path to the configuration file
    /// @param task_name Name of the task
    /// @return Vector of patch configuration objects or error
    [[nodiscard]] static ConfigResult<std::vector<ConfigPtr>> 
    load_patch_configs(const std::string& config_file, const std::string& task_name) {
        auto patches_result = PatchLoader::load_patch_instructions(config_file);
        if (!patches_result) {
            return std::unexpected(patches_result.error());
        }

        std::vector<ConfigPtr> configs;
        
        if (!patches_result->empty()) {
            auto config = std::make_unique<PatchConfig>(config_file, task_name);
            config->set_patch_file_path(config_file);
            config->set_instructions(std::move(*patches_result));
            config->set_description("Instruction patches from " + config_file);

            if (config->is_valid()) {
                configs.push_back(std::move(config));
                LOG_DEBUG("Created valid patch config: {}", config->debug_string());
            } else {
                LOG_WARNING("Invalid patch config for file: {}", config_file);
            }
        }

        LOG_INFO("Successfully created {} patch configuration object(s)", configs.size());
        return configs;
    }
};

} // namespace ff8_hook::config 