#pragma once

#include <string>
#include <cstdint>
#include <optional>

namespace ff8_hook::config {

/// @brief Base configuration for memory operations
struct BaseMemoryConfig {
    std::string key;                    ///< Memory region key (e.g., "memory.K_MAGIC")
    std::uintptr_t address;            ///< Source address
    std::uintptr_t copy_after;         ///< Address where hook should be installed
    std::string description;           ///< Description of the memory region
    std::optional<std::string> patch;  ///< Optional patch file path
    
    /// @brief Validate the base configuration
    /// @return true if base configuration is valid
    [[nodiscard]] bool is_base_valid() const noexcept {
        return !key.empty() && 
               address != 0 && 
               copy_after != 0;
    }
    
    /// @brief Check if this config has a patch file
    /// @return true if patch file is specified
    [[nodiscard]] bool has_patch() const noexcept {
        return patch.has_value() && !patch->empty();
    }
};

/// @brief Configuration for memory copy operations
struct CopyMemoryConfig : BaseMemoryConfig {
    std::size_t original_size;         ///< Original size of the memory region
    std::size_t new_size;              ///< New size for the expanded memory region
    
    /// @brief Validate the configuration
    /// @return true if configuration is valid
    [[nodiscard]] bool is_valid() const noexcept {
        return is_base_valid() && 
               original_size > 0 && 
               new_size >= original_size;
    }
};

/// @brief Configuration for patch memory operations
struct PatchMemoryConfig : BaseMemoryConfig {
    std::string patch_file_path;       ///< Path to the patch TOML file
    
    /// @brief Validate the configuration
    /// @return true if configuration is valid
    [[nodiscard]] bool is_valid() const noexcept {
        return is_base_valid() && 
               !patch_file_path.empty();
    }
};

} // namespace ff8_hook::config
