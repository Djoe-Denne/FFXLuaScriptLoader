#pragma once

#include "hook_manager.hpp"
#include "../config/config_loader.hpp"
#include "../memory/copy_memory.hpp"
#include "../memory/patch_memory.hpp"
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
    
    /// @brief Create hooks from tasks configuration file and add them to the manager
    /// @param tasks_config_path Path to tasks.toml configuration file
    /// @param manager Hook manager to add hooks to
    /// @return Result of operation
    [[nodiscard]] static FactoryResult create_hooks_from_tasks(
        const std::string& tasks_config_path,
        HookManager& manager) {
        
        LOG_DEBUG("Starting hook creation from tasks config: {}", tasks_config_path);
        
        // Load memory configurations from all tasks
        auto config_result = config::ConfigLoader::load_memory_configs_from_tasks(tasks_config_path);
        if (!config_result) {
            LOG_ERROR("Failed to load configurations from tasks file: {}", tasks_config_path);
            return std::unexpected(FactoryError::config_load_failed);
        }
        
        const auto& configs = *config_result;
        LOG_INFO("Loaded {} configuration(s) from tasks file", configs.size());
        
        // Group configurations by copyAfter address
        std::unordered_map<std::uintptr_t, std::vector<config::CopyMemoryConfig>> hooks_by_address;
        
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
        
        LOG_INFO("Hook creation from tasks completed successfully");
        return {};
    }

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
        std::unordered_map<std::uintptr_t, std::vector<config::CopyMemoryConfig>> hooks_by_address;
        
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
        std::unordered_map<std::uintptr_t, std::vector<config::CopyMemoryConfig>> hooks_by_address;
        
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
        const std::vector<config::CopyMemoryConfig>& configs,
        HookManager& manager) {
        
        LOG_DEBUG("Creating tasks for hook at address 0x{:X} with {} config(s)", address, configs.size());
        
        for (const auto& config : configs) {
            LOG_DEBUG("Processing config '{}' - has patch: {}", config.key, config.has_patch());
            
            // Always create copy memory task first
            LOG_DEBUG("Creating CopyMemoryTask for config '{}'", config.key);
            auto copy_task = task::make_task<memory::CopyMemoryTask>(config);
            
            if (auto result = manager.add_task_to_hook(address, std::move(copy_task)); !result) {
                LOG_ERROR("Failed to add CopyMemoryTask '{}' to hook at address 0x{:X}", config.key, address);
                return std::unexpected(FactoryError::hook_creation_failed);
            }
            
            LOG_DEBUG("Successfully added CopyMemoryTask '{}' to hook at address 0x{:X}", config.key, address);
            
            // If config has a patch file, create patch memory task as well
            if (config.has_patch()) {
                LOG_DEBUG("Creating PatchMemoryTask for config '{}' with patch file '{}'", 
                         config.key, *config.patch);
                
                // Load patch instructions from file
                auto patch_result = config::ConfigLoader::load_patch_instructions(*config.patch);
                if (!patch_result) {
                    LOG_ERROR("Failed to load patch instructions from file '{}' for config '{}'", 
                             *config.patch, config.key);
                    return std::unexpected(FactoryError::config_load_failed);
                }
                
                const auto& patches = *patch_result;
                LOG_INFO("Loaded {} patch instruction(s) from '{}' for config '{}'", 
                        patches.size(), *config.patch, config.key);
                
                // Create patch config from copy config
                config::PatchMemoryConfig patch_config;
                patch_config.key = config.key;
                patch_config.address = config.address;
                patch_config.copy_after = config.copy_after;
                patch_config.description = config.description;
                patch_config.patch = config.patch;
                patch_config.patch_file_path = *config.patch;
                
                auto patch_task = task::make_task<memory::PatchMemoryTask>(
                    std::move(patch_config), 
                    std::move(patches)
                );
                
                if (auto result = manager.add_task_to_hook(address, std::move(patch_task)); !result) {
                    LOG_ERROR("Failed to add PatchMemoryTask '{}' to hook at address 0x{:X}", config.key, address);
                    return std::unexpected(FactoryError::hook_creation_failed);
                }
                
                LOG_DEBUG("Successfully added PatchMemoryTask '{}' to hook at address 0x{:X}", config.key, address);
            }
        }
        
        return {};
    }
};

} // namespace ff8_hook::hook
