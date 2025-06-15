# FFScript Loader Injector Usage

The FFScript Loader injector now supports configurable paths for configuration files and plugin directories, making it more flexible for different deployment scenarios.

## Basic Usage

### Default Configuration
```bash
# Uses default paths:
# - Config directory: config
# - Plugin directory: mods/xtender/tasks
injector.exe myapp.exe
```

### Custom DLL (backward compatible)
```bash
injector.exe myapp.exe custom_hook.dll
```

## Advanced Usage with Custom Paths

### Custom Configuration Directory
```bash
injector.exe myapp.exe --config-dir custom_config
```

### Custom Plugin Directory  
```bash
injector.exe myapp.exe --plugin-dir custom_plugins
```

### Custom Both Directories
```bash
injector.exe myapp.exe --config-dir my_config --plugin-dir my_plugins
```

### Custom DLL with Custom Directories
```bash
injector.exe myapp.exe my_hook.dll --config-dir my_config --plugin-dir my_plugins
```

## Configuration Flow

1. **Injector** parses command line arguments for config and plugin directories
2. **Injector** writes these paths to a temporary configuration file in `%TEMP%/ffscript_loader/injector_config.txt`
3. **Injector** injects the DLL into the target process
4. **DLL** reads the configuration file during initialization
5. **DLL** uses the specified paths or falls back to defaults if no config file is found

## Directory Structure Examples

### Default Setup
```
myapp.exe
injector.exe
app_hook.dll
config/
  ├── tasks.toml
  └── [other config files]
mods/
  └── xtender/
    └── tasks/
      ├── plugin1.dll
      └── plugin2.dll
```

### Custom Setup
```bash
# Command: injector.exe myapp.exe --config-dir settings --plugin-dir extensions
```
```
myapp.exe
injector.exe
app_hook.dll
settings/
  ├── tasks.toml
  └── [other config files]
extensions/
  ├── plugin1.dll
  └── plugin2.dll
```

## Logging

The DLL will log which configuration source it's using:
- `Configuration loaded from injector:` - When custom paths are provided
- `Using default configuration (no injector config found):` - When using defaults

Check `logs/app_hook.log` for detailed information about the configuration being used.

## Error Handling

- If the injector fails to write the configuration file, it will exit with an error
- If the DLL cannot read the configuration file, it falls back to default paths
- All configuration-related errors are logged to the DLL log file

## Compatibility

This change is fully backward compatible:
- Existing usage without parameters continues to work with default paths
- The DLL will fall back to hardcoded defaults if no injector config is found
- Legacy scripts and deployment processes remain unaffected 