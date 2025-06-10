#pragma once

#include "../task/hook_task.hpp"
#include "../memory/copy_memory.hpp"
#include "../config/config_loader.hpp"
#include <vector>
#include <string>
#include <expected>
#include <algorithm>
#include <ranges>

namespace ff8_hook::util {

/// @brief Manager for hook tasks
class TaskManager {
public:
    TaskManager() = default;
    ~TaskManager() = default;
    
    // Non-copyable but movable
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;
    TaskManager(TaskManager&&) = default;
    TaskManager& operator=(TaskManager&&) = default;
    
    /// @brief Load tasks from tasks configuration file
    /// @param tasks_config_path Path to the tasks.toml configuration file
    /// @return true if loading succeeded
    [[nodiscard]] bool load_from_tasks_config(const std::string& tasks_config_path) {
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

    /// @brief Load tasks from configuration file (legacy method)
    /// @param config_path Path to the configuration file
    /// @return true if loading succeeded
    [[nodiscard]] bool load_from_config(const std::string& config_path) {
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
    
    /// @brief Add a task to the manager
    /// @param task Task to add
    void add_task(task::HookTaskPtr task) {
        if (task) {
            tasks_.push_back(std::move(task));
        }
    }
    
    /// @brief Execute all tasks
    /// @return true if all tasks succeeded
    [[nodiscard]] bool execute_all() {
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
    
    /// @brief Execute all tasks and return detailed results
    /// @return Vector of task results with names
    [[nodiscard]] std::vector<std::pair<std::string, task::TaskResult>> execute_all_detailed() {
        std::vector<std::pair<std::string, task::TaskResult>> results;
        results.reserve(tasks_.size());
        
        for (const auto& task : tasks_) {
            results.emplace_back(task->name(), task->execute());
        }
        
        return results;
    }
    
    /// @brief Get the number of tasks
    /// @return Number of tasks
    [[nodiscard]] std::size_t task_count() const noexcept {
        return tasks_.size();
    }
    
    /// @brief Check if the manager has any tasks
    /// @return true if there are tasks
    [[nodiscard]] bool has_tasks() const noexcept {
        return !tasks_.empty();
    }
    
    /// @brief Get task names
    /// @return Vector of task names
    [[nodiscard]] std::vector<std::string> get_task_names() const {
        return tasks_ 
            | std::views::transform([](const auto& task) { return task->name(); })
            | std::ranges::to<std::vector>();
    }
    
    /// @brief Clear all tasks
    void clear() noexcept {
        tasks_.clear();
    }
    
private:
    std::vector<task::HookTaskPtr> tasks_;
};

} // namespace ff8_hook::util 