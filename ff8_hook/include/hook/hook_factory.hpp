#pragma once

#include "../config/config_loader.hpp"
#include "../config/task_loader.hpp"
#include "../task/hook_task.hpp"
#include "../memory/copy_memory.hpp"
#include "../memory/patch_memory.hpp"
#include "hook_manager.hpp"
#include <vector>
#include <unordered_map>
#include <expected>
#include <algorithm>
#include <sstream>

namespace ff8_hook::hook {

/// @brief Error types for hook factory operations
enum class FactoryError {
    success = 0,
    config_load_failed,
    invalid_config,
    hook_creation_failed,
    task_creation_failed
};

/// @brief Result type for hook factory operations
using FactoryResult = std::expected<void, FactoryError>;

/// @brief Factory class for creating hooks from configuration
class HookFactory {
public:
    HookFactory() = default;
    ~HookFactory() = default;
    
    // Non-copyable but movable (following C++23 best practices)
    HookFactory(const HookFactory&) = delete;
    HookFactory& operator=(const HookFactory&) = delete;
    HookFactory(HookFactory&&) = default;
    HookFactory& operator=(HookFactory&&) = default;

    /// @brief Create hooks from tasks configuration and add them to the manager
    /// @param tasks_path Path to tasks configuration file  
    /// @param manager Hook manager to add hooks to
    /// @return Result of operation
    [[nodiscard]] static FactoryResult create_hooks_from_tasks(
        const std::string& tasks_path,
        HookManager& manager) {
        
        LOG_DEBUG("Starting hook creation from tasks: {}", tasks_path);
        
        // Load tasks and build execution order 
        auto tasks_result = config::TaskLoader::load_tasks(tasks_path);
        if (!tasks_result) {
            LOG_ERROR("Failed to load tasks from file: {}", tasks_path);
            return std::unexpected(FactoryError::config_load_failed);
        }
        
        // Build execution order respecting followBy dependencies
        auto execution_order = config::TaskLoader::build_execution_order(*tasks_result);
        if (!execution_order) {
            LOG_ERROR("Failed to build task execution order - circular dependencies detected");
            return std::unexpected(FactoryError::invalid_config);
        }
        
        // Create execution order string
        std::ostringstream order_stream;
        for (size_t i = 0; i < execution_order->size(); ++i) {
            if (i > 0) order_stream << " -> ";
            order_stream << (*execution_order)[i];
        }
        LOG_INFO("Task execution order: {}", order_stream.str());
        
        // Execute tasks in the correct order and create hooks following dependencies
        std::unordered_map<std::string, std::uintptr_t> task_hook_addresses; // Track where each task was hooked
        
        for (const auto& task_key : *execution_order) {
            // Find the task info
            auto task_it = std::find_if(tasks_result->begin(), tasks_result->end(),
                [&task_key](const auto& task) {
                    auto pos = task.config_file.find_last_of('/');
                    std::string file_task_key = pos != std::string::npos ? 
                        task.config_file.substr(pos + 1) : task.config_file;
                    if (file_task_key.ends_with(".toml")) {
                        file_task_key.erase(file_task_key.length() - 5);
                    }
                    return file_task_key == task_key;
                });
                
            if (task_it == tasks_result->end()) {
                LOG_WARNING("Task '{}' not found in execution order", task_key);
                continue;
            }
            
            const auto& task = *task_it;
            LOG_DEBUG("Processing task '{}' of type {}", task.name, to_string(task.type));
            
            if (auto result = process_task_with_dependencies(task, task_hook_addresses, manager); !result) {
                LOG_ERROR("Failed to process task '{}'", task.name);
                return result;
            }
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
                     config.key(), config.address(), config.copy_after(), config.original_size(), config.new_size());
            
            if (!config.is_valid()) {
                LOG_WARNING("Invalid configuration for key '{}' - skipping", config.key());
                continue;
            }
            
            // Use the copyAfter field from configuration
            hooks_by_address[config.copy_after()].push_back(config);
            LOG_DEBUG("Added config '{}' to hook address 0x{:X}", config.key(), config.copy_after());
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
                LOG_DEBUG("Added config '{}' to hook address 0x{:X} (extracted)", config.key(), *address);
            } else {
                LOG_WARNING("Failed to extract copyAfter address from config '{}'", config.key());
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
    /// @brief Process a single task with dependency handling
    /// @param task Task information
    /// @param task_hook_addresses Map tracking where each task was hooked
    /// @param manager Hook manager to add tasks to
    /// @return Result of operation
    [[nodiscard]] static FactoryResult process_task_with_dependencies(
        const config::TaskInfo& task,
        std::unordered_map<std::string, std::uintptr_t>& task_hook_addresses,
        HookManager& manager) {
        
        LOG_DEBUG("Processing task '{}' of type '{}'", task.name, to_string(task.type));
        
        if (task.type == config::ConfigType::Memory) {
            // Load memory configurations
            auto memory_configs = config::CopyMemoryLoader::load_memory_configs(task.config_file);
            if (!memory_configs) {
                LOG_ERROR("Failed to load memory configs for task '{}'", task.name);
                return std::unexpected(FactoryError::config_load_failed);
            }
            
            // Create hooks for memory tasks (these define the hook addresses)
            for (const auto& config : *memory_configs) {
                const auto hook_address = config.copy_after();
                const auto task_key = get_task_key_from_file(task.config_file);
                
                LOG_DEBUG("Creating CopyMemoryTask for '{}' at hook address 0x{:X}", config.key(), hook_address);
                
                auto copy_task = task::make_task<memory::CopyMemoryTask>(config);
                
                if (auto result = manager.add_task_to_hook(hook_address, std::move(copy_task)); !result) {
                    LOG_ERROR("Failed to add CopyMemoryTask '{}' to hook at address 0x{:X}", config.key(), hook_address);
                    return std::unexpected(FactoryError::hook_creation_failed);
                }
                
                // Remember where this task was hooked for followBy tasks
                task_hook_addresses[task_key] = hook_address;
                LOG_DEBUG("Recorded task '{}' at hook address 0x{:X}", task_key, hook_address);
            }
            
        } else if (task.type == config::ConfigType::Patch) {
            // Load patch configurations
            auto patch_configs = config::ConfigFactory::load_configs(task.type, task.config_file, task.name);
            if (!patch_configs) {
                LOG_ERROR("Failed to load patch configs for task '{}'", task.name);
                return std::unexpected(FactoryError::config_load_failed);
            }
            
            // Find hook address from dependent tasks  
            std::uintptr_t hook_address = 0;
            const auto task_key = get_task_key_from_file(task.config_file);
            
            // For patch tasks, we need to find which hook address to use
            // This should come from the tasks that this task depends on (reverse followBy)
            for (const auto& [dependent_task, address] : task_hook_addresses) {
                LOG_DEBUG("Checking if patch task '{}' should use hook address 0x{:X} from task '{}'", task_key, address, dependent_task);
                // For now, use the first available hook address
                // In a more sophisticated system, we'd check the dependency graph
                if (hook_address == 0) {
                    hook_address = address;
                    LOG_DEBUG("Using hook address 0x{:X} for patch task '{}'", hook_address, task_key);
                    break;
                }
            }
            
            if (hook_address == 0) {
                LOG_ERROR("No hook address found for patch task '{}'", task_key);
                return std::unexpected(FactoryError::invalid_config);
            }
            
            // Add patch tasks to the same hook address
            for (const auto& config : *patch_configs) {
                if (auto* patch_config = dynamic_cast<config::PatchConfig*>(config.get())) {
                    LOG_DEBUG("Creating PatchMemoryTask for '{}' at hook address 0x{:X}", patch_config->key(), hook_address);
                    
                    auto patches = patch_config->instructions();
                    auto patch_task = task::make_task<memory::PatchMemoryTask>(*patch_config, patches);
                    
                    if (auto result = manager.add_task_to_hook(hook_address, std::move(patch_task)); !result) {
                        LOG_ERROR("Failed to add PatchMemoryTask '{}' to hook at address 0x{:X}", patch_config->key(), hook_address);
                        return std::unexpected(FactoryError::hook_creation_failed);
                    }
                    
                    LOG_DEBUG("Successfully added PatchMemoryTask '{}' to hook at address 0x{:X}", patch_config->key(), hook_address);
                }
            }
        } else {
            LOG_WARNING("Unsupported task type '{}' for task '{}'", to_string(task.type), task.name);
        }
        
        return {};
    }
    
    /// @brief Extract task key from config file path
    /// @param config_file Path to config file
    /// @return Task key
    [[nodiscard]] static std::string get_task_key_from_file(const std::string& config_file) {
        auto pos = config_file.find_last_of('/');
        std::string task_key = pos != std::string::npos ? 
            config_file.substr(pos + 1) : config_file;
        if (task_key.ends_with(".toml")) {
            task_key.erase(task_key.length() - 5);
        }
        return task_key;
    }
    /// @brief Create a hook for a specific address with multiple tasks (generic version)
    /// @param address Hook address
    /// @param configs Task configurations for this address
    /// @param manager Hook manager to add hook to
    /// @return Result of operation
    [[nodiscard]] static FactoryResult create_hook_for_address_generic(
        std::uintptr_t address,
        const std::vector<config::ConfigPtr>& configs,
        HookManager& manager) {
        
        LOG_DEBUG("Creating tasks for hook at address 0x{:X} with {} config(s)", address, configs.size());
        
        for (const auto& config : configs) {
            // Create task based on config type
            if (auto* memory_config = dynamic_cast<config::CopyMemoryConfig*>(config.get())) {
                LOG_DEBUG("Creating CopyMemoryTask for config '{}'", memory_config->key());
                
                auto copy_task = task::make_task<memory::CopyMemoryTask>(*memory_config);
                
                if (auto result = manager.add_task_to_hook(address, std::move(copy_task)); !result) {
                    LOG_ERROR("Failed to add CopyMemoryTask '{}' to hook at address 0x{:X}", memory_config->key(), address);
                    return std::unexpected(FactoryError::hook_creation_failed);
                }
                
                LOG_DEBUG("Successfully added CopyMemoryTask '{}' to hook at address 0x{:X}", memory_config->key(), address);
                
            } else if (auto* patch_config = dynamic_cast<config::PatchConfig*>(config.get())) {
                LOG_DEBUG("Creating PatchMemoryTask for config '{}'", patch_config->key());
                
                                 // Load patch instructions from the patch config
                auto patches = patch_config->instructions();
                auto patch_task = task::make_task<memory::PatchMemoryTask>(*patch_config, patches);
                
                if (auto result = manager.add_task_to_hook(address, std::move(patch_task)); !result) {
                    LOG_ERROR("Failed to add PatchMemoryTask '{}' to hook at address 0x{:X}", patch_config->key(), address);
                    return std::unexpected(FactoryError::hook_creation_failed);
                }
                
                LOG_DEBUG("Successfully added PatchMemoryTask '{}' to hook at address 0x{:X}", patch_config->key(), address);
                
            } else {
                LOG_WARNING("Unknown config type for key '{}' - skipping", config->key());
            }
        }
        
        return {};
    }

    /// @brief Create a hook for a specific address with multiple tasks (legacy version)
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
            LOG_DEBUG("Creating CopyMemoryTask for config '{}'", config.key());
            
            // Create copy memory task
            auto copy_task = task::make_task<memory::CopyMemoryTask>(config);
            
            if (auto result = manager.add_task_to_hook(address, std::move(copy_task)); !result) {
                LOG_ERROR("Failed to add CopyMemoryTask '{}' to hook at address 0x{:X}", config.key(), address);
                return std::unexpected(FactoryError::hook_creation_failed);
            }
            
            LOG_DEBUG("Successfully added CopyMemoryTask '{}' to hook at address 0x{:X}", config.key(), address);
        }
        
        return {};
    }
};

} // namespace ff8_hook::hook
