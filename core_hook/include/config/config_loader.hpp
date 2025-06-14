#pragma once

#include "config_common.hpp"
#include "task_loader.hpp"

namespace app_hook::config {

/// @brief Configuration loader facade class (core functionality only)
/// @note Provides basic task loading functionality for core_hook
class ConfigLoader {
public:
    /// @brief Load task information from the main tasks.toml file
    /// @param tasks_file_path Path to the tasks.toml file
    /// @return Vector of task information or error
    [[nodiscard]] static ConfigResult<std::vector<TaskInfo>> 
    load_tasks(const std::string& tasks_file_path) {
        return TaskLoader::load_tasks(tasks_file_path);
    }
    
    /// @brief Load all configurations from tasks using the generic factory
    /// @param tasks_file_path Path to the main tasks.toml file
    /// @return Vector of configuration objects or error
    [[nodiscard]] static ConfigResult<std::vector<ConfigPtr>> 
    load_configs_from_tasks(const std::string& tasks_file_path) {
        return TaskLoader::load_configs_from_tasks(tasks_file_path);
    }
};

} // namespace app_hook::config 
