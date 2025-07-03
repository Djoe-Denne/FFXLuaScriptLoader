# FFScriptLoader Usage Guide

## Overview

This guide explains how to use the FFScriptLoader plugin injection system to modify legacy x32 applications. The system consists of an injector executable and a plugin-based DLL that extends target applications with new functionality.

## Prerequisites

### System Requirements
- Windows 10/11 with 32-bit compatibility
- Target application must be 32-bit
- Visual C++ Redistributable (if using pre-built binaries)

### File Structure
Ensure your deployment has the following structure:
```
deployment/
├── app_injector.exe         # Main injector executable
├── app_hook.dll            # Plugin host DLL
├── config/                 # Configuration files
│   ├── tasks.toml         # Main task configuration
│   └── tasks/             # Individual task configs
│       ├── memory_config.toml
│       └── patch_config.toml
└── tasks/                  # Plugin directory
    └── memory_plugin.dll   # Available plugins
```

## Basic Usage

### Simple Injection
Inject into a target application using default settings:

```bash
app_injector.exe myapp.exe
```

This will:
- Wait for `myapp.exe` to start (if not already running)
- Inject `app_hook.dll` into the target process
- Load plugins from the `tasks/` directory
- Use configuration files from the `config/` directory

### Custom DLL Injection
Inject a custom hook DLL:

```bash
app_injector.exe myapp.exe 
```

## Advanced Usage

### Custom Directory Paths

Specify custom paths for configuration and plugins:

```bash
# Custom configuration directory
app_injector.exe myapp.exe --config-dir my_config

# Custom plugin directory  
app_injector.exe myapp.exe --plugin-dir my_plugins

# Both custom directories
app_injector.exe myapp.exe --config-dir my_config --plugin-dir my_plugins
```

### Process Discovery Options

The injector supports flexible process targeting:

```bash
# Case-insensitive process name
app_injector.exe MYAPP.EXE    # Works with myapp.exe

# With or without .exe extension
app_injector.exe myapp        # Matches myapp.exe
```

## Configuration System

### Main Task Configuration (`tasks.toml`)

The main configuration file defines available tasks:

```toml
[metadata]
version = "1.0.0"
description = "Main configuration file"

# Memory expansion task
[tasks.memory_expansion]
name = "Memory Expansion"
description = "Expand memory regions for modding"
type = "memory"
config_file = "config/tasks/memory_config.toml"
followBy = ["patches"]
enabled = true

# Patch application task
[tasks.patches]  
name = "Instruction Patches"
description = "Apply memory redirection patches"
type = "patch"
config_file = "config/tasks/patch_config.toml"
enabled = true
```

**Configuration Properties**:
- `name`: Human-readable task name
- `description`: Task description
- `type`: Task type (must match plugin capabilities)
- `config_file`: Path to detailed task configuration
- `followBy`: Tasks that should run after this one
- `enabled`: Whether the task should be executed

### Memory Configuration (`memory_config.toml`)

Defines memory regions to expand:

```toml
[memory.DATA_STRUCTURE]
address = "0x00501234"       # Original memory address
originalSize = 1024          # Current size in bytes  
newSize = 2048              # Target expanded size
copyAfter = "0x00401000"    # Trigger address for copy operation
description = "Main data structure"

[memory.SPELL_DATA]
address = "0x00502000"
originalSize = 512
newSize = 1024
copyAfter = "0x00401100"
description = "Spell configuration data"
```

### Patch Configuration (`patch_config.toml`)

Defines instruction patches for memory redirection:

```toml
[metadata]
script_version = "1.0.0"
memory_base = "0x00501234"   # Base address for calculations
image_base = "0x00400000"    # Application image base
total_instructions = 25
description = "Memory redirection patches"

# Individual instruction patches
[instructions.0x00401500]
bytes = "8B 15 XX XX XX XX"  # Original instruction pattern
offset = "0x10"              # Offset in expanded memory

[instructions.0x00401520]
bytes = "A1 XX XX XX XX"
offset = "0x14"
```

## Plugin Management

### Plugin Discovery

The system automatically discovers and loads plugins from the plugin directory. Plugins must:
- Be compiled as 32-bit DLLs
- Export `CreatePlugin` and `DestroyPlugin` functions
- Implement the `IPlugin` interface

### Plugin Configuration

Plugins register their configuration capabilities during initialization. The system matches task types from `tasks.toml` with available plugin loaders.

### Plugin Logging

All plugin activity is logged to `logs/app_hook.log`:
- Plugin loading and initialization
- Configuration parsing
- Task execution status
- Error details and stack traces

## Execution Flow

### 1. Injector Phase
1. Parse command line arguments
2. Validate target process architecture (must be 32-bit)
3. Wait for target process if not running
4. Write configuration paths to temporary file
5. Inject hook DLL using `LoadLibraryA` technique
6. Exit after successful injection

### 2. Hook DLL Phase (in target process)
1. Initialize logging system
2. Read injector configuration from temporary file
3. Initialize plugin manager with data paths
4. Load plugins from plugin directory
5. Initialize plugins with configuration paths
6. Parse main task configuration
7. Create and execute tasks in order
8. Install memory hooks and patches

### 3. Runtime Phase
- Installed hooks execute when target addresses are accessed
- Memory operations are redirected to expanded regions
- Plugins continue to operate within the target process
- All activity is logged for monitoring

## Troubleshooting

### Common Issues

#### Injection Fails
**Symptoms**: "Failed to inject DLL" error  
**Causes**:
- Architecture mismatch (64-bit injector with 32-bit target)
- Missing dependencies (Visual C++ Redistributable)
- Insufficient privileges
- DLL initialization errors

**Solutions**:
```bash
# Check if DLL loads in isolation
LoadLibraryA("app_hook.dll")  # Test in separate process

# Run with administrator privileges
# Ensure all components are 32-bit
# Install Visual C++ Redistributable
```

#### Plugin Loading Fails
**Symptoms**: "Failed to load plugins" in log  
**Causes**:
- Plugin directory doesn't exist
- Plugins have missing dependencies
- Architecture mismatch in plugins

**Solutions**:
- Verify plugin directory path
- Check plugin dependencies
- Ensure all plugins are 32-bit

#### Configuration Errors
**Symptoms**: "Failed to parse configuration" in log  
**Causes**:
- Invalid TOML syntax
- Missing configuration files
- Invalid memory addresses

**Solutions**:
- Validate TOML syntax
- Check file paths in `tasks.toml`
- Verify memory addresses are correct for target application

### Logging and Diagnostics

#### Log Levels
The system supports multiple log levels:
- **TRACE**: Detailed execution flow
- **DEBUG**: Development information
- **INFO**: General status information
- **WARN**: Non-fatal issues
- **ERROR**: Operation failures
- **CRITICAL**: System-level failures

#### Log Locations
- **Main Log**: `logs/app_hook.log`
- **Injector Output**: Console output
- **System Events**: Windows Event Log (for critical errors)

#### Debug Mode
Enable verbose logging for troubleshooting:
```cpp
// Modify log level in dllmain.cpp
initialize_logging("logs/app_hook.log", 0);  // 0 = TRACE level
```

## Performance Considerations

### Memory Usage
- Each expanded memory region consumes additional RAM
- Plugin DLLs remain loaded in target process memory
- Consider memory constraints of target application

### Hook Performance
- MinHook provides efficient trampolines
- Multiple hooks may impact performance
- Consider hook placement and frequency

### Startup Time
- Plugin loading happens during DLL injection
- Large configuration files increase initialization time
- Consider lazy loading for complex scenarios

## Security Considerations

### Privileges
- Injection requires matching or higher privileges than target
- Consider running as administrator for system applications
- Be aware of UAC implications

### Target Application Stability
- Always test with target application before deployment
- Invalid memory addresses can cause crashes
- Use error handling to prevent cascading failures

### Anti-Virus Compatibility
- DLL injection may trigger anti-virus software
- Consider adding exceptions for deployment directory
- Code signing can improve compatibility

## Best Practices

### Development
1. **Test Incrementally**: Start with simple configurations
2. **Use Logging**: Enable debug logging during development
3. **Validate Addresses**: Verify memory addresses before deployment
4. **Handle Errors**: Implement comprehensive error handling

### Deployment  
1. **Document Configurations**: Maintain clear documentation
2. **Version Control**: Track configuration changes
3. **Backup Originals**: Keep backups of original applications
4. **Test Environment**: Use isolated test environment

### Maintenance
1. **Monitor Logs**: Regularly check log files for issues
2. **Update Configurations**: Keep configurations current with application updates
3. **Plugin Updates**: Update plugins when APIs change
4. **Performance Monitoring**: Monitor target application performance

## Example Scenarios

### Game Modding
```bash
# Expand game data structures for custom content
app_injector.exe game.exe --config-dir game_mods --plugin-dir mod_plugins
```

### Legacy Business Application
```bash
# Add modern functionality to legacy software
app_injector.exe legacy_app.exe --config-dir business_config --plugin-dir business_plugins
```

### Educational/Research
```bash
# Study application behavior with monitoring plugins
app_injector.exe research_target.exe --config-dir research_config --plugin-dir monitoring_plugins
``` 