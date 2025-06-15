# FFScriptLoader Plugin Injector Architecture

## Overview

FFScriptLoader is a plugin-based injection framework designed to extend legacy x32 software applications through dynamic memory patching and behavioral modification. The system uses DLL injection to insert a plugin host into target applications, which then loads and manages plugins that can hook into specific instruction addresses.

## Core Components

### 1. Injector (`app_injector.exe`)
- **Purpose**: Injects the hook DLL into target processes
- **Architecture**: 32-bit executable compatible with legacy applications
- **Key Features**:
  - Process discovery and architecture validation
  - DLL injection using `LoadLibraryA` remote thread technique
  - Configuration path management through temporary files
  - Wait-for-process functionality for delayed injection

### 2. Hook DLL (`app_hook.dll`) 
- **Purpose**: Main plugin host injected into target applications
- **Architecture**: 32-bit library that integrates with target process
- **Key Features**:
  - Plugin discovery and loading from specified directories
  - Configuration management through TOML files
  - Task execution framework
  - Logging and error handling

### 3. Plugin System
- **Purpose**: Extensible architecture for adding custom functionality
- **Implementation**: Dynamic plugin loading with standardized interfaces
- **Key Features**:
  - Configuration loaders for different data types
  - Task creators for memory operations
  - Standardized plugin lifecycle management

## Architecture Flow

```
1. Injector Process
   ├── Parse command line arguments
   ├── Discover target process
   ├── Validate architecture compatibility 
   ├── Write configuration to temp file
   └── Inject hook DLL

2. Hook DLL (in target process)
   ├── Initialize logging system
   ├── Read injector configuration
   ├── Initialize plugin manager
   ├── Load plugins from directory
   ├── Initialize plugins with configuration
   ├── Create hooks from task configuration
   └── Install memory hooks using MinHook

3. Plugin Execution
   ├── Register configuration loaders
   ├── Register task creators
   ├── Load configuration files
   ├── Create and execute tasks
   └── Apply memory patches and hooks
```

## Memory Hooking Strategy

The system uses [MinHook](https://github.com/TsudaKageyu/minhook) library for instruction-level hooking:

1. **Memory Expansion**: Copy fixed-size data structures to larger memory regions
2. **Instruction Patching**: Redirect memory references to new expanded regions
3. **Configuration-Driven**: All addresses and patches defined in TOML files
4. **Runtime Application**: Hooks applied dynamically after DLL injection

## Plugin Interface

Plugins implement the `IPlugin` interface and provide:
- **Configuration Loaders**: Parse TOML configuration files into typed objects
- **Task Creators**: Generate executable tasks from configuration objects
- **Lifecycle Management**: Initialize, configure, and shutdown functionality

## Configuration System

The framework uses a hierarchical TOML-based configuration system:

- **Main Tasks File** (`tasks.toml`): Defines available tasks and their configurations
- **Task-Specific Files**: Individual configuration files for different operation types
- **Plugin Discovery**: Plugins register configuration types they can handle

## Security and Compatibility

- **Architecture Enforcement**: Strict validation of 32-bit compatibility
- **Error Isolation**: Comprehensive error handling prevents target application crashes  
- **Logging**: Detailed logging for debugging and monitoring
- **Graceful Degradation**: Fallback to defaults when configuration is unavailable

## Deployment Model

The system supports flexible deployment patterns:
- **Default Structure**: Standard directory layout for simple deployment
- **Custom Paths**: Configurable directories for complex deployment scenarios
- **Portable Mode**: Self-contained deployment without installation requirements 