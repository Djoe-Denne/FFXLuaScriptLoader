#include "../include/memory/copy_memory.hpp"
#include "../include/memory/memory_region.hpp"
#include "../../core_hook/include/task/hook_task.hpp"
#include "../../core_hook/include/context/mod_context.hpp"
#include "plugin/plugin_interface.hpp"

namespace app_hook::memory {

// Use the config structure from the config namespace
using CopyMemoryConfig = config::CopyMemoryConfig;

/// @brief Execute the memory copy operation
/// @return Task result indicating success or failure
task::TaskResult CopyMemoryTask::execute() {
        PLUGIN_LOG_DEBUG("Executing CopyMemoryTask for key '{}'", config_.key());
        PLUGIN_LOG_DEBUG("Source address: 0x{:X}, Size: {} -> {}", config_.address(), config_.original_size(), config_.new_size());
        
        if (!config_.is_valid()) {
            PLUGIN_LOG_ERROR("Invalid configuration for CopyMemoryTask '{}'", config_.key());
            return std::unexpected(task::TaskError::invalid_config);
        }
        
        try {
            // Create new memory region
            MemoryRegion region{
                config_.new_size(), 
                config_.address(), 
                config_.description()
            };
            
            // Copy original memory content
            const auto* source = reinterpret_cast<const std::uint8_t*>(config_.address());
            if (!source) {
                PLUGIN_LOG_ERROR("Invalid source address 0x{:X} for CopyMemoryTask '{}'", config_.address(), config_.key());
                return std::unexpected(task::TaskError::invalid_address);
            }
            
            PLUGIN_LOG_DEBUG("Copying {} bytes from 0x{:X}", config_.original_size(), config_.address());
            
            // Perform the memory copy
            std::copy_n(source, config_.original_size(), region.data.get());
            
            // Zero-initialize the expanded portion
            if (config_.new_size() > config_.original_size()) {
                const auto expanded_size = config_.new_size() - config_.original_size();
                PLUGIN_LOG_DEBUG("Zero-initializing {} expanded bytes", expanded_size);
                std::fill_n(region.data.get() + config_.original_size(), expanded_size, std::uint8_t{0});
            }
            
            // Log content before storing the region
            PLUGIN_LOG_DEBUG("Copied content: {}", region.to_string(0, 100));
            
            // Store in context using the configured key
            if (!config_.writes_to_context()) {
                PLUGIN_LOG_ERROR("CopyMemoryTask '{}' is not configured to write to context", config_.key());
                return std::unexpected(task::TaskError::invalid_config);
            }
            
            const std::string context_key = config_.write_in_context().name;
            if (context_key.empty()) {
                PLUGIN_LOG_ERROR("CopyMemoryTask '{}' has empty context key name", config_.key());
                return std::unexpected(task::TaskError::invalid_config);
            }
            
            PLUGIN_LOG_DEBUG("Using configured context key: '{}'", context_key);
            
            // Debug: Log the write context configuration details
            PLUGIN_LOG_DEBUG("WriteInContext config - enabled: {}, name: '{}'", 
                           config_.write_in_context().enabled, config_.write_in_context().name);
            
            PLUGIN_LOG_DEBUG("Storing memory region '{}' in context", context_key);
            if (host_) {
                host_->get_mod_context().store_data(context_key, std::move(region));
            } else {
                // Fallback to singleton for backward compatibility
                PLUGIN_LOG_WARN("Using singleton ModContext for backward compatibility");
                app_hook::context::ModContext::instance().store_data(context_key, std::move(region));
            }
            
            PLUGIN_LOG_INFO("Successfully executed CopyMemoryTask for key '{}'", config_.key());
            return {};
            
        } catch (const std::bad_alloc&) {
            PLUGIN_LOG_ERROR("Memory allocation failed for CopyMemoryTask '{}'", config_.key());
            return std::unexpected(task::TaskError::memory_allocation_failed);
        } catch (...) {
            PLUGIN_LOG_ERROR("Copy operation failed for CopyMemoryTask '{}'", config_.key());
            return std::unexpected(task::TaskError::copy_failed);
        }
    }
    
} // namespace app_hook::memory
