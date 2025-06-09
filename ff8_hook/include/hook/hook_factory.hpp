#pragma once

#include "hook_manager.hpp"
#include "../config/config_loader.hpp"
#include "../memory/copy_memory.hpp"
#include "../util/logger.hpp"
#include <string>
#include <expected>

namespace ff8_hook::hook {

/// @brief Error codes for factory operations
enum class FactoryError {
    success = 0,
    config_load_failed,
    invalid_config,
    hook_creation_failed,
    task_creation_failed
};

/// @brief Factory operation result type
using FactoryResult = std::expected<void, FactoryError>;

/// @brief Factory for creating hooks from configuration
class HookFactory {
public:
    HookFactory() = default;
    ~HookFactory() = default;
    
    // Non-copyable but movable
    HookFactory(const HookFactory&) = delete;
    HookFactory& operator=(const HookFactory&) = delete;
    HookFactory(HookFactory&&) = default;
    HookFactory& operator=(HookFactory&&) = default;
    
    /// @brief Create hooks from configuration file and add them to the manager
    /// @param config_path Path to configuration file
    /// @param manager Hook manager to add hooks to
    /// @return Result of operation
    [[nodiscard]] static FactoryResult create_hooks_from_config(
        const std::string& config_path,
        HookManager& manager) {
        
        LOG_DEBUG("Starting hook creation from config: {}", config_path);
        
        // Load memory configurations
        auto config_result = config::ConfigLoader::load_memory_configs(config_path);
        if (!config_result) {
            LOG_ERROR("Failed to load configuration from file: {}", config_path);
            return std::unexpected(FactoryError::config_load_failed);
        }
        
        const auto& configs = *config_result;
        LOG_INFO("Loaded {} configuration(s) from file", configs.size());
        
        // Group configurations by copyAfter address
        std::unordered_map<std::uintptr_t, std::vector<memory::CopyMemoryConfig>> hooks_by_address;
        
        for (const auto& config : configs) {
            LOG_DEBUG("Processing config: key='{}', address=0x{:X}, copyAfter=0x{:X}, originalSize={}, newSize={}", 
                     config.key, config.address, config.copy_after, config.original_size, config.new_size);
            
            if (!config.is_valid()) {
                LOG_WARNING("Invalid configuration for key '{}' - skipping", config.key);
                continue;
            }
            
            // Use the copyAfter field from configuration
            hooks_by_address[config.copy_after].push_back(config);
            LOG_DEBUG("Added config '{}' to hook address 0x{:X}", config.key, config.copy_after);
        }
        
        LOG_INFO("Grouped configurations into {} unique hook address(es)", hooks_by_address.size());
        
        // Create hooks for each address
        for (const auto& [address, task_configs] : hooks_by_address) {
            LOG_DEBUG("Creating hook for address 0x{:X} with {} task(s)", address, task_configs.size());
            
            if (auto result = create_hook_for_address(address, task_configs, manager); !result) {
                LOG_ERROR("Failed to create hook for address 0x{:X}", address);
                return result;
            }
            
            LOG_INFO("Successfully created hook for address 0x{:X} with {} task(s)", address, task_configs.size());
        }
        
        LOG_INFO("Hook creation completed successfully");
        return {};
    }
    
    /// @brief Create hooks from configuration file with custom copyAfter parsing
    /// @param config_path Path to configuration file
    /// @param manager Hook manager to add hooks to
    /// @param extract_copy_after Function to extract copyAfter address from config
    /// @return Result of operation
    template<typename ExtractorFunc>
    [[nodiscard]] static FactoryResult create_hooks_from_config_with_extractor(
        const std::string& config_path,
        HookManager& manager,
        ExtractorFunc&& extract_copy_after) {
        
        LOG_DEBUG("Starting hook creation with custom extractor from config: {}", config_path);
        
        // Load memory configurations
        auto config_result = config::ConfigLoader::load_memory_configs(config_path);
        if (!config_result) {
            LOG_ERROR("Failed to load configuration from file: {}", config_path);
            return std::unexpected(FactoryError::config_load_failed);
        }
        
        const auto& configs = *config_result;
        LOG_INFO("Loaded {} configuration(s) from file", configs.size());
        
        // Group configurations by copyAfter address
        std::unordered_map<std::uintptr_t, std::vector<memory::CopyMemoryConfig>> hooks_by_address;
        
        for (const auto& config : configs) {
            if (auto address = extract_copy_after(config)) {
                hooks_by_address[*address].push_back(config);
                LOG_DEBUG("Added config '{}' to hook address 0x{:X} (extracted)", config.key, *address);
            } else {
                LOG_WARNING("Failed to extract copyAfter address from config '{}'", config.key);
            }
        }
        
        LOG_INFO("Grouped configurations into {} unique hook address(es)", hooks_by_address.size());
        
        // Create hooks for each address
        for (const auto& [address, task_configs] : hooks_by_address) {
            LOG_DEBUG("Creating hook for address 0x{:X} with {} task(s)", address, task_configs.size());
            
            if (auto result = create_hook_for_address(address, task_configs, manager); !result) {
                LOG_ERROR("Failed to create hook for address 0x{:X}", address);
                return result;
            }
            
            LOG_INFO("Successfully created hook for address 0x{:X} with {} task(s)", address, task_configs.size());
        }
        
        LOG_INFO("Hook creation completed successfully");
        return {};
    }
    
private:
    /// @brief Create a hook for a specific address with multiple tasks
    /// @param address Hook address
    /// @param configs Task configurations for this address
    /// @param manager Hook manager to add hook to
    /// @return Result of operation
    [[nodiscard]] static FactoryResult create_hook_for_address(
        std::uintptr_t address,
        const std::vector<memory::CopyMemoryConfig>& configs,
        HookManager& manager) {
        
        LOG_DEBUG("Creating {} task(s) for hook at address 0x{:X}", configs.size(), address);
        
        for (const auto& config : configs) {
            LOG_DEBUG("Creating CopyMemoryTask for config '{}'", config.key);
            
            // Create copy memory task
            auto task = task::make_task<memory::CopyMemoryTask>(config);
            
            // Add task to hook at the specified address
            if (auto result = manager.add_task_to_hook(address, std::move(task)); !result) {
                LOG_ERROR("Failed to add task '{}' to hook at address 0x{:X}", config.key, address);
                return std::unexpected(FactoryError::hook_creation_failed);
            }
            
            LOG_DEBUG("Successfully added task '{}' to hook at address 0x{:X}", config.key, address);
        }
        
        return {};
    }
};

} // namespace ff8_hook::hook
