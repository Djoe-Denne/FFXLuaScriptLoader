#pragma once

#include "../memory/copy_memory.hpp"
#include "../util/logger.hpp"
#include <vector>
#include <string>
#include <expected>
#include <fstream>
#include <sstream>
#include <regex>

namespace ff8_hook::config {

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
    [[nodiscard]] static ConfigResult<std::vector<memory::CopyMemoryConfig>> 
    load_memory_configs(const std::string& file_path) {
        LOG_DEBUG("Attempting to load configuration from: {}", file_path);
        
        std::ifstream file(file_path);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open configuration file: {}", file_path);
            return std::unexpected(ConfigError::file_not_found);
        }
        
        LOG_INFO("Successfully opened configuration file: {}", file_path);
        
        std::vector<memory::CopyMemoryConfig> configs;
        std::string line;
        std::string current_section;
        memory::CopyMemoryConfig current_config;
        int line_number = 0;
        
        while (std::getline(file, line)) {
            line_number++;
            
            // Remove leading/trailing whitespace
            line = trim(line);
            
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            LOG_DEBUG("Processing line {}: '{}'", line_number, line);
            
            // Check for section header
            if (line[0] == '[' && line.back() == ']') {
                // Save previous config if valid
                if (!current_section.empty() && current_config.is_valid()) {
                    LOG_DEBUG("Adding valid config for section: {}", current_section);
                    configs.push_back(std::move(current_config));
                    current_config = {};
                } else if (!current_section.empty()) {
                    LOG_WARNING("Skipping invalid config for section: {}", current_section);
                }
                
                current_section = line.substr(1, line.length() - 2);
                LOG_DEBUG("Found section: {}", current_section);
                
                // Only process memory sections
                if (current_section.starts_with("memory.")) {
                    current_config.key = current_section;
                    LOG_DEBUG("Initialized new memory config with key: {}", current_config.key);
                } else {
                    LOG_DEBUG("Skipping non-memory section: {}", current_section);
                }
                continue;
            }
            
            // Parse key-value pairs
            if (current_section.starts_with("memory.")) {
                if (auto kv = parse_key_value(line)) {
                    const auto& [key, value] = *kv;
                    LOG_DEBUG("Parsed key-value: '{}' = '{}'", key, value);
                    
                    if (key == "address") {
                        current_config.address = parse_address(value);
                        LOG_DEBUG("Set address to: 0x{:X}", current_config.address);
                    } else if (key == "originalSize") {
                        current_config.original_size = static_cast<std::size_t>(std::stoull(value));
                        LOG_DEBUG("Set originalSize to: {}", current_config.original_size);
                    } else if (key == "newSize") {
                        current_config.new_size = static_cast<std::size_t>(std::stoull(value));
                        LOG_DEBUG("Set newSize to: {}", current_config.new_size);
                    } else if (key == "copyAfter") {
                        current_config.copy_after = parse_address(value);
                        LOG_DEBUG("Set copyAfter to: 0x{:X}", current_config.copy_after);
                    } else if (key == "description") {
                        current_config.description = parse_string(value);
                        LOG_DEBUG("Set description to: '{}'", current_config.description);
                    } else {
                        LOG_WARNING("Unknown configuration key '{}' in section '{}' at line {}", 
                                   key, current_section, line_number);
                    }
                } else {
                    LOG_WARNING("Failed to parse key-value pair at line {}: '{}'", line_number, line);
                }
            }
        }
        
        // Save the last config if valid
        if (!current_section.empty() && current_config.is_valid()) {
            LOG_DEBUG("Adding final valid config for section: {}", current_section);
            configs.push_back(std::move(current_config));
        } else if (!current_section.empty()) {
            LOG_WARNING("Skipping final invalid config for section: {}", current_section);
        }
        
        LOG_INFO("Successfully loaded {} configuration(s) from file: {}", configs.size(), file_path);
        
        // Log summary of loaded configs
        for (const auto& config : configs) {
            LOG_DEBUG("Config summary - Key: '{}', Address: 0x{:X}, CopyAfter: 0x{:X}, Size: {} -> {}", 
                     config.key, config.address, config.copy_after, 
                     config.original_size, config.new_size);
        }
        
        return configs;
    }
    
private:
    /// @brief Parse a key-value pair from a line
    /// @param line Line to parse
    /// @return Key-value pair or nullopt if invalid
    [[nodiscard]] static std::optional<std::pair<std::string, std::string>> 
    parse_key_value(const std::string& line) {
        const auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            return std::nullopt;
        }
        
        auto key = trim(line.substr(0, eq_pos));
        auto value = trim(line.substr(eq_pos + 1));
        
        return std::make_pair(std::move(key), std::move(value));
    }
    
    /// @brief Parse an address string (supports hex format)
    /// @param value Address string
    /// @return Parsed address
    [[nodiscard]] static std::uintptr_t parse_address(const std::string& value) {
        const auto clean_value = parse_string(value);
        if (clean_value.starts_with("0x") || clean_value.starts_with("0X")) {
            return static_cast<std::uintptr_t>(std::stoull(clean_value, nullptr, 16));
        }
        return static_cast<std::uintptr_t>(std::stoull(clean_value));
    }
    
    /// @brief Parse a string value (removes quotes)
    /// @param value String value
    /// @return Parsed string
    [[nodiscard]] static std::string parse_string(const std::string& value) {
        if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
            return value.substr(1, value.length() - 2);
        }
        return value;
    }
    
    /// @brief Trim whitespace from a string
    /// @param str String to trim
    /// @return Trimmed string
    [[nodiscard]] static std::string trim(const std::string& str) {
        const auto start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            return "";
        }
        const auto end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }
};

} // namespace ff8_hook::config 