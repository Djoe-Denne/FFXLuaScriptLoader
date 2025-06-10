#pragma once

#include "config_common.hpp"
#include "memory_config.hpp"
#include <toml++/toml.h>
#include <vector>
#include <string>
#include <algorithm>
#include <optional>

namespace ff8_hook::config {

/// @brief Loader for memory copy configuration files
/// @note Handles loading and parsing of memory configuration TOML files
class CopyMemoryLoader {
public:
    CopyMemoryLoader() = default;
    ~CopyMemoryLoader() = default;
    
    // Non-copyable but movable (following C++23 best practices)
    CopyMemoryLoader(const CopyMemoryLoader&) = delete;
    CopyMemoryLoader& operator=(const CopyMemoryLoader&) = delete;
    CopyMemoryLoader(CopyMemoryLoader&&) = default;
    CopyMemoryLoader& operator=(CopyMemoryLoader&&) = default;

    /// @brief Load memory copy configurations from a TOML file
    /// @param file_path Path to the configuration file
    /// @return Vector of memory copy configurations or error
    [[nodiscard]] static ConfigResult<std::vector<CopyMemoryConfig>> 
    load_memory_configs(const std::string& file_path) {
        LOG_INFO("Attempting to load configuration from: {}", file_path);
        
        std::vector<CopyMemoryConfig> configs;
        
        try {
            // Parse TOML file
            auto toml = toml::parse_file(file_path);
            LOG_INFO("Successfully parsed TOML file: {}", file_path);
            
            // Process memory sections using ranges (C++23 style)
            auto memory_sections = extract_memory_sections(toml);
            
            for (const auto& [section_key, section_value] : memory_sections) {
                if (auto config = parse_memory_section(section_key, section_value)) {
                    configs.push_back(std::move(*config));
                    LOG_INFO("Added valid config for section: {}", section_key);
                } else {
                    LOG_WARNING("Invalid config for section '{}' - skipping", section_key);
                }
            }
            
        } catch (const toml::parse_error& e) {
            LOG_ERROR("TOML parse error in file '{}': {} at line {}, column {}", 
                     file_path, e.description(), e.source().begin.line, e.source().begin.column);
            return std::unexpected(ConfigError::parse_error);
        } catch (const std::exception& e) {
            LOG_ERROR("Exception while loading config file '{}': {}", file_path, e.what());
            return std::unexpected(ConfigError::parse_error);
        }
        
        LOG_INFO("Successfully loaded {} configuration(s) from file: {}", configs.size(), file_path);
        
        // Log summary of loaded configs
        for (const auto& config : configs) {
            LOG_INFO("Config summary - Key: '{}', Address: 0x{:X}, CopyAfter: 0x{:X}, Size: {} -> {}", 
                     config.key, config.address, config.copy_after, 
                     config.original_size, config.new_size);
        }
        
        return configs;
    }

private:
    /// @brief Extract memory sections from TOML file
    /// @param toml Parsed TOML file
    /// @return Vector of section key-value pairs
    [[nodiscard]] static std::vector<std::pair<std::string, const toml::table*>>
    extract_memory_sections(const toml::table& toml) {
        std::vector<std::pair<std::string, const toml::table*>> sections;
        
        // Handle nested TOML structure: [memory.K_MAGIC] becomes memory -> K_MAGIC
        for (const auto& [key, value] : toml) {
            std::string key_str{key.str()};
            
            // Look for "memory" section
            if (key_str != "memory") {
                LOG_INFO("Skipping non-memory section: {}", key_str);
                continue;
            }
            
            if (!value.is_table()) {
                LOG_WARNING("Section '{}' is not a table, skipping", key_str);
                continue;
            }
            
            // Process all sub-tables within the memory section
            auto memory_table = value.as_table();
            for (const auto& [sub_key, sub_value] : *memory_table) {
                std::string sub_key_str{sub_key.str()};
                std::string full_key = "memory." + sub_key_str;
                
                if (!sub_value.is_table()) {
                    LOG_WARNING("Sub-section '{}' is not a table, skipping", full_key);
                    continue;
                }
                
                sections.emplace_back(full_key, sub_value.as_table());
            }
        }
        
        return sections;
    }
    
    /// @brief Parse a single memory section
    /// @param section_key Section key
    /// @param table TOML table containing memory config
    /// @return Parsed memory config or nullopt if parsing failed
    [[nodiscard]] static std::optional<CopyMemoryConfig> 
    parse_memory_section(const std::string& section_key, const toml::table* table) {
        LOG_INFO("Processing memory section: {}", section_key);
        
        CopyMemoryConfig config;
        config.key = section_key;
        
        // Parse required fields using modern C++ features
        auto parse_field = [&]<typename T>(const std::string& field_name, auto setter) -> bool {
            if (auto field = table->get(field_name)) {
                if (auto value = field->value<T>()) {
                    setter(*value);
                    return true;
                }
            }
            return false;
        };
        
        // Parse address
        if (!parse_field.template operator()<std::string>("address", [&](const std::string& addr_str) {
            config.address = ConfigParsingUtils::parse_address(addr_str);
            LOG_INFO("Set address to: 0x{:X}", config.address);
        })) {
            LOG_WARNING("Missing or invalid 'address' field in section '{}'", section_key);
            return std::nullopt;
        }
        
        // Parse originalSize
        if (!parse_field.template operator()<std::int64_t>("originalSize", [&](std::int64_t size_val) {
            config.original_size = static_cast<std::size_t>(size_val);
            LOG_INFO("Set originalSize to: {}", config.original_size);
        })) {
            LOG_WARNING("Missing or invalid 'originalSize' field in section '{}'", section_key);
            return std::nullopt;
        }
        
        // Parse newSize
        if (!parse_field.template operator()<std::int64_t>("newSize", [&](std::int64_t size_val) {
            config.new_size = static_cast<std::size_t>(size_val);
            LOG_INFO("Set newSize to: {}", config.new_size);
        })) {
            LOG_WARNING("Missing or invalid 'newSize' field in section '{}'", section_key);
            return std::nullopt;
        }
        
        // Parse copyAfter
        if (!parse_field.template operator()<std::string>("copyAfter", [&](const std::string& addr_str) {
            config.copy_after = ConfigParsingUtils::parse_address(addr_str);
            LOG_INFO("Set copyAfter to: 0x{:X}", config.copy_after);
        })) {
            LOG_WARNING("Missing or invalid 'copyAfter' field in section '{}'", section_key);
            return std::nullopt;
        }
        
        // Parse optional fields
        parse_field.template operator()<std::string>("description", [&](const std::string& desc_str) {
            config.description = desc_str;
            LOG_INFO("Set description to: '{}'", config.description);
        });
        
        parse_field.template operator()<std::string>("patch", [&](const std::string& patch_str) {
            config.patch = patch_str;
            LOG_INFO("Set patch file to: '{}'", *config.patch);
        });
        
        // Validate configuration
        LOG_INFO("Validating config for section '{}': address=0x{:X}, copy_after=0x{:X}, original_size={}, new_size={}", 
                 section_key, config.address, config.copy_after, config.original_size, config.new_size);
        
        if (!config.is_valid()) {
            LOG_WARNING("Invalid config for section '{}' - address=0x{:X}, copy_after=0x{:X}, original_size={}, new_size={}", 
                       section_key, config.address, config.copy_after, config.original_size, config.new_size);
            return std::nullopt;
        }
        
        return config;
    }
};

} // namespace ff8_hook::config 