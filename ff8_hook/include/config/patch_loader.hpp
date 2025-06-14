#pragma once

#include "config_common.hpp"
#include "patch_config.hpp"  // For InstructionPatch definition
#include <toml++/toml.h>
#include <vector>
#include <string>
#include <regex>
#include <algorithm>

namespace ff8_hook::config {

// InstructionPatch is defined in patch_config.hpp

/// @brief Loader for patch instruction files
/// @note Handles loading and parsing of patch instruction TOML files
class PatchLoader {
public:
    PatchLoader() = default;
    ~PatchLoader() = default;
    
    // Non-copyable but movable (following C++23 best practices)
    PatchLoader(const PatchLoader&) = delete;
    PatchLoader& operator=(const PatchLoader&) = delete;
    PatchLoader(PatchLoader&&) = default;
    PatchLoader& operator=(PatchLoader&&) = default;

    /// @brief Load patch instructions from a TOML file
    /// @param file_path Path to the patch file
    /// @return Vector of instruction patches or error
    [[nodiscard]] static ConfigResult<std::vector<InstructionPatch>> 
    load_patch_instructions(const std::string& file_path);

private:
    /// @brief Parse a single instruction entry from TOML
    /// @param key_str Instruction key (address)
    /// @param value TOML value containing instruction data
    /// @return Parsed instruction patch or nullopt if parsing failed
    [[nodiscard]] static std::optional<InstructionPatch> 
    parse_single_instruction(const std::string& key_str, const toml::node& value);
    
    /// @brief Parse bytes string like "8D 86 XX XX XX XX" into byte array
    /// @param bytes_str String of hex bytes
    /// @return Vector of bytes with 0xFF for 'XX' placeholders
    [[nodiscard]] static std::vector<std::uint8_t> parse_bytes_string(const std::string& bytes_str);
};

} // namespace ff8_hook::config 