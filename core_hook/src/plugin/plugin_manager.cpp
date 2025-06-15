#include "plugin/plugin_manager.hpp"
#include "config/config_factory.hpp"
#include "task/task_factory.hpp"
#include "context/mod_context.hpp"
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

PluginResult PluginHost::register_task_creator(const std::string& config_type_name, 
                                                std::function<std::unique_ptr<::app_hook::task::IHookTask>(const ::app_hook::config::ConfigBase&)> creator) {
    if (config_type_name.empty() || !creator) {
        LOG_ERROR("Cannot register task creator: invalid config type name or creator function");
        return PluginResult::Failed;
    }
    
    LOG_INFO("Plugin registering task creator for config type: {}", config_type_name);
    
    if (app_hook::task::TaskFactory::instance().register_task_creator(config_type_name, creator)) {
        return PluginResult::Success;
    } else {
        LOG_ERROR("Failed to register task creator with factory");
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

::app_hook::context::ModContext& PluginHost::get_mod_context() {
    return ::app_hook::context::ModContext::instance();
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
    
    try {
        LOG_DEBUG("About to call load_plugin_dll for: {}", plugin_path);
        auto plugin_instance = load_plugin_dll(plugin_path);
        LOG_DEBUG("load_plugin_dll returned for: {}", plugin_path);
        
        if (!plugin_instance) {
            LOG_ERROR("Failed to load plugin DLL: {}", plugin_path);
            return PluginResult::Failed;
        }
        
        LOG_DEBUG("Successfully loaded plugin DLL, getting info...");
        const auto& info = plugin_instance->info;
        LOG_DEBUG("Plugin info - Name: {}, Version: {}, Description: {}", 
                 info.name, info.version, info.description);
        
        // Check if plugin is already loaded
        LOG_DEBUG("Checking if plugin '{}' is already loaded...", info.name);
        if (plugins_.find(info.name) != plugins_.end()) {
            LOG_WARN("Plugin '{}' is already loaded", info.name);
            FreeLibrary(plugin_instance->module_handle);
            return PluginResult::AlreadyLoaded;
        }
        
        // Validate API version
        LOG_DEBUG("Validating API version for plugin '{}'...", info.name);
        if (!validate_plugin_version(plugin_instance->plugin.get())) {
            LOG_ERROR("Plugin '{}' has incompatible API version", info.name);
            FreeLibrary(plugin_instance->module_handle);
            return PluginResult::InvalidVersion;
        }
        
        LOG_INFO("Successfully loaded plugin: {} v{} ({})", 
                 info.name, info.version, info.description);
        
        LOG_DEBUG("Adding plugin '{}' to plugins map...", info.name);
        plugins_[info.name] = std::move(plugin_instance);
        LOG_DEBUG("Plugin '{}' added to plugins map successfully", info.name);
        
        return PluginResult::Success;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception during plugin loading for '{}': {}", plugin_path, e.what());
        return PluginResult::Failed;
    }
    catch (...) {
        LOG_ERROR("Unknown exception during plugin loading for '{}'", plugin_path);
        return PluginResult::Failed;
    }
}

std::size_t PluginManager::load_plugins_from_directory(const std::string& plugin_directory) {
    if (!initialized_) {
        LOG_ERROR("Plugin manager not initialized");
        return 0;
    }
    
    LOG_INFO("Loading plugins from directory: {}", plugin_directory);
    
    std::size_t loaded_count = 0;
    
    try {
        LOG_DEBUG("Checking if plugin directory exists: {}", plugin_directory);
        if (!std::filesystem::exists(plugin_directory)) {
            LOG_WARN("Plugin directory does not exist: {}", plugin_directory);
            return 0;
        }
        
        LOG_DEBUG("Plugin directory exists, starting iteration...");
        
        for (const auto& entry : std::filesystem::directory_iterator(plugin_directory)) {
            LOG_DEBUG("Found directory entry: {}", entry.path().string());
            
            if (entry.is_regular_file() && entry.path().extension() == ".dll") {
                const auto plugin_path = entry.path().string();
                LOG_DEBUG("Found plugin DLL: {}", plugin_path);
                
                LOG_DEBUG("About to call load_plugin for: {}", plugin_path);
                auto result = load_plugin(plugin_path);
                LOG_DEBUG("load_plugin returned: {} for: {}", static_cast<int>(result), plugin_path);
                
                if (result == PluginResult::Success) {
                    ++loaded_count;
                    LOG_DEBUG("Successfully loaded plugin: {} (total loaded: {})", plugin_path, loaded_count);
                } else {
                    LOG_WARN("Failed to load plugin: {} (result: {})", plugin_path, static_cast<int>(result));
                }
            } else {
                LOG_DEBUG("Skipping non-DLL file: {}", entry.path().string());
            }
        }
        
        LOG_DEBUG("Directory iteration completed");
    }
    catch (const std::filesystem::filesystem_error& e) {
        LOG_ERROR("Filesystem error while loading plugins: {}", e.what());
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception while loading plugins from directory: {}", e.what());
    }
    catch (...) {
        LOG_ERROR("Unknown exception while loading plugins from directory");
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
    LOG_DEBUG("load_plugin_dll called with path: {}", dll_path);
    
    try {
        // Check if file exists first
        if (!std::filesystem::exists(dll_path)) {
            LOG_ERROR("Plugin DLL file does not exist: {}", dll_path);
            return nullptr;
        }
        
        LOG_DEBUG("Plugin DLL file exists, attempting to load...");
        
        // Load the DLL
        LOG_DEBUG("Calling LoadLibraryA for: {}", dll_path);
        HMODULE module = LoadLibraryA(dll_path.c_str());
        
        if (!module) {
            DWORD error = GetLastError();
            LOG_ERROR("Failed to load plugin DLL '{}', error: {} (0x{:X})", dll_path, error, error);
            
            // Log common error meanings
            switch (error) {
                case 126: // ERROR_MOD_NOT_FOUND
                    LOG_ERROR("  ERROR_MOD_NOT_FOUND: The specified module or one of its dependencies could not be found");
                    break;
                case 193: // ERROR_BAD_EXE_FORMAT
                    LOG_ERROR("  ERROR_BAD_EXE_FORMAT: The image file is valid, but is for a machine type other than the current machine");
                    break;
                case 1114: // ERROR_DLL_INIT_FAILED
                    LOG_ERROR("  ERROR_DLL_INIT_FAILED: A dynamic link library (DLL) initialization routine failed");
                    break;
                default:
                    LOG_ERROR("  Unknown LoadLibrary error code: {}", error);
                    break;
            }
            
            return nullptr;
        }
        
        LOG_DEBUG("LoadLibraryA succeeded, module handle: 0x{:X}", reinterpret_cast<uintptr_t>(module));
        
        // Get the CreatePlugin function
        LOG_DEBUG("Looking for function: {}", PLUGIN_CREATE_FUNCTION_NAME);
        auto create_func = reinterpret_cast<IPlugin*(*)()>(
            GetProcAddress(module, PLUGIN_CREATE_FUNCTION_NAME));
        
        if (!create_func) {
            DWORD error = GetLastError();
            LOG_ERROR("Plugin '{}' does not export {} (error: {})", dll_path, PLUGIN_CREATE_FUNCTION_NAME, error);
            FreeLibrary(module);
            return nullptr;
        }
        
        LOG_DEBUG("Found {} function at address: 0x{:X}", PLUGIN_CREATE_FUNCTION_NAME, 
                 reinterpret_cast<uintptr_t>(create_func));
        
        // Create plugin instance
        LOG_DEBUG("Calling {} function...", PLUGIN_CREATE_FUNCTION_NAME);
        IPlugin* raw_plugin = create_func();
        LOG_DEBUG("{} function returned", PLUGIN_CREATE_FUNCTION_NAME);
        
        if (!raw_plugin) {
            LOG_ERROR("Failed to create plugin instance from '{}' - {} returned nullptr", dll_path, PLUGIN_CREATE_FUNCTION_NAME);
            FreeLibrary(module);
            return nullptr;
        }
        
        // Wrap raw pointer in unique_ptr
        auto plugin = std::unique_ptr<IPlugin>(raw_plugin);
        LOG_DEBUG("Wrapped raw plugin pointer in unique_ptr");
        
        LOG_DEBUG("Plugin instance created successfully, getting plugin info...");
        
        // Check if plugin pointer is valid
        if (!plugin) {
            LOG_ERROR("Plugin pointer is null after creation");
            FreeLibrary(module);
            return nullptr;
        }
        
        LOG_DEBUG("Plugin pointer is valid: 0x{:X}", reinterpret_cast<uintptr_t>(plugin.get()));
        
        // Get plugin info with additional safety
        LOG_DEBUG("About to call plugin->get_plugin_info()...");
        PluginInfo info;
        
        try {
            info = plugin->get_plugin_info();
            LOG_DEBUG("plugin->get_plugin_info() returned successfully");
        }
        catch (const std::exception& e) {
            LOG_ERROR("Exception calling plugin->get_plugin_info(): {}", e.what());
            FreeLibrary(module);
            return nullptr;
        }
        catch (...) {
            LOG_ERROR("Unknown exception calling plugin->get_plugin_info()");
            FreeLibrary(module);
            return nullptr;
        }
        
        LOG_DEBUG("Got plugin info: name='{}', version='{}', api_version={}", 
                 info.name, info.version, info.api_version);
        
        LOG_DEBUG("Creating PluginInstance object...");
        auto instance = std::make_unique<PluginInstance>(module, std::move(plugin), std::move(info), dll_path);
        
        LOG_DEBUG("PluginInstance created successfully for: {}", dll_path);
        return instance;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception in load_plugin_dll for '{}': {}", dll_path, e.what());
        return nullptr;
    }
    catch (...) {
        LOG_ERROR("Unknown exception in load_plugin_dll for '{}'", dll_path);
        return nullptr;
    }
}

bool PluginManager::validate_plugin_version(const IPlugin* plugin) const {
    LOG_DEBUG("Validating plugin API version...");
    
    try {
        const auto info = plugin->get_plugin_info();
        LOG_DEBUG("Plugin '{}' has API version: {}, expected: {}", 
                 info.name, info.api_version, PLUGIN_API_VERSION);
        
        bool is_valid = (info.api_version == PLUGIN_API_VERSION);
        LOG_DEBUG("API version validation result: {}", is_valid ? "VALID" : "INVALID");
        
        return is_valid;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception during plugin version validation: {}", e.what());
        return false;
    }
    catch (...) {
        LOG_ERROR("Unknown exception during plugin version validation");
        return false;
    }
}

} // namespace app_hook::plugin 