# Available Plugins

This document describes the plugins currently available in the FFScriptLoader system and their capabilities.

## Memory Plugin

The Memory Plugin is the core plugin that provides memory manipulation functionality for legacy applications.

### Purpose
- **Memory Expansion**: Copy fixed-size data structures to larger memory regions
- **Instruction Patching**: Redirect memory references to expanded regions using MinHook
- **Configuration-Driven Operations**: All operations defined through TOML configuration files

### Configuration Loaders

#### MemoryConfigLoader
Handles memory expansion configuration:

**Configuration Type**: `memory`  
**File Format**: TOML sections defining memory regions to expand

**Example Configuration** (`memory_config.toml`):
```toml
[memory.K_MAGIC]
address = "0x01CF4064"           # Original memory address  
originalSize = 3420              # Current size in bytes
newSize = 4096                   # Expanded size in bytes
copyAfter = "0x0047D343"         # Address where copy should occur
description = "K_MAGIC data structure - expanded for custom spells"
```

**Properties**:
- `address`: Hexadecimal address of the original memory region
- `originalSize`: Current size of the data structure in bytes
- `newSize`: Target size after expansion
- `copyAfter`: Memory address where the copy operation should be triggered
- `description`: Human-readable description of the memory region

#### PatchConfigLoader
Handles instruction patching configuration:

**Configuration Type**: `patch`  
**File Format**: TOML sections defining instruction patches

**Example Configuration** (`magic_patch.toml`):
```toml
[metadata]
script_version = "1.0.0"
memory_base = "0x01CF4064"       # Base address for patch calculations
image_base = "0x00400000"        # Application image base
total_instructions = 67          # Total number of instructions to patch
description = "Pre-patched instructions for memory redirection"

[instructions.0x0048D774]
bytes = "8D 86 XX XX XX XX"      # Byte pattern (XX = placeholder)
offset = "0x2A"                   # Offset within the new memory region

[instructions.0x0048D8A4]
bytes = "8D 90 XX XX XX XX"
offset = "0x2A"
```

**Properties**:
- `memory_base`: Base address of the expanded memory region
- `image_base`: Application's image base address for calculations
- `total_instructions`: Number of instructions to patch
- `instructions.[address]`: Individual instruction patches
  - `bytes`: Byte pattern of the instruction (XX marks bytes to replace)
  - `offset`: Offset within the expanded memory region

### Tasks

#### CopyMemoryTask
Performs memory region copying and expansion.

**Features**:
- Allocates new memory regions with specified sizes
- Copies existing data to expanded regions
- Installs hooks at specified trigger addresses
- Provides logging and error handling

**Execution Flow**:
1. Allocate new memory region of specified size
2. Copy original data to new location
3. Install hook at the `copyAfter` address
4. Redirect memory access to new region

#### PatchMemoryTask  
Applies instruction-level patches to redirect memory references.

**Features**:
- Parses instruction byte patterns
- Calculates new memory addresses based on offsets
- Uses MinHook for safe instruction patching
- Handles multiple instruction patches per configuration

**Execution Flow**:
1. Parse instruction configurations
2. Calculate target addresses for each instruction
3. Generate new instruction bytes with updated addresses
4. Apply patches using MinHook
5. Verify patch installation

### Plugin Registration

The Memory Plugin registers with the plugin host during initialization:

```cpp
// Register configuration loaders
host_->register_config_loader(std::make_unique<MemoryConfigLoader>());
host_->register_config_loader(std::make_unique<PatchConfigLoader>());

// Register task creators
host_->register_task_creator("app_hook::config::CopyMemoryConfig", memory_task_creator);
host_->register_task_creator("app_hook::config::PatchConfig", patch_task_creator);
```

### Usage Example

**Task Configuration** (`tasks.toml`):
```toml
[tasks.memory_expansion]
name = "Memory Expansion"
description = "Tasks that expand memory regions for modding"
type = "memory"
config_file = "config/tasks/memory_config.toml"
followBy = ["magic_patches"]
enabled = true

[tasks.magic_patches]
name = "Magic System Patches"
description = "Pre-patched instructions for magic system modifications"
type = "patch"
config_file = "config/tasks/magic_patch.toml"
enabled = true
```

### Target Applications

The Memory Plugin is particularly useful for:

- **Game Modding**: Expanding data structures for custom content
- **Legacy Software Enhancement**: Adding new functionality to old applications
- **Data Structure Modifications**: Changing fixed-size arrays to dynamic sizes
- **Memory Layout Optimization**: Reorganizing memory for better performance

### Limitations

- **32-bit Only**: Works only with 32-bit applications
- **Static Addresses**: Requires known memory addresses (not ASLR-compatible)
- **Instruction Dependencies**: Patch configurations must be accurate
- **MinHook Dependency**: Relies on MinHook library for safe patching

## Future Plugin Development

The plugin architecture supports extending the system with additional plugins:

- **Script Execution Plugins**: Execute custom scripts within target applications
- **Network Communication Plugins**: Add network functionality to legacy applications
- **File System Plugins**: Modify file access patterns
- **Graphics Enhancement Plugins**: Improve visual output of legacy applications
- **Input Handling Plugins**: Add modern input device support

## Plugin Dependencies

Current plugins depend on:

- **core_hook**: Core plugin framework and interfaces
- **MinHook**: Low-level hooking library for instruction patching
- **toml11**: TOML configuration file parsing
- **Windows API**: Process and memory manipulation functions

## Configuration Validation

The Memory Plugin includes validation for:

- **Address Format**: Ensures hexadecimal addresses are properly formatted
- **Size Constraints**: Validates that `newSize` is larger than `originalSize`
- **Instruction Patterns**: Verifies instruction byte patterns are valid
- **Offset Ranges**: Ensures offsets are within expanded memory regions

## Error Handling

The plugin provides comprehensive error handling:

- **Memory Allocation Failure**: Graceful handling when memory cannot be allocated
- **Hook Installation Failure**: Detailed logging when hooks cannot be installed
- **Configuration Errors**: Clear error messages for invalid configurations
- **Target Process Issues**: Handles cases where target process memory is inaccessible 