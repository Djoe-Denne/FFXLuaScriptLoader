#include "../../include/config/config_factory.hpp"

namespace app_hook::config {

ConfigResult<std::vector<ConfigPtr>> 
ConfigFactory::load_configs(ConfigType type, const std::string& config_file, const std::string& task_name) {
    LOG_INFO("Loading {} configs from file: {} for task: {}", 
             to_string(type), config_file, task_name);

    switch (type) {
        case ConfigType::Memory:
            return load_memory_configs(config_file, task_name);
        
        case ConfigType::Patch:
            return load_patch_configs(config_file, task_name);
        
        case ConfigType::Script:
        case ConfigType::Audio:
        case ConfigType::Graphics:
            LOG_WARNING("Configuration type '{}' is not yet implemented", to_string(type));
            return std::vector<ConfigPtr>{};
        
        case ConfigType::Unknown:
        default:
            LOG_ERROR("Unknown configuration type: {}", static_cast<int>(type));
            return std::unexpected(ConfigError::invalid_format);
    }
}

ConfigResult<std::vector<ConfigPtr>> 
ConfigFactory::load_memory_configs(const std::string& config_file, const std::string& task_name) {
    LOG_DEBUG("Loading memory configurations from: {}", config_file);
    
    auto memory_configs_result = CopyMemoryLoader::load_memory_configs(config_file);
    if (!memory_configs_result) {
        LOG_ERROR("Failed to load memory configurations from: {}", config_file);
        return std::unexpected(memory_configs_result.error());
    }
    
    std::vector<ConfigPtr> configs;
    configs.reserve(memory_configs_result->size());
    
    for (auto&& memory_config : *memory_configs_result) {
        auto config_ptr = make_config<CopyMemoryConfig>(std::move(memory_config));
        configs.push_back(std::move(config_ptr));
    }
    
    LOG_INFO("Loaded {} memory configuration(s) for task: {}", configs.size(), task_name);
    return configs;
}

ConfigResult<std::vector<ConfigPtr>> 
ConfigFactory::load_patch_configs(const std::string& config_file, const std::string& task_name) {
    LOG_DEBUG("Loading patch configurations from: {}", config_file);
    
    auto patch_instructions_result = PatchLoader::load_patch_instructions(config_file);
    if (!patch_instructions_result) {
        LOG_ERROR("Failed to load patch instructions from: {}", config_file);
        return std::unexpected(patch_instructions_result.error());
    }
    
    std::vector<ConfigPtr> configs;
    
    if (!patch_instructions_result->empty()) {
        // Create a single PatchConfig with all instructions
        auto patch_config = make_config<PatchConfig>(task_name, task_name);
        auto* patch_config_ptr = static_cast<PatchConfig*>(patch_config.get());
        
        patch_config_ptr->set_patch_file_path(config_file);
        patch_config_ptr->set_instructions(std::move(*patch_instructions_result));
        
        configs.push_back(std::move(patch_config));
    }
    
    LOG_INFO("Loaded {} patch configuration(s) for task: {}", configs.size(), task_name);
    return configs;
}

} // namespace app_hook::config 
