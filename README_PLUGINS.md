# Plugin Architecture

This document describes the hot-loadable plugin system for the FFScriptLoader project.

## Overview

The application has been restructured to support hot-loaded plugins that can be injected after the main `app_hook.dll` is loaded. This allows for modular functionality and easier maintenance.

## Architecture

### Core Components

1. **app_hook.dll** - Main hook library that exposes the plugin interface
2. **memory_plugin.dll** - Plugin containing memory copy and patch functionality
3. **injector.exe** - Updated to inject plugins after the main DLL

### Plugin Interface

The plugin system uses the `IPlugin` interface defined in `app_hook/include/plugin/plugin_interface.hpp`:

```cpp
class IPlugin {
public:
    virtual PluginInfo get_plugin_info() const = 0;
    virtual PluginResult initialize(IPluginHost* host) = 0;
    virtual PluginResult load_configurations(const std::string& config_path) = 0;
    virtual void shutdown() = 0;
};
```

### Plugin Host Interface

The main app_hook exposes services to plugins through `IPluginHost`:

```cpp
class IPluginHost {
public:
    virtual PluginResult register_config(std::unique_ptr<config::ConfigBase> config) = 0;
    virtual std::string get_plugin_data_path() const = 0;
    virtual void log_message(int level, const std::string& message) = 0;
    virtual std::pair<void*, std::uint32_t> get_process_info() const = 0;
};
```

## Directory Structure

```
FFScriptLoader/
├── app_hook/                  # Main hook library
│   ├── include/plugin/        # Plugin interface definitions
│   └── src/plugin/           # Plugin manager implementation
├── memory_plugin/            # Memory operations plugin
│   ├── include/              # Plugin headers
│   ├── src/                  # Plugin implementation
│   └── CMakeLists.txt        # Plugin build configuration
├── injector/                 # Updated injector
└── plugins/                  # Runtime plugin directory
    └── memory_plugin.dll     # Built plugins go here
```

## Build Process

1. **Main Build**: Builds `app_hook.dll` with plugin support
2. **Plugin Build**: Builds `memory_plugin.dll` as a separate plugin
3. **Output**: Plugins are built to `bin/plugins/` directory

## Runtime Flow

1. **Injection**: `injector.exe` injects `app_hook.dll` into target process
2. **Plugin Loading**: `app_hook.dll` automatically loads plugins from `plugins/` directory
3. **Configuration**: Plugins load their configurations and register them with the host
4. **Operation**: The hook system uses plugin-provided configurations

## Memory Plugin

The memory plugin (`memory_plugin.dll`) provides:

- Memory copy configuration loading from TOML files
- Patch instruction loading from TOML files
- Registration of configurations with the main hook system

### Configuration Layout

```
config/
├── memory/           # Memory copy configurations
│   └── *.toml
└── patches/          # Patch configurations
    └── *.toml
```

## Usage

### Building

```bash
cmake -B build -A Win32 -DBUILD_TESTING=ON
cmake --build build --config Release
```

### Running

```bash
# Inject into target process
./bin/app_injector.exe target_process.exe

# The injector will:
# 1. Inject app_hook.dll
# 2. Wait for initialization
# 3. Inject all plugins from plugins/ directory
```

## Plugin Development

To create a new plugin:

1. **Create Plugin Class**: Implement `IPlugin` interface
2. **Export Functions**: Use `EXPORT_PLUGIN(YourPluginClass)` macro
3. **Build Configuration**: Create CMakeLists.txt that outputs to `bin/plugins/`
4. **Runtime**: Place built plugin DLL in `plugins/` directory

### Example Plugin

```cpp
#include "plugin/plugin_interface.hpp"

class MyPlugin : public app_hook::plugin::IPlugin {
public:
    PluginInfo get_plugin_info() const override {
        return {"My Plugin", "1.0.0", "Description", PLUGIN_API_VERSION};
    }
    
    PluginResult initialize(IPluginHost* host) override {
        // Initialize plugin
        return PluginResult::Success;
    }
    
    PluginResult load_configurations(const std::string& config_path) override {
        // Load and register configurations
        return PluginResult::Success;
    }
    
    void shutdown() override {
        // Cleanup
    }
};

EXPORT_PLUGIN(MyPlugin)
```

## Benefits

- **Modularity**: Separate concerns into focused plugins
- **Hot Loading**: Plugins can be updated without rebuilding main DLL
- **Maintainability**: Easier to maintain and debug individual components
- **Extensibility**: Easy to add new functionality through plugins

## Migration Notes

The following components have been moved from `app_hook.dll` to `memory_plugin.dll`:

- `CopyMemoryLoader`
- `PatchConfigLoader` 
- `CopyMemoryConfig`
- `PatchConfig`
- Memory operation implementations

The main `app_hook.dll` now focuses on:

- Plugin management
- Hook installation
- Configuration registry
- Task execution 