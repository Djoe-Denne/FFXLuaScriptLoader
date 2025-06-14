#include "../../include/util/task_manager.hpp"

namespace ff8_hook::util {

bool TaskManager::load_from_tasks_config(const std::string& tasks_config_path) {
    auto config_result = config::ConfigLoader::load_memory_configs_from_tasks(tasks_config_path);
    if (!config_result) {
        return false;
    }
    
    // Clear existing tasks
    tasks_.clear();
    
    // Create copy memory tasks from configuration
    for (auto&& config : *config_result) {
        tasks_.push_back(task::make_task<memory::CopyMemoryTask>(std::move(config)));
    }
    
    return true;
}

bool TaskManager::load_from_config(const std::string& config_path) {
    auto config_result = config::ConfigLoader::load_memory_configs(config_path);
    if (!config_result) {
        return false;
    }
    
    // Clear existing tasks
    tasks_.clear();
    
    // Create copy memory tasks from configuration
    for (auto&& config : *config_result) {
        tasks_.push_back(task::make_task<memory::CopyMemoryTask>(std::move(config)));
    }
    
    return true;
}

void TaskManager::add_task(task::HookTaskPtr task) {
    if (task) {
        tasks_.push_back(std::move(task));
    }
}

bool TaskManager::execute_all() {
    if (tasks_.empty()) {
        return true;
    }
    
    // Execute all tasks and collect results
    const auto results = tasks_ 
        | std::views::transform([](const auto& task) { return task->execute(); })
        | std::ranges::to<std::vector>();
    
    // Check if all tasks succeeded
    return std::ranges::all_of(results, [](const auto& result) { 
        return result.has_value(); 
    });
}

std::vector<std::pair<std::string, task::TaskResult>> TaskManager::execute_all_detailed() {
    std::vector<std::pair<std::string, task::TaskResult>> results;
    results.reserve(tasks_.size());
    
    for (const auto& task : tasks_) {
        results.emplace_back(task->name(), task->execute());
    }
    
    return results;
}

std::vector<std::string> TaskManager::get_task_names() const {
    return tasks_ 
        | std::views::transform([](const auto& task) { return task->name(); })
        | std::ranges::to<std::vector>();
}

} // namespace ff8_hook::util 