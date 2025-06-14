#include "../../include/hook/hook_factory.hpp"
#include "../../include/util/logger.hpp"
#include <algorithm>
#include <ranges>

namespace app_hook::hook {

FactoryResult HookFactory::create_hooks_from_tasks(
    const std::string& tasks_path,
    HookManager& manager) {
    
    LOG_INFO("Creating hooks from tasks configuration: {}", tasks_path);
    
    // Load task information
    auto tasks_result = config::TaskLoader::load_tasks(tasks_path);
    if (!tasks_result) {
        LOG_ERROR("Failed to load tasks from: {}", tasks_path);
        return std::unexpected(FactoryError::config_load_failed);
    }
    
    // Build execution order
    auto order_result = config::TaskLoader::build_execution_order(*tasks_result);
    if (!order_result) {
        LOG_ERROR("Failed to build task execution order");
        return std::unexpected(FactoryError::invalid_config);
    }
    
    LOG_INFO("Processing {} task(s) in dependency order", order_result->size());
    
    // Track where each task was hooked for dependency resolution
    std::unordered_map<std::string, std::uintptr_t> task_hook_addresses;
    
    // Process tasks in dependency order
    for (const auto& task_key : *order_result) {
        // Find the task info for this key
        auto task_it = std::ranges::find_if(*tasks_result, [&task_key](const auto& task) {
            return get_task_key_from_file(task.config_file) == task_key;
        });
        
        if (task_it == tasks_result->end()) {
            LOG_ERROR("Task key '{}' not found in loaded tasks", task_key);
            return std::unexpected(FactoryError::invalid_config);
        }
        
        LOG_INFO("Processing task '{}' ({})", task_it->name, task_key);
        
        if (auto result = process_task_with_dependencies(*task_it, task_hook_addresses, manager); !result) {
            LOG_ERROR("Failed to process task '{}' ({})", task_it->name, task_key);
            return result;
        }
        
        LOG_INFO("Successfully processed task '{}' ({})", task_it->name, task_key);
    }
    
    LOG_INFO("Successfully created hooks from tasks configuration");
    return {};
}

FactoryResult HookFactory::create_hooks_from_configs(
    const std::vector<config::ConfigPtr>& configs,
    HookManager& manager) {
    
    LOG_INFO("Creating hooks from {} configuration(s)", configs.size());
    
    // Group configurations by hook address
    std::unordered_map<std::uintptr_t, std::vector<config::ConfigPtr>> hooks_by_address;
    
    for (const auto& config : configs) {
        if (!config || !config->is_valid()) {
            LOG_WARNING("Invalid configuration - skipping");
            continue;
        }
        
        std::uintptr_t hook_address = extract_hook_address(*config);
        if (hook_address == 0) {
            LOG_WARNING("No hook address found for config '{}' - skipping", config->key());
            continue;
        }
        
        LOG_DEBUG("Added config '{}' to hook address 0x{:X}", config->key(), hook_address);
        hooks_by_address[hook_address].push_back(config);
    }
    
    LOG_INFO("Grouped configurations into {} unique hook address(es)", hooks_by_address.size());
    
    // Create tasks for each address
    for (const auto& [address, task_configs] : hooks_by_address) {
        LOG_DEBUG("Creating tasks for address 0x{:X} with {} config(s)", address, task_configs.size());
        
        if (auto result = create_tasks_for_address(address, task_configs, manager); !result) {
            LOG_ERROR("Failed to create tasks for address 0x{:X}", address);
            return result;
        }
        
        LOG_INFO("Successfully created tasks for address 0x{:X} with {} config(s)", address, task_configs.size());
    }
    
    LOG_INFO("Hook creation completed successfully");
    return {};
}

FactoryResult HookFactory::process_task_with_dependencies(
    const config::TaskInfo& task,
    std::unordered_map<std::string, std::uintptr_t>& task_hook_addresses,
    HookManager& manager) {
    
    LOG_DEBUG("Processing task '{}' of type '{}'", task.name, to_string(task.type));
    
    // Load configurations for this task using the generic factory
    auto configs_result = config::ConfigFactory::load_configs(task.type, task.config_file, task.name);
    if (!configs_result) {
        LOG_ERROR("Failed to load configs for task '{}'", task.name);
        return std::unexpected(FactoryError::config_load_failed);
    }
    
    const auto task_key = get_task_key_from_file(task.config_file);
    
    // For memory tasks, they define their own hook addresses
    if (task.type == config::ConfigType::Memory) {
        for (const auto& config : *configs_result) {
            std::uintptr_t hook_address = extract_hook_address(*config);
            if (hook_address == 0) {
                LOG_WARNING("No hook address found for config '{}' - skipping", config->key());
                continue;
            }
            
            // Create task using the TaskFactory
            auto task_ptr = task::TaskFactory::instance().create_task(*config);
            if (!task_ptr) {
                LOG_ERROR("Failed to create task for config '{}'", config->key());
                return std::unexpected(FactoryError::task_creation_failed);
            }
            
            if (auto result = manager.add_task_to_hook(hook_address, std::move(task_ptr)); !result) {
                LOG_ERROR("Failed to add task '{}' to hook at address 0x{:X}", config->key(), hook_address);
                return std::unexpected(FactoryError::hook_creation_failed);
            }
            
            // Remember where this task was hooked for followBy tasks
            task_hook_addresses[task_key] = hook_address;
            LOG_DEBUG("Recorded task '{}' at hook address 0x{:X}", task_key, hook_address);
        }
    } 
    // For other task types (like patches), find hook address from dependent tasks
    else {
        std::uintptr_t hook_address = 0;
        
        // Find hook address from dependent tasks
        for (const auto& [dependent_task, address] : task_hook_addresses) {
            if (hook_address == 0) {
                hook_address = address;
                LOG_DEBUG("Using hook address 0x{:X} for task '{}'", hook_address, task_key);
                break;
            }
        }
        
        if (hook_address == 0) {
            LOG_ERROR("No hook address found for task '{}'", task_key);
            return std::unexpected(FactoryError::invalid_config);
        }
        
        // Create tasks for this address
        for (const auto& config : *configs_result) {
            auto task_ptr = task::TaskFactory::instance().create_task(*config);
            if (!task_ptr) {
                LOG_ERROR("Failed to create task for config '{}'", config->key());
                return std::unexpected(FactoryError::task_creation_failed);
            }
            
            if (auto result = manager.add_task_to_hook(hook_address, std::move(task_ptr)); !result) {
                LOG_ERROR("Failed to add task '{}' to hook at address 0x{:X}", config->key(), hook_address);
                return std::unexpected(FactoryError::hook_creation_failed);
            }
            
            LOG_DEBUG("Successfully added task '{}' to hook at address 0x{:X}", config->key(), hook_address);
        }
    }
    
    return {};
}

std::string HookFactory::get_task_key_from_file(const std::string& config_file) {
    auto pos = config_file.find_last_of('/');
    std::string task_key = pos != std::string::npos ? 
        config_file.substr(pos + 1) : config_file;
    if (task_key.ends_with(".toml")) {
        task_key.erase(task_key.length() - 5);
    }
    return task_key;
}

FactoryResult HookFactory::create_tasks_for_address(
    std::uintptr_t address,
    const std::vector<config::ConfigPtr>& configs,
    HookManager& manager) {
    
    LOG_DEBUG("Creating tasks for hook at address 0x{:X} with {} config(s)", address, configs.size());
    
    for (const auto& config : configs) {
        // Create task using the TaskFactory
        auto task_ptr = task::TaskFactory::instance().create_task(*config);
        if (!task_ptr) {
            LOG_ERROR("Failed to create task for config '{}' - no task creator registered", config->key());
            return std::unexpected(FactoryError::task_creation_failed);
        }
        
        if (auto result = manager.add_task_to_hook(address, std::move(task_ptr)); !result) {
            LOG_ERROR("Failed to add task '{}' to hook at address 0x{:X}", config->key(), address);
            return std::unexpected(FactoryError::hook_creation_failed);
        }
        
        LOG_DEBUG("Successfully added task '{}' to hook at address 0x{:X}", config->key(), address);
    }
    
    return {};
}

std::uintptr_t HookFactory::extract_hook_address(const config::ConfigBase& config) {
    // Try to get hook address from config
    // This is a generic approach - specific config types should override this
    
    // For now, return 0 to indicate no address found
    // Plugins should register their own address extraction logic
    LOG_DEBUG("No hook address extraction logic for config type: {}", typeid(config).name());
    return 0;
}

} // namespace app_hook::hook
