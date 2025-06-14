#pragma once

#include "../util/logger.hpp"
#include <expected>
#include <string>
#include <cstdint>

namespace app_hook::config {

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

/// @brief Common parsing utilities for configuration loaders
class ConfigParsingUtils {
public:
    /// @brief Parse an address string (supports hex format)
    /// @param value Address string
    /// @return Parsed address
    [[nodiscard]] static constexpr std::uintptr_t parse_address(const std::string& value) {
        if (value.starts_with("0x") || value.starts_with("0X")) {
            return static_cast<std::uintptr_t>(std::stoull(value, nullptr, 16));
        }
        return static_cast<std::uintptr_t>(std::stoull(value));
    }
    
    /// @brief Parse offset string like "0x2A" or "-0x10"
    /// @param offset_str Offset string
    /// @return Parsed offset value
    [[nodiscard]] static constexpr std::int32_t parse_offset(const std::string& offset_str) {
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

} // namespace app_hook::config 
