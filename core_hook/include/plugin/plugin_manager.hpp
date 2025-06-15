#pragma once

#include "plugin_interface.hpp"
#include "config/config_base.hpp"
#include "config/config_loader_base.hpp"
#include <Windows.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <optional>

namespace app_hook::plugin {

/// @brief Plugin instance wrapper
struct PluginInstance {
    HMODULE module_handle;              ///< DLL module handle
    std::unique_ptr<IPlugin> plugin;    ///< Plugin interface
    PluginInfo info;                    ///< Plugin information
    std::string file_path;              ///< Path to plugin DLL
    bool initialized;                   ///< Whether plugin is initialized
    
    PluginInstance(HMODULE handle, std::unique_ptr<IPlugin> p, PluginInfo i, std::string path)
        : module_handle(handle), plugin(std::move(p)), info(std::move(i)), 
          file_path(std::move(path)), initialized(false) {}
};

/// @brief Plugin host implementation
class PluginHost : public IPluginHost {
public:
    PluginHost();
    ~PluginHost() override = default;

    /// @brief Set configuration registry callback
    /// @param callback Function to call when configurations are registered
    void set_config_registry(std::function<void(std::unique_ptr<::app_hook::config::ConfigBase>)> callback);

    /// @brief Set base data path for plugins
    /// @param path Base path for plugin data
    void set_data_path(const std::string& path);

    // IPluginHost interface
    PluginResult register_config(std::unique_ptr<::app_hook::config::ConfigBase> config) override;
    PluginResult register_config_loader(std::unique_ptr<::app_hook::config::ConfigLoaderBase> loader) override;
    PluginResult register_task_creator(const std::string& config_type_name, 
                                        std::function<std::unique_ptr<::app_hook::task::IHookTask>(const ::app_hook::config::ConfigBase&)> creator) override;
    std::string get_plugin_data_path() const override;
    void log_message(int level, const std::string& message) override;
    void log_message_with_location(int level, const std::string& message, const std::source_location& location) override;
    std::pair<void*, std::uint32_t> get_process_info() const override;
    ::app_hook::context::ModContext& get_mod_context() override;

private:
    std::function<void(std::unique_ptr<::app_hook::config::ConfigBase>)> config_registry_;
    std::string data_path_;
};

/// @brief Plugin manager for loading and managing plugins
class PluginManager {
public:
    PluginManager();
    ~PluginManager();

    // Non-copyable, non-movable
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;
    PluginManager(PluginManager&&) = delete;
    PluginManager& operator=(PluginManager&&) = delete;

    /// @brief Initialize the plugin manager
    /// @param data_path Base path for plugin data
    /// @param config_registry Callback for registering configurations
    /// @return Success if initialized
    [[nodiscard]] PluginResult initialize(
        const std::string& data_path,
        std::function<void(std::unique_ptr<::app_hook::config::ConfigBase>)> config_registry
    );

    /// @brief Load a plugin from a DLL file
    /// @param plugin_path Path to the plugin DLL
    /// @return Success if loaded
    [[nodiscard]] PluginResult load_plugin(const std::string& plugin_path);

    /// @brief Load all plugins from a directory
    /// @param plugin_directory Directory containing plugin DLLs
    /// @return Number of plugins loaded
    [[nodiscard]] std::size_t load_plugins_from_directory(const std::string& plugin_directory);

    /// @brief Initialize all loaded plugins
    /// @param config_path Base configuration path
    /// @return Success if all plugins initialized
    [[nodiscard]] PluginResult initialize_plugins(const std::string& config_path);

    /// @brief Unload a specific plugin
    /// @param plugin_name Name of the plugin to unload
    /// @return Success if unloaded
    [[nodiscard]] PluginResult unload_plugin(const std::string& plugin_name);

    /// @brief Unload all plugins
    void unload_all_plugins();

    /// @brief Get list of loaded plugin names
    /// @return Vector of plugin names
    [[nodiscard]] std::vector<std::string> get_loaded_plugin_names() const;

    /// @brief Get plugin information by name
    /// @param plugin_name Name of the plugin
    /// @return Plugin info or nullopt if not found
    [[nodiscard]] std::optional<PluginInfo> get_plugin_info(const std::string& plugin_name) const;

    /// @brief Check if a plugin is loaded
    /// @param plugin_name Name of the plugin
    /// @return True if loaded
    [[nodiscard]] bool is_plugin_loaded(const std::string& plugin_name) const;

    /// @brief Get total number of loaded plugins
    /// @return Number of loaded plugins
    [[nodiscard]] std::size_t plugin_count() const;

private:
    /// @brief Load a single plugin DLL
    /// @param dll_path Path to the DLL
    /// @return Plugin instance or nullptr if failed
    [[nodiscard]] std::unique_ptr<PluginInstance> load_plugin_dll(const std::string& dll_path);

    /// @brief Validate plugin API version
    /// @param plugin Plugin to validate
    /// @return True if compatible
    [[nodiscard]] bool validate_plugin_version(const IPlugin* plugin) const;

    std::unordered_map<std::string, std::unique_ptr<PluginInstance>> plugins_;
    std::unique_ptr<PluginHost> host_;
    bool initialized_;
};

} // namespace app_hook::plugin 