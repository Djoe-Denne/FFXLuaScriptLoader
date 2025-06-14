#include "../../include/config/task_loader.hpp"
#include "../../include/config/config_factory.hpp"
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <ranges>
#include <algorithm>

namespace app_hook::config {

ConfigResult<std::vector<ConfigPtr>> 
TaskLoader::load_configs_from_tasks(const std::string& tasks_file_path) {
    LOG_INFO("Loading configs from tasks file: {}", tasks_file_path);
    
    // First load the tasks
    auto tasks_result = load_tasks(tasks_file_path);
    if (!tasks_result) {
        return std::unexpected(tasks_result.error());
    }
    
    std::vector<ConfigPtr> all_configs;
    
    // Load configs from each task file using the generic factory
    for (const auto& task : *tasks_result) {
        LOG_INFO("Loading {} configs from task '{}' file: {}", 
                 to_string(task.type), task.name, task.config_file);
        
        auto configs_result = ConfigFactory::load_configs(task.type, task.config_file, task.name);
        if (!configs_result) {
            LOG_ERROR("Failed to load configs from task file: {}", task.config_file);
            return std::unexpected(configs_result.error());
        }
        
        // Add configs from this task
        for (auto& config : *configs_result) {
            all_configs.push_back(std::move(config));
        }
        
        LOG_INFO("Loaded {} config(s) from task '{}' file: {}", 
                configs_result->size(), task.name, task.config_file);
    }
    
    LOG_INFO("Successfully loaded {} total config(s) from {} task(s)", 
            all_configs.size(), tasks_result->size());
    return all_configs;
}

ConfigResult<std::vector<std::string>>
TaskLoader::build_execution_order(const std::vector<TaskInfo>& tasks) {
    LOG_INFO("Building task execution order for {} task(s)", tasks.size());
    
    // Create a map of task names to TaskInfo for quick lookup
    std::unordered_map<std::string, const TaskInfo*> task_map;
    for (const auto& task : tasks) {
        // Extract task name from key (format: "tasks.task_name")
        auto pos = task.config_file.find_last_of('/');
        std::string task_key = pos != std::string::npos ? 
            task.config_file.substr(pos + 1) : task.config_file;
        
        // Remove .toml extension
        if (task_key.ends_with(".toml")) {
            task_key.erase(task_key.length() - 5);
        }
        
        task_map[task_key] = &task;
        LOG_DEBUG("Mapped task key '{}' to task '{}'", task_key, task.name);
    }
    
    // Build dependency graph using modern C++23 features
    std::unordered_map<std::string, std::vector<std::string>> dependencies;
    std::unordered_set<std::string> all_tasks;
    
    for (const auto& task : tasks) {
        auto task_key = task.config_file;
        auto pos = task_key.find_last_of('/');
        if (pos != std::string::npos) task_key = task_key.substr(pos + 1);
        if (task_key.ends_with(".toml")) task_key.erase(task_key.length() - 5);
        
        all_tasks.insert(task_key);
        dependencies[task_key] = task.follow_by;
        
        if (task.has_follow_up_tasks()) {
            LOG_DEBUG("Task '{}' has {} follow-up task(s)", task_key, task.follow_by.size());
            for (const auto& follow_task : task.follow_by) {
                LOG_DEBUG("  -> '{}'", follow_task);
            }
        }
    }
    
    // Topological sort to detect cycles and determine execution order
    std::vector<std::string> execution_order;
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> in_path;
    
    std::function<bool(const std::string&)> visit_task = [&](const std::string& task_key) -> bool {
        if (in_path.contains(task_key)) {
            LOG_ERROR("Circular dependency detected involving task '{}'", task_key);
            return false; // Cycle detected
        }
        
        if (visited.contains(task_key)) {
            return true; // Already processed
        }
        
        in_path.insert(task_key);
        
        // Visit all follow-up tasks first (reverse dependency)
        if (dependencies.contains(task_key)) {
            for (const auto& follow_task : dependencies[task_key]) {
                if (all_tasks.contains(follow_task)) {
                    if (!visit_task(follow_task)) {
                        return false; // Propagate cycle detection
                    }
                } else {
                    LOG_WARNING("Follow-up task '{}' referenced by '{}' not found", follow_task, task_key);
                }
            }
        }
        
        in_path.erase(task_key);
        visited.insert(task_key);
        execution_order.push_back(task_key);
        
        return true;
    };
    
    // Visit all tasks
    for (const auto& task_key : all_tasks) {
        if (!visit_task(task_key)) {
            return std::unexpected(ConfigError::invalid_format);
        }
    }
    
    // Reverse to get correct execution order (dependencies first)
    std::ranges::reverse(execution_order);
    
    // Build execution order string manually since fmt::join isn't available
    std::string order_str;
    for (size_t i = 0; i < execution_order.size(); ++i) {
        if (i > 0) order_str += " -> ";
        order_str += execution_order[i];
    }
    LOG_INFO("Built execution order: {}", order_str);
    return execution_order;
}

} // namespace app_hook::config 
