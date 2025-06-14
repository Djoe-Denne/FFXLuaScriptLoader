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
    load_memory_configs(const std::string& file_path);

private:
    /// @brief Extract memory sections from TOML file
    /// @param toml Parsed TOML file
    /// @return Vector of section key-value pairs
    [[nodiscard]] static std::vector<std::pair<std::string, const toml::table*>>
    extract_memory_sections(const toml::table& toml);
    
    /// @brief Parse a single memory section
    /// @param section_key Section key
    /// @param table TOML table containing memory config
    /// @return Parsed memory config or nullopt if parsing failed
    [[nodiscard]] static std::optional<CopyMemoryConfig> 
    parse_memory_section(const std::string& section_key, const toml::table* table);
};

} // namespace ff8_hook::config 