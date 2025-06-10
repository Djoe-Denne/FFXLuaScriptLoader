#pragma once

#include "config_base.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace ff8_hook::config {

/// @brief Single instruction patch data
struct InstructionPatch {
    std::uintptr_t address;                ///< Address to patch
    std::vector<std::uint8_t> bytes;       ///< Instruction bytes with placeholders
    std::int32_t offset;                   ///< Offset to apply to new memory base
    
    /// @brief Check if this patch is valid
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return address != 0 && !bytes.empty();
    }
};

/// @brief Configuration for instruction patches
class PatchConfig : public ConfigBase {
public:
    /// @brief Constructor
    /// @param key Configuration key
    /// @param name Display name
    PatchConfig(std::string key, std::string name)
        : ConfigBase(ConfigType::Patch, std::move(key), std::move(name))
        , patch_file_path_{}
        , instructions_{} {}

    /// @brief Default destructor
    ~PatchConfig() override = default;

    // Accessors following C++23 conventions
    [[nodiscard]] const std::string& patch_file_path() const noexcept { return patch_file_path_; }
    [[nodiscard]] const std::vector<InstructionPatch>& instructions() const noexcept { return instructions_; }

    // Mutators
    void set_patch_file_path(std::string path) { patch_file_path_ = std::move(path); }
    void set_instructions(std::vector<InstructionPatch> instructions) { 
        instructions_ = std::move(instructions); 
    }
    void add_instruction(InstructionPatch instruction) { 
        instructions_.push_back(std::move(instruction)); 
    }

    /// @brief Check if this configuration is valid
    /// @return True if all required fields are properly set
    [[nodiscard]] bool is_valid() const noexcept override {
        return ConfigBase::is_valid() && 
               (!patch_file_path_.empty() || !instructions_.empty());
    }

    /// @brief Get debug string representation
    /// @return Debug string with patch-specific information
    [[nodiscard]] std::string debug_string() const override {
        return ConfigBase::debug_string() + 
               " patch_file=" + patch_file_path_ +
               " instructions=" + std::to_string(instructions_.size());
    }

private:
    std::string patch_file_path_;              ///< Path to the patch TOML file
    std::vector<InstructionPatch> instructions_;  ///< Loaded instruction patches
};

} // namespace ff8_hook::config 