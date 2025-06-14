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
    [[nodiscard]] bool load_from_tasks_config(const std::string& tasks_config_path);

    /// @brief Load tasks from configuration file (legacy method)
    /// @param config_path Path to the configuration file
    /// @return true if loading succeeded
    [[nodiscard]] bool load_from_config(const std::string& config_path);
    
    /// @brief Add a task to the manager
    /// @param task Task to add
    void add_task(task::HookTaskPtr task);
    
    /// @brief Execute all tasks
    /// @return true if all tasks succeeded
    [[nodiscard]] bool execute_all();
    
    /// @brief Execute all tasks and return detailed results
    /// @return Vector of task results with names
    [[nodiscard]] std::vector<std::pair<std::string, task::TaskResult>> execute_all_detailed();
    
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
    [[nodiscard]] std::vector<std::string> get_task_names() const;
    
    /// @brief Clear all tasks
    void clear() noexcept;
    
private:
    std::vector<task::HookTaskPtr> tasks_;
};

} // namespace ff8_hook::util 