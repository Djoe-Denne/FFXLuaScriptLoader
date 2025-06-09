#pragma once

#include "memory_config.hpp"
#include "../util/logger.hpp"
#include <toml++/toml.h>
#include <vector>
#include <string>
#include <expected>
#include <cstdint>
#include <optional>
#include <regex>

namespace ff8_hook::config {

/// @brief Single instruction patch data
struct InstructionPatch {
    std::uintptr_t address;           ///< Address to patch
    std::vector<std::uint8_t> bytes;  ///< Instruction bytes with placeholders
    std::int32_t offset;              ///< Offset to apply to new memory base
    
    /// @brief Check if this patch is valid
    [[nodiscard]] bool is_valid() const noexcept {
        return address != 0 && !bytes.empty();
    }
};

/// @brief Error codes for configuration loading
enum class ConfigError {
    success = 0,
    file_not_found,
    parse_error,
    invalid_format,
    missing_required_field
};

/// @brief Configuration loader result type
template<typename T>
using ConfigResult = std::expected<T, ConfigError>;

/// @brief Simple TOML-like configuration loader
/// @note This is a minimal implementation for the specific use case
class ConfigLoader {
public:
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
            
            // Handle nested TOML structure: [memory.K_MAGIC] becomes memory -> K_MAGIC
            for (const auto& [key, value] : toml) {
                std::string key_str(key.str());
                
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
                    std::string sub_key_str(sub_key.str());
                    std::string full_key = "memory." + sub_key_str;
                    
                    if (!sub_value.is_table()) {
                        LOG_WARNING("Sub-section '{}' is not a table, skipping", full_key);
                        continue;
                    }
                    
                    LOG_INFO("Processing memory section: {}", full_key);
                    
                    CopyMemoryConfig config;
                    config.key = full_key;
                    
                    auto table = sub_value.as_table();
                
                // Parse required fields
                if (auto addr = table->get("address")) {
                    if (auto addr_str = addr->value<std::string>()) {
                        config.address = parse_address(*addr_str);
                        LOG_INFO("Set address to: 0x{:X}", config.address);
                    }
                }
                
                if (auto orig_size = table->get("originalSize")) {
                    if (auto size_val = orig_size->value<std::int64_t>()) {
                        config.original_size = static_cast<std::size_t>(*size_val);
                        LOG_INFO("Set originalSize to: {}", config.original_size);
                    }
                }
                
                if (auto new_size = table->get("newSize")) {
                    if (auto size_val = new_size->value<std::int64_t>()) {
                        config.new_size = static_cast<std::size_t>(*size_val);
                        LOG_INFO("Set newSize to: {}", config.new_size);
                    }
                }
                
                if (auto copy_after = table->get("copyAfter")) {
                    if (auto addr_str = copy_after->value<std::string>()) {
                        config.copy_after = parse_address(*addr_str);
                        LOG_INFO("Set copyAfter to: 0x{:X}", config.copy_after);
                    }
                }
                
                // Parse optional fields
                if (auto desc = table->get("description")) {
                    if (auto desc_str = desc->value<std::string>()) {
                        config.description = *desc_str;
                        LOG_INFO("Set description to: '{}'", config.description);
                    }
                }
                
                if (auto patch = table->get("patch")) {
                    if (auto patch_str = patch->value<std::string>()) {
                        config.patch = *patch_str;
                        LOG_INFO("Set patch file to: '{}'", *config.patch);
                    }
                }
                
                    // Validate and add config
                    LOG_INFO("Validating config for section '{}': address=0x{:X}, copy_after=0x{:X}, original_size={}, new_size={}", 
                             full_key, config.address, config.copy_after, config.original_size, config.new_size);
                    
                    if (config.is_valid()) {
                        configs.push_back(std::move(config));
                        LOG_INFO("Added valid config for section: {}", full_key);
                    } else {
                        LOG_WARNING("Invalid config for section '{}' - address=0x{:X}, copy_after=0x{:X}, original_size={}, new_size={}", 
                                   full_key, config.address, config.copy_after, config.original_size, config.new_size);
                    }
                } // End sub-table loop
            } // End main loop
            
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
    
    /// @brief Load patch instructions from a TOML file
    /// @param file_path Path to the patch file
    /// @return Vector of instruction patches or error
    [[nodiscard]] static ConfigResult<std::vector<InstructionPatch>> 
    load_patch_instructions(const std::string& file_path) {
        LOG_INFO("Loading patch instructions from: {}", file_path);
        
        std::vector<InstructionPatch> patches;
        
        try {
            // Parse TOML file
            auto toml = toml::parse_file(file_path);
            LOG_INFO("Successfully parsed TOML file: {}", file_path);
            
            // Look for instructions table
            auto instructions = toml["instructions"];
            if (!instructions) {
                LOG_WARNING("No 'instructions' table found in TOML file: {}", file_path);
                return patches; // Return empty vector
            }
            
            if (!instructions.is_table()) {
                LOG_ERROR("'instructions' is not a table in TOML file: {}", file_path);
                return std::unexpected(ConfigError::invalid_format);
            }
            
            // Iterate through instruction entries manually
            auto instructions_table = instructions.as_table();
            for (const auto& [key, value] : *instructions_table) {
                std::string key_str(key.str());
                LOG_DEBUG("Processing instruction key: '{}'", key_str);
                
                if (!value.is_table()) {
                    LOG_WARNING("Instruction '{}' is not a table, skipping", key_str);
                    continue;
                }
                
                InstructionPatch patch;
                
                // Parse address from key (format: 0x1234ABCD)
                patch.address = parse_address(key_str);
                LOG_DEBUG("Parsed address from key '{}': 0x{:X}", key_str, patch.address);
                
                auto table = value.as_table();
                
                // Parse bytes field
                if (auto bytes_node = table->get("bytes")) {
                    if (auto bytes_str = bytes_node->value<std::string>()) {
                        patch.bytes = parse_bytes_string(*bytes_str);
                        LOG_INFO("Parsed bytes: '{}' -> {} byte(s)", *bytes_str, patch.bytes.size());
                    } else {
                        LOG_WARNING("'bytes' field is not a string for instruction '{}'", key_str);
                        continue;
                    }
                } else {
                    LOG_WARNING("Missing 'bytes' field for instruction '{}'", key_str);
                    continue;
                }
                
                // Parse offset field
                if (auto offset_node = table->get("offset")) {
                    if (auto offset_str = offset_node->value<std::string>()) {
                        patch.offset = parse_offset(*offset_str);
                        LOG_DEBUG("Parsed offset: '{}' -> {}", *offset_str, patch.offset);
                    } else {
                        LOG_WARNING("'offset' field is not a string for instruction '{}'", key_str);
                        continue;
                    }
                } else {
                    LOG_WARNING("Missing 'offset' field for instruction '{}'", key_str);
                    continue;
                }
                
                // Validate and add patch
                if (patch.is_valid()) {
                    patches.push_back(std::move(patch));
                    LOG_DEBUG("Added valid patch for instruction '{}'", key_str);
                } else {
                    LOG_WARNING("Invalid patch for instruction '{}' - address: 0x{:X}, bytes: {}", 
                               key_str, patch.address, patch.bytes.size());
                }
            }
            
        } catch (const toml::parse_error& e) {
            LOG_ERROR("TOML parse error in file '{}': {} at line {}, column {}", 
                     file_path, e.description(), e.source().begin.line, e.source().begin.column);
            return std::unexpected(ConfigError::parse_error);
        } catch (const std::exception& e) {
            LOG_ERROR("Exception while loading patch file '{}': {}", file_path, e.what());
            return std::unexpected(ConfigError::parse_error);
        }
        
        LOG_INFO("Loaded {} patch instruction(s) from file: {}", patches.size(), file_path);
        
        // Log summary of loaded patches
        for (size_t i = 0; i < patches.size(); ++i) {
            LOG_DEBUG("Patch {}: address=0x{:X}, offset={}, bytes={}", 
                     i, patches[i].address, patches[i].offset, patches[i].bytes.size());
        }
        
        return patches;
    }
    
private:
    /// @brief Parse an address string (supports hex format)
    /// @param value Address string
    /// @return Parsed address
    [[nodiscard]] static std::uintptr_t parse_address(const std::string& value) {
        if (value.starts_with("0x") || value.starts_with("0X")) {
            return static_cast<std::uintptr_t>(std::stoull(value, nullptr, 16));
        }
        return static_cast<std::uintptr_t>(std::stoull(value));
    }
    
    /// @brief Parse bytes string like "8D 86 XX XX XX XX" into byte array
    /// @param bytes_str String of hex bytes
    /// @return Vector of bytes with 0xFF for 'XX' placeholders
    [[nodiscard]] static std::vector<std::uint8_t> parse_bytes_string(const std::string& bytes_str) {
        std::vector<std::uint8_t> bytes;
        std::regex byte_regex("[0-9A-Fa-f]{2}|XX");
        std::sregex_iterator iter(bytes_str.begin(), bytes_str.end(), byte_regex);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
            const std::string byte_str = iter->str();
            if (byte_str == "XX") {
                bytes.push_back(0xFF); // Placeholder
            } else {
                bytes.push_back(static_cast<std::uint8_t>(std::stoul(byte_str, nullptr, 16)));
            }
        }
        
        return bytes;
    }
    
    /// @brief Parse offset string like "0x2A" or "-0x10"
    /// @param offset_str Offset string
    /// @return Parsed offset value
    [[nodiscard]] static std::int32_t parse_offset(const std::string& offset_str) {
        if (offset_str.starts_with("-")) {
            const auto positive_part = offset_str.substr(1);
            if (positive_part.starts_with("0x") || positive_part.starts_with("0X")) {
                return -static_cast<std::int32_t>(std::stoul(positive_part, nullptr, 16));
            }
            return -static_cast<std::int32_t>(std::stoul(positive_part));
        }
        
        if (offset_str.starts_with("0x") || offset_str.starts_with("0X")) {
            return static_cast<std::int32_t>(std::stoul(offset_str, nullptr, 16));
        }
        return static_cast<std::int32_t>(std::stoul(offset_str));
    }


};

} // namespace ff8_hook::config 