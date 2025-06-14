#include "../../include/config/config_factory.hpp"

namespace app_hook::config {

// Static member definition
std::unordered_map<std::string, ConfigLoaderPtr> ConfigFactory::loaders_;

bool ConfigFactory::register_loader(ConfigLoaderPtr loader) {
    if (!loader) {
        LOG_ERROR("Cannot register null loader");
        return false;
    }

    const auto& name = loader->get_name();
    if (loaders_.find(name) != loaders_.end()) {
        LOG_WARNING("Loader '{}' is already registered - replacing", name);
    }

    LOG_INFO("Registering config loader: {} v{}", name, loader->get_version());
    
    // Log supported types
    auto supported = loader->supported_types();
    for (const auto& type : supported) {
        LOG_DEBUG("  - Supports: {}", to_string(type));
    }

    loaders_[name] = std::move(loader);
    return true;
}

bool ConfigFactory::unregister_loader(const std::string& loader_name) {
    auto it = loaders_.find(loader_name);
    if (it == loaders_.end()) {
        LOG_WARNING("Loader '{}' not found for unregistration", loader_name);
        return false;
    }

    LOG_INFO("Unregistering config loader: {}", loader_name);
    loaders_.erase(it);
    return true;
}

ConfigResult<std::vector<ConfigPtr>> 
ConfigFactory::load_configs(ConfigType type, const std::string& config_file, const std::string& task_name) {
    LOG_INFO("Loading {} configs from file: {} for task: {}", 
             to_string(type), config_file, task_name);

    auto* loader = find_loader_for_type(type);
    if (!loader) {
        LOG_ERROR("No registered loader supports configuration type: {}", to_string(type));
        return std::unexpected(ConfigError::invalid_format);
    }

    LOG_DEBUG("Using loader '{}' for type '{}'", loader->get_name(), to_string(type));
    return loader->load_configs(type, config_file, task_name);
}

std::vector<std::string> ConfigFactory::get_registered_loaders() {
    std::vector<std::string> names;
    names.reserve(loaders_.size());
    
    for (const auto& [name, loader] : loaders_) {
        names.push_back(name);
    }
    
    return names;
}

bool ConfigFactory::is_type_supported(ConfigType type) {
    return find_loader_for_type(type) != nullptr;
}

ConfigLoaderBase* ConfigFactory::find_loader_for_type(ConfigType type) {
    for (const auto& [name, loader] : loaders_) {
        const auto supported = loader->supported_types();
        if (std::find(supported.begin(), supported.end(), type) != supported.end()) {
            return loader.get();
        }
    }
    return nullptr;
}

} // namespace app_hook::config 
