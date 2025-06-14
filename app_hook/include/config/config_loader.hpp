#pragma once

#include "config_common.hpp"
#include "task_loader.hpp"
#include "copy_memory_loader.hpp"
#include "patch_loader.hpp"
#include "memory_config.hpp"

namespace app_hook::config {

// Re-export types for backward compatibility
using TaskInfo = TaskInfo;
using InstructionPatch = InstructionPatch;

/// @brief Configuration loader facade class (backward compatibility)
/// @note Delegates to specialized loader classes
/// @deprecated Use TaskLoader, CopyMemoryLoader, or PatchLoader directly
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

    /// @brief Load memory copy configurations from a TOML file
    /// @param file_path Path to the configuration file
    /// @return Vector of memory copy configurations or error
    [[nodiscard]] static ConfigResult<std::vector<CopyMemoryConfig>> 
    load_memory_configs(const std::string& file_path) {
        return CopyMemoryLoader::load_memory_configs(file_path);
    }
    
    /// @brief Load patch instructions from a TOML file
    /// @param file_path Path to the patch file
    /// @return Vector of instruction patches or error
    [[nodiscard]] static ConfigResult<std::vector<InstructionPatch>> 
    load_patch_instructions(const std::string& file_path) {
        return PatchLoader::load_patch_instructions(file_path);
    }
};

} // namespace app_hook::config 
