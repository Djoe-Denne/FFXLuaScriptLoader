#pragma once

#include "../task/hook_task.hpp"
#include "../context/mod_context.hpp"
#include "../util/logger.hpp"
#include "../config/memory_config.hpp"
#include "../config/config_loader.hpp"
#include <string>
#include <cstdint>
#include <vector>
#include <windows.h>

namespace ff8_hook::memory {

/// @brief Use instruction patch from config namespace
using InstructionPatch = config::InstructionPatch;

/// @brief Patch memory configuration
using PatchMemoryConfig = config::PatchMemoryConfig;

/// @brief Task that applies memory patches to instructions
class PatchMemoryTask final : public task::IHookTask {
public:
    /// @brief Construct a memory patch task
    /// @param config Configuration for the patch operation
    /// @param patches Pre-parsed instruction patches
    PatchMemoryTask(PatchMemoryConfig config, std::vector<InstructionPatch> patches) noexcept
        : config_(std::move(config)), patches_(std::move(patches)) {}
    
    /// @brief Execute the memory patch operation
    /// @return Task result indicating success or failure
    [[nodiscard]] task::TaskResult execute() override {
        LOG_DEBUG("Executing PatchMemoryTask for key '{}'", config_.key);
        LOG_INFO("Applying {} patch instruction(s) for task '{}'", patches_.size(), config_.key);
        
        if (!config_.is_valid()) {
            LOG_ERROR("Invalid configuration for PatchMemoryTask '{}'", config_.key);
            return task::TaskResult{}; // Return empty result for error
        }
        
        if (patches_.empty()) {
            LOG_WARNING("No patches to apply for task '{}'", config_.key);
            return task::TaskResult{};
        }
        
        try {
            // Get the new memory base address from context
            auto memory_region = context::ModContext::instance().get_memory_region(config_.key);
            if (!memory_region) {
                LOG_ERROR("Memory region '{}' not found in context for PatchMemoryTask", config_.key);
                return task::TaskResult{}; // Return empty result for error
            }
            
            const auto new_base = reinterpret_cast<std::uintptr_t>(memory_region->data.get());
            LOG_DEBUG("Using new memory base address: 0x{:X}", new_base);
            
            // Apply all patches
            std::size_t successful_patches = 0;
            for (const auto& patch : patches_) {
                if (apply_instruction_patch(patch, new_base)) {
                    successful_patches++;
                } else {
                    LOG_WARNING("Failed to apply patch at address 0x{:X}", patch.address);
                }
            }
            
            LOG_INFO("Successfully applied {}/{} patches for task '{}'", 
                    successful_patches, patches_.size(), config_.key);
            
            if (successful_patches == 0) {
                LOG_ERROR("No patches were successfully applied for task '{}'", config_.key);
                return task::TaskResult{}; // Return empty result for error
            }
            
            return task::TaskResult{};
            
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in PatchMemoryTask '{}': {}", config_.key, e.what());
            return task::TaskResult{}; // Return empty result for error
        } catch (...) {
            LOG_ERROR("Unknown exception in PatchMemoryTask '{}'", config_.key);
            return task::TaskResult{}; // Return empty result for error
        }
    }
    
    /// @brief Get the task name
    /// @return Task name
    [[nodiscard]] std::string name() const override {
        return "PatchMemory";
    }
    
    /// @brief Get the task description
    /// @return Task description
    [[nodiscard]] std::string description() const override {
        return "Apply " + std::to_string(patches_.size()) + " memory patches for '" + config_.key + "'";
    }
    
    /// @brief Get the configuration
    /// @return Patch memory configuration
    [[nodiscard]] const PatchMemoryConfig& config() const noexcept {
        return config_;
    }
    
    /// @brief Get the patches
    /// @return Vector of instruction patches
    [[nodiscard]] const std::vector<InstructionPatch>& patches() const noexcept {
        return patches_;
    }
    
private:
    PatchMemoryConfig config_;
    std::vector<InstructionPatch> patches_;
    
    /// @brief Apply a single instruction patch
    /// @param patch Patch to apply
    /// @param new_base New memory base address
    /// @return true if patch was successful
    [[nodiscard]] bool apply_instruction_patch(const InstructionPatch& patch, std::uintptr_t new_base) {
        LOG_DEBUG("Applying patch at address 0x{:X} with offset {}", patch.address, patch.offset);
        
        // Calculate the new address
        const auto new_address = new_base + patch.offset;
        LOG_INFO("New address: 0x{:X} + {} = 0x{:X}", new_base, patch.offset, new_address);
        
        // Create patched bytes by replacing 'XX XX XX XX' with new address
        auto patched_bytes = patch.bytes;
        if (!replace_placeholders(patched_bytes, new_address)) {
            LOG_ERROR("Failed to replace placeholders in patch at 0x{:X}", patch.address);
            return false;
        }
        
        // Apply the binary patch
        auto* target = reinterpret_cast<std::uint8_t*>(patch.address);
        if (!target) {
            LOG_ERROR("Invalid target address 0x{:X} for patch", patch.address);
            return false;
        }
        
        // Make memory writable
        DWORD old_protect;
        if (!VirtualProtect(target, patched_bytes.size(), PAGE_EXECUTE_READWRITE, &old_protect)) {
            LOG_ERROR("Failed to make memory writable at 0x{:X}", patch.address);
            return false;
        }
        
        // Copy the patched bytes
        std::copy(patched_bytes.begin(), patched_bytes.end(), target);
        
        // Restore original protection
        VirtualProtect(target, patched_bytes.size(), old_protect, &old_protect);
        
        LOG_INFO("Successfully applied patch at 0x{:X}", patch.address);
        return true;
    }
    
    /// @brief Replace 'XX XX XX XX' placeholders with actual address
    /// @param bytes Byte array to modify
    /// @param address Address to insert
    /// @return true if placeholders were found and replaced
    [[nodiscard]] bool replace_placeholders(std::vector<std::uint8_t>& bytes, std::uintptr_t address) {
        // Look for 4 consecutive 0xFF bytes (our placeholder)
        for (std::size_t i = 0; i <= bytes.size() - 4; ++i) {
            if (bytes[i] == 0xFF && bytes[i+1] == 0xFF && bytes[i+2] == 0xFF && bytes[i+3] == 0xFF) {
                // Replace with address in little-endian format
                const auto addr_bytes = reinterpret_cast<const std::uint8_t*>(&address);
                bytes[i] = addr_bytes[0];
                bytes[i+1] = addr_bytes[1];
                bytes[i+2] = addr_bytes[2];
                bytes[i+3] = addr_bytes[3];
                return true;
            }
        }
        return false;
    }
};

} // namespace ff8_hook::memory 