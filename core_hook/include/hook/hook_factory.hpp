#pragma once

#include "../config/config_loader.hpp"
#include "../config/task_loader.hpp"
#include "../task/hook_task.hpp"
#include "../task/task_factory.hpp"
#include "hook_manager.hpp"
#include <vector>
#include <unordered_map>
#include <expected>
#include <algorithm>
#include <sstream>

namespace app_hook::hook {

/// @brief Error types for hook factory operations
enum class FactoryError {
    success = 0,
    config_load_failed,
    invalid_config,
    hook_creation_failed,
    task_creation_failed
};

/// @brief Result type for hook factory operations
using FactoryResult = std::expected<void, FactoryError>;

/// @brief Generic factory class for creating hooks from configuration
/// Uses the TaskFactory for plugin-based task creation
class HookFactory {
public:
    HookFactory() = default;
    ~HookFactory() = default;
    
    // Non-copyable but movable (following C++23 best practices)
    HookFactory(const HookFactory&) = delete;
    HookFactory& operator=(const HookFactory&) = delete;
    HookFactory(HookFactory&&) = default;
    HookFactory& operator=(HookFactory&&) = default;

    /// @brief Create hooks from tasks configuration and add them to the manager
    /// @param tasks_path Path to tasks configuration file  
    /// @param manager Hook manager to add hooks to
    /// @return Result of operation
    [[nodiscard]] static FactoryResult create_hooks_from_tasks(
        const std::string& tasks_path,
        HookManager& manager);

    /// @brief Create hooks from generic configurations and add them to the manager
    /// @param configs Vector of configurations to create hooks from
    /// @param manager Hook manager to add hooks to
    /// @return Result of operation
    [[nodiscard]] static FactoryResult create_hooks_from_configs(
        const std::vector<config::ConfigPtr>& configs,
        HookManager& manager);
    
private:
    /// @brief Process a single task with dependency handling
    /// @param task Task information
    /// @param task_hook_addresses Map tracking where each task was hooked
    /// @param task_info_map Map of task keys to task information for followBy lookup
    /// @param manager Hook manager to add tasks to
    /// @return Result of operation
    [[nodiscard]] static FactoryResult process_task_with_dependencies(
        const config::TaskInfo& task,
        std::unordered_map<std::string, std::uintptr_t>& task_hook_addresses,
        const std::unordered_map<std::string, const config::TaskInfo*>& task_info_map,
        HookManager& manager);
    
    /// @brief Extract task key from config file path
    /// @param config_file Path to config file
    /// @return Task key
    [[nodiscard]] static std::string get_task_key_from_file(const std::string& config_file);
    
    /// @brief Create tasks for a specific address using the TaskFactory
    /// @param address Hook address
    /// @param configs Task configurations for this address
    /// @param manager Hook manager to add hook to
    /// @return Result of operation
    [[nodiscard]] static FactoryResult create_tasks_for_address(
        std::uintptr_t address,
        const std::vector<config::ConfigPtr>& configs,
        HookManager& manager);

    /// @brief Extract hook address from a configuration
    /// @param config Configuration to extract address from
    /// @return Hook address or 0 if not found
    [[nodiscard]] static std::uintptr_t extract_hook_address(const config::ConfigBase& config);
};

} // namespace app_hook::hook
