#include "../include/memory/copy_memory.hpp"
#include "../../core_hook/include/task/hook_task.hpp"
#include "../../core_hook/include/context/mod_context.hpp"
#include "../../core_hook/include/util/logger.hpp"

namespace app_hook::memory {

// Use the config structure from the config namespace
using CopyMemoryConfig = config::CopyMemoryConfig;

/// @brief Execute the memory copy operation
/// @return Task result indicating success or failure
task::TaskResult CopyMemoryTask::execute() {
        LOG_DEBUG("Executing CopyMemoryTask for key '{}'", config_.key());
        LOG_DEBUG("Source address: 0x{:X}, Size: {} -> {}", config_.address(), config_.original_size(), config_.new_size());
        
        if (!config_.is_valid()) {
            LOG_ERROR("Invalid configuration for CopyMemoryTask '{}'", config_.key());
            return std::unexpected(task::TaskError::invalid_config);
        }
        
        try {
            LOG_DEBUG("Creating memory region for key '{}'", config_.key());
            
            // Create new memory region
            app_hook::context::MemoryRegion region{
                config_.new_size(), 
                config_.address(), 
                config_.description()
            };
            
            // Copy original memory content
            const auto* source = reinterpret_cast<const std::uint8_t*>(config_.address());
            if (!source) {
                LOG_ERROR("Invalid source address 0x{:X} for CopyMemoryTask '{}'", config_.address(), config_.key());
                return std::unexpected(task::TaskError::invalid_address);
            }
            
            LOG_DEBUG("Copying {} bytes from 0x{:X}", config_.original_size(), config_.address());
            
            // Perform the memory copy
            std::copy_n(source, config_.original_size(), region.data.get());
            
            // Zero-initialize the expanded portion
            if (config_.new_size() > config_.original_size()) {
                const auto expanded_size = config_.new_size() - config_.original_size();
                LOG_DEBUG("Zero-initializing {} expanded bytes", expanded_size);
                std::fill_n(region.data.get() + config_.original_size(), expanded_size, std::uint8_t{0});
            }
            
            // Log content before moving the region
            LOG_DEBUG("Copied content: {}", region.to_string(50, 100));
            
            // Store in context
            LOG_DEBUG("Storing memory region '{}' in context", config_.key());
            app_hook::context::ModContext::instance().store_memory_region(config_.key(), std::move(region));
            
            LOG_INFO("Successfully executed CopyMemoryTask for key '{}'", config_.key());
            return {};
            
        } catch (const std::bad_alloc&) {
            LOG_ERROR("Memory allocation failed for CopyMemoryTask '{}'", config_.key());
            return std::unexpected(task::TaskError::memory_allocation_failed);
        } catch (...) {
            LOG_ERROR("Copy operation failed for CopyMemoryTask '{}'", config_.key());
            return std::unexpected(task::TaskError::copy_failed);
        }
    }
    
} // namespace app_hook::memory
