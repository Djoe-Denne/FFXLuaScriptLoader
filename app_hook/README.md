# Application Hook Module

This module demonstrates the use of **MinHook** library for low-level inline patching and function hooking in x32 applications.

## Overview

The Application Hook module (`app_hook.dll`) provides a framework for:

- **Inline Assembly Hooking**: Using naked functions with inline assembly for precise control
- **Machine Code Manipulation**: Backing up and executing original instruction bytes
- **Low-level Patching**: Hooking at specific instruction offsets rather than function boundaries
- **MinHook Integration**: Using the MinHook library for hook management
- **Configuration-Driven Patching**: TOML-based configuration for flexible targeting

## Key Features

### 🎯 **Instruction-Level Hooking**
- Hooks at configurable RVA offsets
- Patches specific bytes of machine code
- Preserves and executes original instructions

### 🔧 **Low-Level Control**
- Uses `__declspec(naked)` functions for assembly control
- Manual register preservation with `pushad`/`popad`
- Direct machine code execution via inline assembly

### 🛡️ **Safe Hook Management**
- Automatic hook installation on DLL load
- Proper cleanup on DLL unload
- Error handling for hook operations

### 📋 **Configuration-Driven**
- TOML configuration files for patch definitions
- Dynamic memory expansion capabilities
- No hardcoded addresses - fully configurable

## Technical Details

### Configuration-Based Targeting
```toml
[patch]
offset = 0x47D2A0 + 0x41  # Specific instruction address
length = 6                # Bytes to patch (minimum 5 for JMP)
```

### Hook Flow
1. **Load Configuration**: Read TOML files for patch definitions
2. **Backup Original Bytes**: Store the original bytes being overwritten
3. **Install Hook**: MinHook replaces target with jump to our handler
4. **Execute Handler**: Process the hook according to configuration
5. **Restore & Continue**: Execute original bytes and return to function

### Assembly Implementation
```cpp
void __declspec(naked) MyHookHandler() {
    __asm {
        pushad          // Save all registers
        pushfd          // Save flags
    }
    
    OnHookTriggered();  // C++ callback
    
    __asm {
        popfd           // Restore flags
        popad           // Restore registers
        // Execute original overwritten instructions
        lea esi, originalBytes
        mov ecx, PATCH_LENGTH
        call ExecuteOriginalBytes
        jmp [pTrampoline]  // Return to original function
    }
}
```

## Differences from Detours

| Feature | MinHook | Microsoft Detours |
|---------|---------|-------------------|
| **Level** | Instruction-level | Function-level |
| **Control** | Manual assembly | Automatic trampolines |
| **Size** | Smaller library | Larger, more features |
| **License** | BSD-2-Clause | Proprietary/Commercial |
| **Complexity** | Lower-level, more manual | Higher-level, easier |

## Usage

1. **Configure**: Set up TOML configuration files for your target application
2. **Build**: The module builds as `app_hook.dll`
3. **Inject**: Use the included injector or any DLL injection method
4. **Deploy**: Place configuration files in the `config/` directory
5. **Run**: Launch your target application

## Requirements

- **MinHook Library**: Automatically fetched via CMake FetchContent
- **x86 Target**: Designed for 32-bit applications
- **MSVC Compiler**: Uses Microsoft-specific inline assembly syntax
- **TOML++ Library**: Automatically fetched for configuration parsing

## Dependencies

All dependencies are automatically managed via CMake FetchContent:
- **MinHook**: For low-level function hooking
- **spdlog**: For logging capabilities  
- **TOML++**: For configuration file parsing

## Configuration

Place your TOML configuration files in the `config/` directory:

- `memory_config.toml` - Defines memory regions to expand
- `patch_config.toml` - Instruction patches for memory expansion
- `tasks.toml` - Task definitions for the target application

## Warning

⚠️ **Important Considerations**
- Offsets are specific to particular binary versions
- Always verify instruction boundaries before patching
- Test thoroughly to ensure stability with your target application
- Back up your target application before testing

## License

This module follows the same license as the parent Legacy Software Extension Framework project. 