#pragma once

#include "config/patch_config.hpp"
#include "../../app_hook/include/task/hook_task.hpp"
#include "../../app_hook/include/context/mod_context.hpp"
#include "../../app_hook/include/util/logger.hpp"
#include "../config/patch_config.hpp"
#include "../../app_hook/include/config/config_loader.hpp"
#include <string>
#include <cstdint>
#include <vector>
#include <windows.h>

namespace app_hook::memory {

/// @brief Use instruction patch from config namespace
using InstructionPatch = config::InstructionPatch;

/// @brief Use patch config from config namespace
using PatchConfig = config::PatchConfig;

/// @brief Task that applies memory patches to instructions
class PatchMemoryTask final : public task::IHookTask {
public:
    /// @brief Construct a memory patch task
    /// @param config Configuration for the patch operation
    /// @param patches Pre-parsed instruction patches
    PatchMemoryTask(PatchConfig config, std::vector<InstructionPatch> patches) noexcept
        : config_(std::move(config)), patches_(std::move(patches)) {}
    
    /// @brief Execute the memory patch operation
    /// @return Task result indicating success or failure
    [[nodiscard]] task::TaskResult execute() override;
    
    /// @brief Get the task name
    /// @return Task name
    [[nodiscard]] std::string name() const override;
    
    /// @brief Get the task description
    /// @return Task description
    [[nodiscard]] std::string description() const override;
    
    /// @brief Get the configuration
    /// @return Patch configuration
    [[nodiscard]] const PatchConfig& config() const noexcept {
        return config_;
    }
    
    /// @brief Get the patches
    /// @return Vector of instruction patches
    [[nodiscard]] const std::vector<InstructionPatch>& patches() const noexcept {
        return patches_;
    }
    
private:
    PatchConfig config_;
    std::vector<InstructionPatch> patches_;
    
    /// @brief Apply a single instruction patch
    /// @param patch Patch to apply
    /// @param new_base New memory base address
    /// @return true if patch was successful
    [[nodiscard]] bool apply_instruction_patch(const InstructionPatch& patch, std::uintptr_t new_base);
    
    /// @brief Replace 'XX XX XX XX' placeholders with actual address
    /// @param bytes Byte array to modify
    /// @param address Address to insert
    /// @return true if placeholders were found and replaced
    [[nodiscard]] bool replace_placeholders(std::vector<std::uint8_t>& bytes, std::uintptr_t address);
};

} // namespace app_hook::memory 
