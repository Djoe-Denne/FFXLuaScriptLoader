#include "plugin/plugin_interface.hpp"
#include "../include/config/memory_config_loader.hpp"
#include "../include/config/patch_config_loader.hpp"
#include "../include/memory/copy_memory.hpp"
#include "../include/memory/patch_memory.hpp"
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
        
        // Register memory config loader
        auto memory_loader = std::make_unique<MemoryConfigLoader>();
        auto memory_result = host_->register_config_loader(std::move(memory_loader));
        if (memory_result != app_hook::plugin::PluginResult::Success) {
            PLUGIN_LOG_ERROR("Memory Plugin: Failed to register memory config loader");
            return memory_result;
        }
        PLUGIN_LOG_INFO("Memory Plugin: Memory config loader registered successfully");
        
        // Register patch config loader
        auto patch_loader = std::make_unique<PatchConfigLoader>();
        auto patch_result = host_->register_config_loader(std::move(patch_loader));
        if (patch_result != app_hook::plugin::PluginResult::Success) {
            PLUGIN_LOG_ERROR("Memory Plugin: Failed to register patch config loader");
            return patch_result;
        }
        PLUGIN_LOG_INFO("Memory Plugin: Patch config loader registered successfully");
        
        // Register task creators through host interface
        PLUGIN_LOG_INFO("Memory Plugin: Registering task creators...");
        
        // Register CopyMemoryTask creator
        auto memory_creator_result = host_->register_task_creator(
            "app_hook::config::CopyMemoryConfig",
            [](const app_hook::config::ConfigBase& base_config) -> std::unique_ptr<app_hook::task::IHookTask> {
                if (const auto* memory_config = dynamic_cast<const app_hook::config::CopyMemoryConfig*>(&base_config)) {
                    return app_hook::task::make_task<app_hook::memory::CopyMemoryTask>(*memory_config);
                }
                return nullptr;
            }
        );
        
        if (memory_creator_result != app_hook::plugin::PluginResult::Success) {
            PLUGIN_LOG_ERROR("Memory Plugin: Failed to register CopyMemoryTask creator");
            return memory_creator_result;
        }
        PLUGIN_LOG_INFO("Memory Plugin: CopyMemoryTask creator registered successfully");
        
        // Register PatchMemoryTask creator
        auto patch_creator_result = host_->register_task_creator(
            "app_hook::config::PatchConfig",
            [](const app_hook::config::ConfigBase& base_config) -> std::unique_ptr<app_hook::task::IHookTask> {
                if (const auto* patch_config = dynamic_cast<const app_hook::config::PatchConfig*>(&base_config)) {
                    // Extract instructions from the patch config
                    const auto& instructions = patch_config->instructions();
                    return app_hook::task::make_task<app_hook::memory::PatchMemoryTask>(*patch_config, instructions);
                }
                return nullptr;
            }
        );
        
        if (patch_creator_result != app_hook::plugin::PluginResult::Success) {
            PLUGIN_LOG_ERROR("Memory Plugin: Failed to register PatchMemoryTask creator");
            return patch_creator_result;
        }
        PLUGIN_LOG_INFO("Memory Plugin: PatchMemoryTask creator registered successfully");
        
        PLUGIN_LOG_INFO("Memory Plugin: Initialized successfully");
        return app_hook::plugin::PluginResult::Success;
    }

    app_hook::plugin::PluginResult load_configurations(const std::string& config_path) override {
        if (!host_) {
            return app_hook::plugin::PluginResult::Failed;
        }

        PLUGIN_LOG_INFO("Memory Plugin: Configuration loaders registered with factory");
        PLUGIN_LOG_INFO("Memory Plugin: Configuration loading will be handled by the factory");
        PLUGIN_LOG_DEBUG("Memory Plugin: Config path provided: {}", config_path);
        
        // The config loaders are now registered with the factory and will be used
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