# Plugin Development Guide

## Overview

This guide explains how to create custom plugins for the FFScriptLoader injection framework. Plugins extend the system's capabilities by providing custom configuration loaders and task implementations.

## Plugin Architecture

### Core Interfaces

Every plugin must implement the `IPlugin` interface:

```cpp
class IPlugin {
public:
    virtual PluginInfo get_plugin_info() const = 0;
    virtual PluginResult initialize(IPluginHost* host) = 0;
    virtual PluginResult load_configurations(const std::string& config_path) = 0;
    virtual void shutdown() = 0;
};
```

### Plugin Host Services

The `IPluginHost` interface provides services to plugins:

- **Configuration Registration**: Register config loaders and configurations
- **Task Registration**: Register task creators for specific configuration types
- **Logging**: Access to the host's logging system
- **Process Information**: Get target process details
- **Data Paths**: Access to plugin-specific data directories

## Creating a Basic Plugin

### 1. Plugin Class Structure

```cpp
#include "plugin/plugin_interface.hpp"

class MyPlugin : public app_hook::plugin::IPlugin {
public:
    MyPlugin() : host_(nullptr) {}
    ~MyPlugin() override = default;

    // Plugin information
    app_hook::plugin::PluginInfo get_plugin_info() const override {
        return {
            "My Custom Plugin",           // name
            "1.0.0",                     // version
            "Description of functionality", // description
            app_hook::plugin::PLUGIN_API_VERSION // required API version
        };
    }

    // Initialize plugin with host
    app_hook::plugin::PluginResult initialize(app_hook::plugin::IPluginHost* host) override;

    // Load configurations
    app_hook::plugin::PluginResult load_configurations(const std::string& config_path) override;

    // Cleanup
    void shutdown() override;

private:
    app_hook::plugin::IPluginHost* host_;
};
```

### 2. Plugin Export Functions

Every plugin DLL must export these functions:

```cpp
extern "C" {
    __declspec(dllexport) app_hook::plugin::IPlugin* CreatePlugin() {
        return new MyPlugin();
    }
    
    __declspec(dllexport) void DestroyPlugin(app_hook::plugin::IPlugin* plugin) {
        delete plugin;
    }
}
```

Or use the convenience macro:

```cpp
EXPORT_PLUGIN(MyPlugin)
```

## Configuration Loaders

Configuration loaders parse TOML files into typed configuration objects.

### Creating a Configuration Type

```cpp
class MyTaskConfig : public app_hook::config::ConfigBase {
public:
    MyTaskConfig(const std::string& key, const std::string& name) 
        : ConfigBase(key, name) {}
    
    // Configuration-specific properties
    std::string target_address() const { return target_address_; }
    void set_target_address(const std::string& addr) { target_address_ = addr; }

private:
    std::string target_address_;
};
```

### Creating a Configuration Loader

```cpp
class MyConfigLoader : public app_hook::config::ConfigLoaderBase {
public:
    bool can_load(const std::string& type) const override {
        return type == "my_task_type";
    }

    std::unique_ptr<app_hook::config::ConfigBase> load_config(
        const std::string& key,
        const toml::table& table) const override {
        
        auto config = std::make_unique<MyTaskConfig>(key, "My Task");
        
        // Parse TOML configuration
        if (auto addr = table["address"].value<std::string>()) {
            config->set_target_address(*addr);
        }
        
        return config;
    }
};
```

## Task Implementation

Tasks perform the actual work defined by configurations.

### Creating a Task

```cpp
class MyTask : public app_hook::task::IHookTask {
public:
    MyTask(const MyTaskConfig& config) : config_(config) {}

    bool execute() override {
        // Implement task logic here
        try {
            // Example: Hook a function at the specified address
            void* target = reinterpret_cast<void*>(
                std::stoull(config_.target_address(), nullptr, 16)
            );
            
            // Use MinHook or other hooking mechanisms
            // return install_hook(target, my_hook_function);
            
            return true;
        }
        catch (const std::exception& e) {
            // Log error through host if available
            return false;
        }
    }

    std::string get_name() const override {
        return "My Custom Task";
    }

private:
    const MyTaskConfig& config_;
};
```

## Plugin Registration

### Initialize Function Implementation

```cpp
app_hook::plugin::PluginResult MyPlugin::initialize(app_hook::plugin::IPluginHost* host) {
    if (!host) {
        return app_hook::plugin::PluginResult::Failed;
    }

    host_ = host;
    PLUGIN_LOG_INFO("My Plugin: Initializing...");
    
    // Register configuration loader
    auto loader = std::make_unique<MyConfigLoader>();
    auto result = host_->register_config_loader(std::move(loader));
    if (result != app_hook::plugin::PluginResult::Success) {
        PLUGIN_LOG_ERROR("Failed to register config loader");
        return result;
    }
    
    // Register task creator
    auto creator_result = host_->register_task_creator(
        "MyTaskConfig",  // Configuration type name
        [](const app_hook::config::ConfigBase& base_config) -> std::unique_ptr<app_hook::task::IHookTask> {
            if (const auto* my_config = dynamic_cast<const MyTaskConfig*>(&base_config)) {
                return std::make_unique<MyTask>(*my_config);
            }
            return nullptr;
        }
    );
    
    if (creator_result != app_hook::plugin::PluginResult::Success) {
        PLUGIN_LOG_ERROR("Failed to register task creator");
        return creator_result;
    }
    
    PLUGIN_LOG_INFO("My Plugin: Initialized successfully");
    return app_hook::plugin::PluginResult::Success;
}
```

## Configuration Files

Create TOML configuration files that your plugin can parse:

```toml
# config/my_task.toml
[my_task.hook_example]
type = "my_task_type"
address = "0x00401000"
description = "Hook at specific address"
enabled = true
```

Reference your configuration in the main tasks file:

```toml
# config/tasks.toml
[tasks.my_custom_task]
name = "My Custom Task"
description = "Custom functionality"
type = "my_task_type"
config_file = "config/my_task.toml"
enabled = true
```

## Logging

Use the plugin logging macros for consistent logging:

```cpp
PLUGIN_LOG_TRACE("Detailed debug information");
PLUGIN_LOG_DEBUG("Debug information");
PLUGIN_LOG_INFO("General information");
PLUGIN_LOG_WARN("Warning message");
PLUGIN_LOG_ERROR("Error message");
PLUGIN_LOG_CRITICAL("Critical error");
```

## Building Plugins

### CMake Configuration

```cmake
# CMakeLists.txt for plugin
cmake_minimum_required(VERSION 3.25)
project(MyPlugin)

# Set as 32-bit
set(CMAKE_GENERATOR_PLATFORM Win32)

# Create shared library
add_library(my_plugin SHARED
    src/my_plugin.cpp
    src/my_config_loader.cpp
    src/my_task.cpp
)

# Link against core_hook
target_link_libraries(my_plugin 
    PRIVATE core_hook
)

# Include directories
target_include_directories(my_plugin 
    PRIVATE ${CMAKE_SOURCE_DIR}/core_hook/include
)
```

### Build Process

1. Configure for 32-bit architecture
2. Link against the core_hook library
3. Ensure all dependencies are available as 32-bit
4. Build as a shared library (.dll)

## Plugin Deployment

1. **Build**: Compile plugin as 32-bit DLL
2. **Deploy**: Place plugin DLL in the plugin directory (default: `tasks/`)
3. **Configure**: Create TOML configuration files
4. **Reference**: Add task definitions to main `tasks.toml`
5. **Test**: Use the injector to load and test your plugin

## Example: Memory Plugin

The included `memory_plugin` demonstrates a complete plugin implementation:

- **Configuration Loaders**: `MemoryConfigLoader`, `PatchConfigLoader`
- **Tasks**: `CopyMemoryTask`, `PatchMemoryTask`
- **Features**: Memory expansion and instruction patching

Study this implementation as a reference for your own plugins.

## Best Practices

1. **Error Handling**: Always handle exceptions gracefully
2. **Logging**: Use appropriate log levels for different types of messages
3. **Resource Management**: Clean up resources in the shutdown method
4. **Type Safety**: Use proper casting and validation for configuration types
5. **Testing**: Test plugins with target applications before deployment
6. **Documentation**: Document configuration file formats and requirements

## API Versioning

Always check the API version compatibility:

```cpp
if (required_api_version != app_hook::plugin::PLUGIN_API_VERSION) {
    return app_hook::plugin::PluginResult::InvalidVersion;
}
```

The framework will reject plugins with incompatible API versions to prevent crashes and undefined behavior. 