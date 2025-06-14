#include "../../include/util/task_manager.hpp"

namespace app_hook::util {

bool TaskManager::load_from_tasks_config(const std::string& tasks_config_path) {
    auto config_result = config::ConfigLoader::load_configs_from_tasks(tasks_config_path);
    if (!config_result) {
        return false;
    }
    
    // Clear existing tasks
    tasks_.clear();
    
    // Note: Task creation from configs is handled by specific implementations
    // This core version just validates that configs can be loaded
    return !config_result->empty();
}

bool TaskManager::load_from_config(const std::string& config_path) {
    // Note: This method is deprecated in core_hook
    // Specific implementations should override this method
        return false;
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

} // namespace app_hook::util 
