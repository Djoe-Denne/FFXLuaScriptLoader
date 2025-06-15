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
    
    // Create a map of task information for quick lookup
    std::unordered_map<std::string, const config::TaskInfo*> task_info_map;
    for (const auto& task : *tasks_result) {
        task_info_map[get_task_key_from_file(task.config_file)] = &task;
    }
    
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
        
        if (auto result = process_task_with_dependencies(*task_it, task_hook_addresses, task_info_map, manager); !result) {
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
    const std::unordered_map<std::string, const config::TaskInfo*>& task_info_map,
    HookManager& manager) {
    
    LOG_DEBUG("Processing task '{}' of type '{}'", task.name, to_string(task.type));
    
    // Load configurations for this task using the generic factory
    auto configs_result = config::ConfigFactory::load_configs(task.type, task.config_file, task.name);
    if (!configs_result) {
        LOG_ERROR("Failed to load configs for task '{}'", task.name);
        return std::unexpected(FactoryError::config_load_failed);
    }
    
    LOG_DEBUG("Loaded {} configuration(s) for task '{}'", configs_result->size(), task.name);
    
    const auto task_key = get_task_key_from_file(task.config_file);
    
    // For memory tasks, they define their own hook addresses
    if (task.type == config::ConfigType::Memory) {
        LOG_DEBUG("Processing memory task - will define own hook addresses");
        
        for (const auto& config : *configs_result) {
            LOG_DEBUG("Processing memory config: '{}'", config->key());
            
            std::uintptr_t hook_address = extract_hook_address(*config);
            LOG_DEBUG("extract_hook_address returned: 0x{:X} for config '{}'", hook_address, config->key());
            
            if (hook_address == 0) {
                LOG_WARNING("No hook address found for config '{}' - skipping", config->key());
                continue;
            }
            
            // Create task using the TaskFactory
            LOG_DEBUG("Creating task for config: '{}'", config->key());
            auto task_ptr = task::TaskFactory::instance().create_task(*config);
            if (!task_ptr) {
                LOG_ERROR("Failed to create task for config '{}'", config->key());
                return std::unexpected(FactoryError::task_creation_failed);
            }
            
            LOG_DEBUG("Adding task to hook at address 0x{:X}", hook_address);
            if (auto result = manager.add_task_to_hook(hook_address, std::move(task_ptr)); !result) {
                LOG_ERROR("Failed to add task '{}' to hook at address 0x{:X}", config->key(), hook_address);
                return std::unexpected(FactoryError::hook_creation_failed);
            }
            
            // Remember where this task was hooked for followBy tasks
            task_hook_addresses[task_key] = hook_address;
            LOG_DEBUG("Recorded task '{}' at hook address 0x{:X}", task_key, hook_address);
        }
        
        LOG_DEBUG("Completed processing memory task '{}' with {} configs", task.name, configs_result->size());
    } 
    // For other task types (like patches), they are following tasks that should use parent task's hook address
    else {
        LOG_DEBUG("Processing following task - will use parent hook address");
        
        std::uintptr_t hook_address = 0;
        
        // This is a following task - it should be attached to the same hook as its parent
        // Find which task this one is following by looking at the followBy relationships
        std::string parent_task_key;
        
        LOG_DEBUG("Looking for parent hook address from {} existing task(s)", task_hook_addresses.size());
        
        // Find the parent task that has this task in its followBy list
        for (const auto& [existing_task_key, existing_hook_address] : task_hook_addresses) {
            LOG_DEBUG("  Available task: '{}' at address 0x{:X}", existing_task_key, existing_hook_address);
            
            // Check if this existing task has the current task in its followBy list
            auto parent_task_it = task_info_map.find(existing_task_key);
            if (parent_task_it != task_info_map.end()) {
                const auto& parent_task = *parent_task_it->second;
                
                // Check if this parent task has the current task in its followBy list
                auto follow_it = std::find(parent_task.follow_by.begin(), parent_task.follow_by.end(), task_key);
                if (follow_it != parent_task.follow_by.end()) {
                    hook_address = existing_hook_address;
                    parent_task_key = existing_task_key;
                    LOG_DEBUG("Following task '{}' will use hook address 0x{:X} from parent task '{}'", 
                             task_key, hook_address, parent_task_key);
                    break;
                }
            }
        }
        
        if (hook_address == 0) {
            LOG_ERROR("No parent hook address found for following task '{}'", task_key);
            LOG_ERROR("Following tasks must be processed after their parent address-triggered tasks");
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
            
            LOG_INFO("Successfully added following task '{}' to hook at address 0x{:X} (parent: '{}')", 
                     config->key(), hook_address, parent_task_key);
        }
        
        // Record this task's hook address for any tasks that might follow it
        task_hook_addresses[task_key] = hook_address;
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
    // Use the polymorphic AddressTrigger interface
    if (config.is_address_trigger()) {
        std::uintptr_t hook_address = config.get_hook_address_if_trigger();
        LOG_DEBUG("Extracted hook address 0x{:X} from address trigger config '{}'", hook_address, config.key());
        return hook_address;
    }
    
    // Non-address trigger configs don't provide hook addresses
    LOG_DEBUG("Config '{}' is not an address trigger - no hook address", config.key());
    return 0;
}

} // namespace app_hook::hook
