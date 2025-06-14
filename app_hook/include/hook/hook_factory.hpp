#pragma once

#include "../config/config_loader.hpp"
#include "../config/task_loader.hpp"
#include "../task/hook_task.hpp"
#include "../memory/copy_memory.hpp"
#include "../memory/patch_memory.hpp"
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

/// @brief Factory class for creating hooks from configuration
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

    /// @brief Create hooks from configuration file and add them to the manager
    /// @param config_path Path to configuration file
    /// @param manager Hook manager to add hooks to
    /// @return Result of operation
    [[nodiscard]] static FactoryResult create_hooks_from_config(
        const std::string& config_path,
        HookManager& manager);
    
    /// @brief Create hooks from configuration file with custom copyAfter parsing
    /// @param config_path Path to configuration file
    /// @param manager Hook manager to add hooks to
    /// @param extract_copy_after Function to extract copyAfter address from config
    /// @return Result of operation
    template<typename ExtractorFunc>
    [[nodiscard]] static FactoryResult create_hooks_from_config_with_extractor(
        const std::string& config_path,
        HookManager& manager,
        ExtractorFunc&& extract_copy_after);
    
private:
    /// @brief Process a single task with dependency handling
    /// @param task Task information
    /// @param task_hook_addresses Map tracking where each task was hooked
    /// @param manager Hook manager to add tasks to
    /// @return Result of operation
    [[nodiscard]] static FactoryResult process_task_with_dependencies(
        const config::TaskInfo& task,
        std::unordered_map<std::string, std::uintptr_t>& task_hook_addresses,
        HookManager& manager);
    
    /// @brief Extract task key from config file path
    /// @param config_file Path to config file
    /// @return Task key
    [[nodiscard]] static std::string get_task_key_from_file(const std::string& config_file);
    
    /// @brief Create a hook for a specific address with multiple tasks (generic version)
    /// @param address Hook address
    /// @param configs Task configurations for this address
    /// @param manager Hook manager to add hook to
    /// @return Result of operation
    [[nodiscard]] static FactoryResult create_hook_for_address_generic(
        std::uintptr_t address,
        const std::vector<config::ConfigPtr>& configs,
        HookManager& manager);

    /// @brief Create a hook for a specific address with multiple tasks (legacy version)
    /// @param address Hook address
    /// @param configs Task configurations for this address
    /// @param manager Hook manager to add hook to
    /// @return Result of operation
    [[nodiscard]] static FactoryResult create_hook_for_address(
        std::uintptr_t address,
        const std::vector<config::CopyMemoryConfig>& configs,
        HookManager& manager);
};

} // namespace app_hook::hook
