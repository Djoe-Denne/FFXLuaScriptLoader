#include "plugin/plugin_manager.hpp"
#include "config/config_factory.hpp"
#include "util/logger.hpp"
#include <filesystem>
#include <algorithm>

namespace app_hook::plugin {

// PluginHost implementation
PluginHost::PluginHost() = default;

void PluginHost::set_config_registry(std::function<void(std::unique_ptr<::app_hook::config::ConfigBase>)> callback) {
    config_registry_ = std::move(callback);
}

void PluginHost::set_data_path(const std::string& path) {
    data_path_ = path;
}

PluginResult PluginHost::register_config(std::unique_ptr<::app_hook::config::ConfigBase> config) {
    if (!config_registry_) {
        LOG_ERROR("Config registry callback not set");
        return PluginResult::Failed;
    }
    
    if (!config || !config->is_valid()) {
        LOG_ERROR("Invalid configuration provided");
        return PluginResult::InvalidConfig;
    }
    
    LOG_DEBUG("Registering configuration: {} ({})", config->key(), config->name());
    config_registry_(std::move(config));
    return PluginResult::Success;
}

PluginResult PluginHost::register_config_loader(std::unique_ptr<::app_hook::config::ConfigLoaderBase> loader) {
    if (!loader) {
        LOG_ERROR("Cannot register null config loader");
        return PluginResult::Failed;
    }
    
    LOG_INFO("Plugin registering config loader: {} v{}", loader->get_name(), loader->get_version());
    
    if (app_hook::config::ConfigFactory::register_loader(std::move(loader))) {
        return PluginResult::Success;
    } else {
        LOG_ERROR("Failed to register config loader with factory");
        return PluginResult::Failed;
    }
}

std::string PluginHost::get_plugin_data_path() const {
    return data_path_;
}

void PluginHost::log_message(int level, const std::string& message) {
    // Map plugin log levels to spdlog levels
    switch (level) {
        case 0: LOG_TRACE("{}", message); break;
        case 1: LOG_DEBUG("{}", message); break;
        case 2: LOG_INFO("{}", message); break;
        case 3: LOG_WARN("{}", message); break;
        case 4: LOG_ERROR("{}", message); break;
        case 5: LOG_CRITICAL("{}", message); break;
        default: LOG_INFO("{}", message); break;
    }
}

std::pair<void*, std::uint32_t> PluginHost::get_process_info() const {
    return {GetCurrentProcess(), GetCurrentProcessId()};
}

// PluginManager implementation
PluginManager::PluginManager() 
    : host_(std::make_unique<PluginHost>()), initialized_(false) {
}

PluginManager::~PluginManager() {
    unload_all_plugins();
}

PluginResult PluginManager::initialize(
    const std::string& data_path,
    std::function<void(std::unique_ptr<::app_hook::config::ConfigBase>)> config_registry) {
    
    if (initialized_) {
        LOG_WARN("Plugin manager already initialized");
        return PluginResult::AlreadyLoaded;
    }
    
    host_->set_data_path(data_path);
    host_->set_config_registry(std::move(config_registry));
    initialized_ = true;
    
    LOG_INFO("Plugin manager initialized with data path: {}", data_path);
    return PluginResult::Success;
}

PluginResult PluginManager::load_plugin(const std::string& plugin_path) {
    if (!initialized_) {
        LOG_ERROR("Plugin manager not initialized");
        return PluginResult::Failed;
    }
    
    LOG_INFO("Loading plugin from: {}", plugin_path);
    
    auto plugin_instance = load_plugin_dll(plugin_path);
    if (!plugin_instance) {
        LOG_ERROR("Failed to load plugin DLL: {}", plugin_path);
        return PluginResult::Failed;
    }
    
    const auto& info = plugin_instance->info;
    
    // Check if plugin is already loaded
    if (plugins_.find(info.name) != plugins_.end()) {
        LOG_WARN("Plugin '{}' is already loaded", info.name);
        FreeLibrary(plugin_instance->module_handle);
        return PluginResult::AlreadyLoaded;
    }
    
    // Validate API version
    if (!validate_plugin_version(plugin_instance->plugin.get())) {
        LOG_ERROR("Plugin '{}' has incompatible API version", info.name);
        FreeLibrary(plugin_instance->module_handle);
        return PluginResult::InvalidVersion;
    }
    
    LOG_INFO("Successfully loaded plugin: {} v{} ({})", 
             info.name, info.version, info.description);
    
    plugins_[info.name] = std::move(plugin_instance);
    return PluginResult::Success;
}

std::size_t PluginManager::load_plugins_from_directory(const std::string& plugin_directory) {
    if (!initialized_) {
        LOG_ERROR("Plugin manager not initialized");
        return 0;
    }
    
    LOG_INFO("Loading plugins from directory: {}", plugin_directory);
    
    std::size_t loaded_count = 0;
    
    try {
        if (!std::filesystem::exists(plugin_directory)) {
            LOG_WARN("Plugin directory does not exist: {}", plugin_directory);
            return 0;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(plugin_directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".dll") {
                const auto plugin_path = entry.path().string();
                LOG_DEBUG("Found plugin DLL: {}", plugin_path);
                
                if (load_plugin(plugin_path) == PluginResult::Success) {
                    ++loaded_count;
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        LOG_ERROR("Filesystem error while loading plugins: {}", e.what());
    }
    
    LOG_INFO("Loaded {} plugin(s) from directory", loaded_count);
    return loaded_count;
}

PluginResult PluginManager::initialize_plugins(const std::string& config_path) {
    LOG_INFO("Initializing {} loaded plugin(s)", plugins_.size());
    
    for (auto& [name, instance] : plugins_) {
        if (instance->initialized) {
            continue;
        }
        
        LOG_DEBUG("Initializing plugin: {}", name);
        
        // Initialize plugin
        auto result = instance->plugin->initialize(host_.get());
        if (result != PluginResult::Success) {
            LOG_ERROR("Failed to initialize plugin '{}': {}", name, to_string(result));
            continue;
        }
        
        // Load configurations
        result = instance->plugin->load_configurations(config_path);
        if (result != PluginResult::Success) {
            LOG_ERROR("Failed to load configurations for plugin '{}': {}", name, to_string(result));
            // Continue anyway - some plugins might not need configurations
        }
        
        instance->initialized = true;
        LOG_INFO("Successfully initialized plugin: {}", name);
    }
    
    const auto initialized_count = std::count_if(plugins_.begin(), plugins_.end(),
        [](const auto& pair) { return pair.second->initialized; });
    
    LOG_INFO("Initialized {}/{} plugin(s)", initialized_count, plugins_.size());
    return PluginResult::Success;
}

PluginResult PluginManager::unload_plugin(const std::string& plugin_name) {
    auto it = plugins_.find(plugin_name);
    if (it == plugins_.end()) {
        LOG_WARN("Plugin '{}' not found", plugin_name);
        return PluginResult::NotFound;
    }
    
    LOG_INFO("Unloading plugin: {}", plugin_name);
    
    auto& instance = it->second;
    if (instance->initialized) {
        instance->plugin->shutdown();
    }
    
    FreeLibrary(instance->module_handle);
    plugins_.erase(it);
    
    LOG_INFO("Successfully unloaded plugin: {}", plugin_name);
    return PluginResult::Success;
}

void PluginManager::unload_all_plugins() {
    LOG_INFO("Unloading all {} plugin(s)", plugins_.size());
    
    for (auto& [name, instance] : plugins_) {
        LOG_DEBUG("Shutting down plugin: {}", name);
        if (instance->initialized) {
            instance->plugin->shutdown();
        }
        FreeLibrary(instance->module_handle);
    }
    
    plugins_.clear();
    LOG_INFO("All plugins unloaded");
}

std::vector<std::string> PluginManager::get_loaded_plugin_names() const {
    std::vector<std::string> names;
    names.reserve(plugins_.size());
    
    for (const auto& [name, _] : plugins_) {
        names.push_back(name);
    }
    
    return names;
}

std::optional<PluginInfo> PluginManager::get_plugin_info(const std::string& plugin_name) const {
    auto it = plugins_.find(plugin_name);
    if (it != plugins_.end()) {
        return it->second->info;
    }
    return std::nullopt;
}

bool PluginManager::is_plugin_loaded(const std::string& plugin_name) const {
    return plugins_.find(plugin_name) != plugins_.end();
}

std::size_t PluginManager::plugin_count() const {
    return plugins_.size();
}

std::unique_ptr<PluginInstance> PluginManager::load_plugin_dll(const std::string& dll_path) {
    // Load the DLL
    HMODULE module = LoadLibraryA(dll_path.c_str());
    if (!module) {
        LOG_ERROR("Failed to load plugin DLL '{}', error: {}", dll_path, GetLastError());
        return nullptr;
    }
    
    // Get the CreatePlugin function
    auto create_func = reinterpret_cast<CreatePluginFunc>(
        GetProcAddress(module, PLUGIN_CREATE_FUNCTION_NAME));
    
    if (!create_func) {
        LOG_ERROR("Plugin '{}' does not export {}", dll_path, PLUGIN_CREATE_FUNCTION_NAME);
        FreeLibrary(module);
        return nullptr;
    }
    
    // Create plugin instance
    auto plugin = create_func();
    if (!plugin) {
        LOG_ERROR("Failed to create plugin instance from '{}'", dll_path);
        FreeLibrary(module);
        return nullptr;
    }
    
    // Get plugin info
    auto info = plugin->get_plugin_info();
    
    LOG_DEBUG("Created plugin instance: {} v{}", info.name, info.version);
    
    return std::make_unique<PluginInstance>(module, std::move(plugin), std::move(info), dll_path);
}

bool PluginManager::validate_plugin_version(const IPlugin* plugin) const {
    const auto info = plugin->get_plugin_info();
    return info.api_version == PLUGIN_API_VERSION;
}

} // namespace app_hook::plugin 