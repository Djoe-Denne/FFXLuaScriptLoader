#include "../include/memory/patch_memory.hpp"
#include "../include/memory/memory_region.hpp"
#include "../../core_hook/include/task/hook_task.hpp"
#include "../../core_hook/include/context/mod_context.hpp"
#include "plugin/plugin_interface.hpp"
#include <windows.h>

namespace app_hook::memory {

task::TaskResult PatchMemoryTask::execute() {
    PLUGIN_LOG_DEBUG("Executing PatchMemoryTask for key '{}'", config_.key());
    PLUGIN_LOG_INFO("Applying {} patch instruction(s) for task '{}'", patches_.size(), config_.key());
    
    if (!config_.is_valid()) {
        PLUGIN_LOG_ERROR("Invalid configuration for PatchMemoryTask '{}'", config_.key());
        return std::unexpected(task::TaskError::invalid_config);
    }
    
    if (patches_.empty()) {
        PLUGIN_LOG_WARN("No patches to apply for task '{}'", config_.key());
        return {};
    }
    
    try {
        // Determine which key to use for reading from context
        std::string context_key;
        if (config_.reads_from_context()) {
            context_key = config_.read_from_context();
            PLUGIN_LOG_DEBUG("Using configured read context key: '{}'", context_key);
        } else {
            // Fall back to legacy key (same as task key) for backward compatibility
            context_key = config_.key();
            PLUGIN_LOG_DEBUG("Using legacy read context key: '{}'", context_key);
        }
        
        // Get the new memory base address from context  
        auto memory_region = host_ ? 
            host_->get_mod_context().get_data<MemoryRegion>(context_key) :
            app_hook::context::ModContext::instance().get_data<MemoryRegion>(context_key);
        if (!memory_region) {
            PLUGIN_LOG_ERROR("Memory region '{}' not found in context for PatchMemoryTask", context_key);
            return std::unexpected(task::TaskError::invalid_address);
        }
        
        const auto new_base = reinterpret_cast<std::uintptr_t>(memory_region->data.get());
        PLUGIN_LOG_DEBUG("Using new memory base address: 0x{:X}", new_base);
        
        // Apply all patches
        std::size_t successful_patches = 0;
        for (const auto& patch : patches_) {
            if (apply_instruction_patch(patch, new_base)) {
                successful_patches++;
            } else {
                PLUGIN_LOG_WARN("Failed to apply patch at address 0x{:X}", patch.address);
            }
        }
        
        PLUGIN_LOG_INFO("Successfully applied {}/{} patches for task '{}'", 
                successful_patches, patches_.size(), config_.key());
        
        if (successful_patches == 0) {
            PLUGIN_LOG_ERROR("No patches were successfully applied for task '{}'", config_.key());
            return std::unexpected(task::TaskError::patch_failed);
        }
        
        return {};
        
    } catch (const std::exception& e) {
        PLUGIN_LOG_ERROR("Exception in PatchMemoryTask '{}': {}", config_.key(), e.what());
        return std::unexpected(task::TaskError::patch_failed);
    } catch (...) {
        PLUGIN_LOG_ERROR("Unknown exception in PatchMemoryTask '{}'", config_.key());
        return std::unexpected(task::TaskError::patch_failed);
    }
}

std::string PatchMemoryTask::name() const {
    return "PatchMemory";
}

std::string PatchMemoryTask::description() const {
    return "Apply " + std::to_string(patches_.size()) + " memory patches for '" + config_.key() + "'";
}

bool PatchMemoryTask::apply_instruction_patch(const InstructionPatch& patch, std::uintptr_t new_base) {
    PLUGIN_LOG_DEBUG("Applying patch at address 0x{:X} with offset {}", patch.address, patch.offset);
    
    // Calculate the new address
    const auto new_address = new_base + patch.offset;
    PLUGIN_LOG_INFO("New address: 0x{:X} + {} = 0x{:X}", new_base, patch.offset, new_address);
    
    // Create patched bytes by replacing 'XX XX XX XX' with new address
    auto patched_bytes = patch.bytes;
    if (!replace_placeholders(patched_bytes, new_address)) {
        PLUGIN_LOG_ERROR("Failed to replace placeholders in patch at 0x{:X}", patch.address);
        return false;
    }
    
    // Apply the binary patch
    auto* target = reinterpret_cast<std::uint8_t*>(patch.address);
    if (!target) {
        PLUGIN_LOG_ERROR("Invalid target address 0x{:X} for patch", patch.address);
        return false;
    }
    
    // Make memory writable
    DWORD old_protect;
    if (!VirtualProtect(target, patched_bytes.size(), PAGE_EXECUTE_READWRITE, &old_protect)) {
        PLUGIN_LOG_ERROR("Failed to make memory writable at 0x{:X}", patch.address);
        return false;
    }
    
    // Copy the patched bytes
    std::copy(patched_bytes.begin(), patched_bytes.end(), target);
    
    // Restore original protection
    VirtualProtect(target, patched_bytes.size(), old_protect, &old_protect);
    
    PLUGIN_LOG_INFO("Successfully applied patch at 0x{:X}", patch.address);
    return true;
}

bool PatchMemoryTask::replace_placeholders(std::vector<std::uint8_t>& bytes, std::uintptr_t address) {
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

} // namespace app_hook::memory 
