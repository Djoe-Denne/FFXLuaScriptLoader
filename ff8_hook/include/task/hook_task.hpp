#pragma once

#include <concepts>
#include <string>
#include <memory>
#include <expected>
#include <system_error>

namespace ff8_hook::task {

/// @brief Error codes for task operations
enum class TaskError {
    success = 0,
    invalid_config,
    memory_allocation_failed,
    invalid_address,
    copy_failed,
    dependency_not_met,
    patch_failed,
    unknown_error
};

/// @brief Task result type
using TaskResult = std::expected<void, TaskError>;

/// @brief Base concept for hook tasks
template<typename T>
concept HookTask = requires(T t) {
    { t.execute() } -> std::same_as<TaskResult>;
    { t.name() } -> std::convertible_to<std::string>;
    { t.description() } -> std::convertible_to<std::string>;
};

/// @brief Abstract base class for hook tasks
class IHookTask {
public:
    virtual ~IHookTask() = default;
    
    /// @brief Execute the task
    /// @return Task result indicating success or failure
    [[nodiscard]] virtual TaskResult execute() = 0;
    
    /// @brief Get the task name
    /// @return Task name
    [[nodiscard]] virtual std::string name() const = 0;
    
    /// @brief Get the task description
    /// @return Task description
    [[nodiscard]] virtual std::string description() const = 0;
    
protected:
    IHookTask() = default;
    IHookTask(const IHookTask&) = default;
    IHookTask& operator=(const IHookTask&) = default;
    IHookTask(IHookTask&&) = default;
    IHookTask& operator=(IHookTask&&) = default;
};

/// @brief Unique pointer to a hook task
using HookTaskPtr = std::unique_ptr<IHookTask>;

/// @brief Create a hook task
/// @tparam TaskType Type of task to create
/// @tparam Args Constructor argument types
/// @param args Constructor arguments
/// @return Unique pointer to the created task
template<HookTask TaskType, typename... Args>
[[nodiscard]] HookTaskPtr make_task(Args&&... args) {
    return std::make_unique<TaskType>(std::forward<Args>(args)...);
}

} // namespace ff8_hook::task
