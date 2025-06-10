#include "../../include/config/task_loader.hpp"
#include "../../include/config/copy_memory_loader.hpp"

namespace ff8_hook::config {

ConfigResult<std::vector<CopyMemoryConfig>> 
TaskLoader::load_memory_configs_from_tasks(const std::string& tasks_file_path) {
    LOG_INFO("Loading memory configs from tasks file: {}", tasks_file_path);
    
    // First load the tasks
    auto tasks_result = load_tasks(tasks_file_path);
    if (!tasks_result) {
        return std::unexpected(tasks_result.error());
    }
    
    std::vector<CopyMemoryConfig> all_configs;
    
    // Load configs from each task file using the dedicated loader
    for (const auto& task : *tasks_result) {
        LOG_INFO("Loading memory configs from task '{}' file: {}", task.name, task.config_file);
        
        auto configs_result = CopyMemoryLoader::load_memory_configs(task.config_file);
        if (!configs_result) {
            LOG_ERROR("Failed to load configs from task file: {}", task.config_file);
            return std::unexpected(configs_result.error());
        }
        
        // Add configs from this task
        for (auto& config : *configs_result) {
            all_configs.push_back(std::move(config));
        }
        
        LOG_INFO("Loaded {} config(s) from task '{}' file: {}", 
                configs_result->size(), task.name, task.config_file);
    }
    
    LOG_INFO("Successfully loaded {} total memory config(s) from {} task(s)", 
            all_configs.size(), tasks_result->size());
    return all_configs;
}

} // namespace ff8_hook::config 