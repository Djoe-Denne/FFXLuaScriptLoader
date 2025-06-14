#pragma once

#include "config_common.hpp"
#include "config_base.hpp"
#include "memory_config.hpp"
#include "config_factory.hpp"
#include <toml++/toml.h>
#include <vector>
#include <string>
#include <optional>

namespace app_hook::config {

/// @brief Task metadata from tasks.toml
struct TaskInfo {
    std::string name;                    ///< Display name of the task
    std::string description;             ///< Description of the task
    std::string config_file;             ///< Path to the task's config file
    ConfigType type;                     ///< Type of configuration
    std::vector<std::string> follow_by;  ///< Tasks to execute after this one completes
    bool enabled;                        ///< Whether the task is enabled
    
    /// @brief Constructor
    TaskInfo() : type(ConfigType::Unknown), enabled(true) {}
    
    /// @brief Check if this task info is valid
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return !name.empty() && !config_file.empty() && type != ConfigType::Unknown;
    }
    
    /// @brief Check if this task has follow-up tasks
    [[nodiscard]] constexpr bool has_follow_up_tasks() const noexcept {
        return !follow_by.empty();
    }
    
    /// @brief Get follow-up tasks
    [[nodiscard]] const std::vector<std::string>& get_follow_up_tasks() const noexcept {
        return follow_by;
    }
};

/// @brief Loader for task configuration files
/// @note Handles loading and parsing of tasks.toml files
class TaskLoader {
public:
    TaskLoader() = default;
    ~TaskLoader() = default;
    
    // Non-copyable but movable (following C++23 best practices)
    TaskLoader(const TaskLoader&) = delete;
    TaskLoader& operator=(const TaskLoader&) = delete;
    TaskLoader(TaskLoader&&) = default;
    TaskLoader& operator=(TaskLoader&&) = default;

    /// @brief Load task information from the main tasks.toml file
    /// @param tasks_file_path Path to the tasks.toml file
    /// @return Vector of task information or error
    [[nodiscard]] static ConfigResult<std::vector<TaskInfo>> 
    load_tasks(const std::string& tasks_file_path) {
        LOG_INFO("Loading tasks from: {}", tasks_file_path);
        
        std::vector<TaskInfo> tasks;
        
        try {
            // Parse TOML file
            auto toml = toml::parse_file(tasks_file_path);
            LOG_INFO("Successfully parsed tasks TOML file: {}", tasks_file_path);
            
            // Look for tasks section
            auto tasks_section = toml["tasks"];
            if (!tasks_section) {
                LOG_WARNING("No 'tasks' section found in TOML file: {}", tasks_file_path);
                return tasks; // Return empty vector
            }
            
            if (!tasks_section.is_table()) {
                LOG_ERROR("'tasks' is not a table in TOML file: {}", tasks_file_path);
                return std::unexpected(ConfigError::invalid_format);
            }
            
            // Process task entries using modern C++23 approach
            auto tasks_table = tasks_section.as_table();
            for (const auto& [key, value] : *tasks_table) {
                auto task_info = parse_single_task(std::string{key.str()}, value);
                if (task_info && task_info->is_valid() && task_info->enabled) {
                    tasks.push_back(std::move(*task_info));
                    LOG_INFO("Added enabled task '{}': {}", key.str(), task_info->name);
                } else if (task_info && !task_info->enabled) {
                    LOG_INFO("Skipping disabled task '{}'", key.str());
                } else {
                    LOG_WARNING("Invalid task '{}' - skipping", key.str());
                }
            }
            
        } catch (const toml::parse_error& e) {
            LOG_ERROR("TOML parse error in file '{}': {} at line {}, column {}", 
                     tasks_file_path, e.description(), e.source().begin.line, e.source().begin.column);
            return std::unexpected(ConfigError::parse_error);
        } catch (const std::exception& e) {
            LOG_ERROR("Exception while loading tasks file '{}': {}", tasks_file_path, e.what());
            return std::unexpected(ConfigError::parse_error);
        }
        
        LOG_INFO("Successfully loaded {} enabled task(s) from file: {}", tasks.size(), tasks_file_path);
        return tasks;
    }
    
    /// @brief Load all configurations from tasks using the generic factory
    /// @param tasks_file_path Path to the main tasks.toml file
    /// @return Vector of configuration objects or error
    [[nodiscard]] static ConfigResult<std::vector<ConfigPtr>> 
    load_configs_from_tasks(const std::string& tasks_file_path);

    /// @brief Build execution order respecting followBy dependencies
    /// @param tasks Vector of task information
    /// @return Vector of task keys in execution order or error if cycles detected
    [[nodiscard]] static ConfigResult<std::vector<std::string>>
    build_execution_order(const std::vector<TaskInfo>& tasks);

private:
    /// @brief Parse a single task entry from TOML
    /// @param key_str Task key
    /// @param value TOML value containing task data
    /// @return Parsed task info or nullopt if parsing failed
    [[nodiscard]] static std::optional<TaskInfo> 
    parse_single_task(const std::string& key_str, const toml::node& value) {
        LOG_DEBUG("Processing task: '{}'", key_str);
        
        if (!value.is_table()) {
            LOG_WARNING("Task '{}' is not a table, skipping", key_str);
            return std::nullopt;
        }
        
        auto task_table = value.as_table();
        TaskInfo task_info;
        
        // Parse task fields using structured binding and modern C++
        if (auto name = task_table->get("name")) {
            if (auto name_str = name->value<std::string>()) {
                task_info.name = *name_str;
            }
        }
        
        if (auto description = task_table->get("description")) {
            if (auto desc_str = description->value<std::string>()) {
                task_info.description = *desc_str;
            }
        }
        
        if (auto config_file = task_table->get("config_file")) {
            if (auto config_str = config_file->value<std::string>()) {
                task_info.config_file = *config_str;
            }
        }
        
        // Parse type field
        if (auto type_field = task_table->get("type")) {
            if (auto type_str = type_field->value<std::string>()) {
                task_info.type = from_string(*type_str);
                LOG_DEBUG("Parsed task type: '{}' -> {}", *type_str, static_cast<int>(task_info.type));
            }
        }
        
        // Parse followBy field (can be string or array of strings)
        if (auto follow_by_field = task_table->get("followBy")) {
            if (auto follow_by_str = follow_by_field->value<std::string>()) {
                // Single task as string
                task_info.follow_by.push_back(*follow_by_str);
                LOG_DEBUG("Parsed single followBy task: '{}'", *follow_by_str);
            } else if (follow_by_field->is_array()) {
                // Multiple tasks as array
                auto follow_by_array = follow_by_field->as_array();
                for (const auto& task_node : *follow_by_array) {
                    if (auto task_str = task_node.value<std::string>()) {
                        task_info.follow_by.push_back(*task_str);
                        LOG_DEBUG("Parsed followBy task: '{}'", *task_str);
                    }
                }
            }
        }
        
        // Parse enabled field with default value
        if (auto enabled = task_table->get("enabled")) {
            if (auto enabled_val = enabled->value<bool>()) {
                task_info.enabled = *enabled_val;
            } else {
                task_info.enabled = true; // Default to enabled
            }
        } else {
            task_info.enabled = true; // Default to enabled
        }
        
        return task_info;
    }
};

} // namespace app_hook::config 
