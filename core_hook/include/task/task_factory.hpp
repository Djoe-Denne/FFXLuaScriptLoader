#pragma once

#include "hook_task.hpp"
#include "../config/config_base.hpp"
#include <functional>
#include <unordered_map>
#include <string>
#include <memory>
#include <expected>

namespace app_hook::task {

/// @brief Task creation function signature
/// @param config Configuration to create task from
/// @return Created task or nullptr if failed
using TaskCreatorFunc = std::function<HookTaskPtr(const config::ConfigBase& config)>;

/// @brief Generic task factory that supports plugin registration
class TaskFactory {
public:
    /// @brief Get the singleton instance
    /// @return Task factory instance
    static TaskFactory& instance();

    /// @brief Register a task creator for a specific config type
    /// @param config_type_name Name of the configuration type (e.g., "CopyMemoryConfig")
    /// @param creator Function to create tasks from this config type
    /// @return True if registered successfully
    bool register_task_creator(const std::string& config_type_name, TaskCreatorFunc creator);

    /// @brief Create a task from a configuration
    /// @param config Configuration to create task from
    /// @return Created task or nullptr if no creator found
    HookTaskPtr create_task(const config::ConfigBase& config);

    /// @brief Check if a task creator is registered for a config type
    /// @param config_type_name Name of the configuration type
    /// @return True if creator is registered
    bool has_creator(const std::string& config_type_name) const;

    /// @brief Get list of registered config types
    /// @return Vector of registered config type names
    std::vector<std::string> get_registered_types() const;

    /// @brief Clear all registered creators (for testing)
    void clear_creators();

private:
    TaskFactory() = default;
    ~TaskFactory() = default;

    // Non-copyable, non-movable
    TaskFactory(const TaskFactory&) = delete;
    TaskFactory& operator=(const TaskFactory&) = delete;
    TaskFactory(TaskFactory&&) = delete;
    TaskFactory& operator=(TaskFactory&&) = delete;

    std::unordered_map<std::string, TaskCreatorFunc> creators_;
};

/// @brief Helper macro to register a task creator
/// @param ConfigType The configuration type class
/// @param TaskType The task type class
#define REGISTER_TASK_CREATOR(ConfigType, TaskType) \
    TaskFactory::instance().register_task_creator(#ConfigType, \
        [](const config::ConfigBase& base_config) -> HookTaskPtr { \
            if (const auto* typed_config = dynamic_cast<const ConfigType*>(&base_config)) { \
                return make_task<TaskType>(*typed_config); \
            } \
            return nullptr; \
        })

} // namespace app_hook::task 