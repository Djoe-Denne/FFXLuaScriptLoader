#include "plugin/plugin_interface.hpp"
#include "memory_config_loader.hpp"
#include <filesystem>
#include <string>
#include <format>

namespace memory_plugin {

/// @brief Memory operations plugin implementation
class MemoryPlugin : public app_hook::plugin::IPlugin {
public:
    MemoryPlugin() : host_(nullptr) {}
    ~MemoryPlugin() override = default;

    // IPlugin interface
    app_hook::plugin::PluginInfo get_plugin_info() const override {
        return {
            "Memory Operations Plugin",
            "1.0.0", 
            "Provides memory copying and patching functionality",
            app_hook::plugin::PLUGIN_API_VERSION
        };
    }

    app_hook::plugin::PluginResult initialize(app_hook::plugin::IPluginHost* host) override {
        if (!host) {
            return app_hook::plugin::PluginResult::Failed;
        }

        host_ = host;
        PLUGIN_LOG_INFO("Memory Plugin: Initializing...");
        
        // Register our config loader with the factory  
        auto config_loader = std::make_unique<MemoryConfigLoader>();
        auto result = host_->register_config_loader(std::move(config_loader));
        if (result != app_hook::plugin::PluginResult::Success) {
            PLUGIN_LOG_ERROR("Memory Plugin: Failed to register config loader");
            return result;
        }
        
        PLUGIN_LOG_INFO("Memory Plugin: Config loader registered successfully");
        PLUGIN_LOG_INFO("Memory Plugin: Initialized successfully");
        return app_hook::plugin::PluginResult::Success;
    }

    app_hook::plugin::PluginResult load_configurations(const std::string& config_path) override {
        if (!host_) {
            return app_hook::plugin::PluginResult::Failed;
        }

        PLUGIN_LOG_INFO("Memory Plugin: Configuration loader registered with factory");
        PLUGIN_LOG_INFO("Memory Plugin: Configuration loading will be handled by the factory");
        
        // The config loader is now registered with the factory and will be used
        // automatically when configurations are requested
        return app_hook::plugin::PluginResult::Success;
    }

    void shutdown() override {
        if (host_) {
            PLUGIN_LOG_INFO("Memory Plugin: Shutting down...");
            host_ = nullptr;
        }
    }

private:
    app_hook::plugin::IPluginHost* host_;
};

} // namespace memory_plugin

// Export plugin functions
extern "C" {
    __declspec(dllexport) app_hook::plugin::IPlugin* CreatePlugin() {
        return new memory_plugin::MemoryPlugin();
    }
    
    __declspec(dllexport) void DestroyPlugin(app_hook::plugin::IPlugin* plugin) {
        delete plugin;
    }
} 