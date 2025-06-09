#pragma once

#include "../task/hook_task.hpp"
#include "../context/mod_context.hpp"
#include "../util/logger.hpp"
#include <string>
#include <cstdint>

namespace ff8_hook::memory {

/// @brief Configuration for memory copy operations
struct CopyMemoryConfig {
    std::string key;                    ///< Memory region key (e.g., "memory.K_MAGIC")
    std::uintptr_t address;            ///< Source address to copy from
    std::size_t original_size;         ///< Original size of the memory region
    std::size_t new_size;              ///< New size for the expanded memory region
    std::uintptr_t copy_after;         ///< Address where hook should be installed
    std::string description;           ///< Description of the memory region
    
    /// @brief Validate the configuration
    /// @return true if configuration is valid
    [[nodiscard]] bool is_valid() const noexcept {
        return !key.empty() && 
               address != 0 && 
               original_size > 0 && 
               new_size >= original_size &&
               copy_after != 0;
    }
};

/// @brief Task that copies memory from one location to an expanded buffer
class CopyMemoryTask final : public task::IHookTask {
public:
    /// @brief Construct a memory copy task
    /// @param config Configuration for the copy operation
    explicit CopyMemoryTask(CopyMemoryConfig config) noexcept
        : config_(std::move(config)) {}
    
    /// @brief Execute the memory copy operation
    /// @return Task result indicating success or failure
    [[nodiscard]] task::TaskResult execute() override {
        LOG_DEBUG("Executing CopyMemoryTask for key '{}'", config_.key);
        LOG_DEBUG("Source address: 0x{:X}, Size: {} -> {}", config_.address, config_.original_size, config_.new_size);
        
        if (!config_.is_valid()) {
            LOG_ERROR("Invalid configuration for CopyMemoryTask '{}'", config_.key);
            return std::unexpected(task::TaskError::invalid_config);
        }
        
        try {
            LOG_DEBUG("Creating memory region for key '{}'", config_.key);
            
            // Create new memory region
            context::MemoryRegion region{
                config_.new_size, 
                config_.address, 
                config_.description
            };
            
            // Copy original memory content
            const auto* source = reinterpret_cast<const std::uint8_t*>(config_.address);
            if (!source) {
                LOG_ERROR("Invalid source address 0x{:X} for CopyMemoryTask '{}'", config_.address, config_.key);
                return std::unexpected(task::TaskError::invalid_address);
            }
            
            LOG_DEBUG("Copying {} bytes from 0x{:X}", config_.original_size, config_.address);
            
            // Perform the memory copy
            std::copy_n(source, config_.original_size, region.data.get());
            
            // Zero-initialize the expanded portion
            if (config_.new_size > config_.original_size) {
                const auto expanded_size = config_.new_size - config_.original_size;
                LOG_DEBUG("Zero-initializing {} expanded bytes", expanded_size);
                std::fill_n(region.data.get() + config_.original_size, expanded_size, std::uint8_t{0});
            }
            
            // Store in context
            LOG_DEBUG("Storing memory region '{}' in context", config_.key);
            context::ModContext::instance().store_memory_region(config_.key, std::move(region));
            
            LOG_INFO("Successfully executed CopyMemoryTask for key '{}'", config_.key);
            return {};
            
        } catch (const std::bad_alloc&) {
            LOG_ERROR("Memory allocation failed for CopyMemoryTask '{}'", config_.key);
            return std::unexpected(task::TaskError::memory_allocation_failed);
        } catch (...) {
            LOG_ERROR("Copy operation failed for CopyMemoryTask '{}'", config_.key);
            return std::unexpected(task::TaskError::copy_failed);
        }
    }
    
    /// @brief Get the task name
    /// @return Task name
    [[nodiscard]] std::string name() const override {
        return "CopyMemory";
    }
    
    /// @brief Get the task description
    /// @return Task description
    [[nodiscard]] std::string description() const override {
        return "Copy memory region '" + config_.key + "' from " + 
               std::to_string(config_.original_size) + " to " + 
               std::to_string(config_.new_size) + " bytes";
    }
    
    /// @brief Get the configuration
    /// @return Copy memory configuration
    [[nodiscard]] const CopyMemoryConfig& config() const noexcept {
        return config_;
    }
    
private:
    CopyMemoryConfig config_;
};

} // namespace ff8_hook::memory
