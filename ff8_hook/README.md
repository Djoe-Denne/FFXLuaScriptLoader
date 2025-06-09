# MinHook Test Module

This module demonstrates the use of **MinHook** library for low-level inline patching and function hooking in x86 applications.

## Overview

The MinHook Test module (`minhook_test.dll`) provides a minimal example of:

- **Inline Assembly Hooking**: Using naked functions with inline assembly for precise control
- **Machine Code Manipulation**: Backing up and executing original instruction bytes
- **Low-level Patching**: Hooking at specific instruction offsets rather than function boundaries
- **MinHook Integration**: Using the MinHook library for hook management

## Key Features

### 🎯 **Instruction-Level Hooking**
- Hooks at a specific RVA offset (`0x47D2A0 + 0x41`)
- Patches exactly 6 bytes of machine code
- Preserves and executes original instructions

### 🔧 **Low-Level Control**
- Uses `__declspec(naked)` functions for assembly control
- Manual register preservation with `pushad`/`popad`
- Direct machine code execution via inline assembly

### 🛡️ **Safe Hook Management**
- Automatic hook installation on DLL load
- Proper cleanup on DLL unload
- Error handling for hook operations

## Technical Details

### Hook Target
```cpp
#define PATCH_OFFSET 0x47D2A0 + 0x41  // Specific instruction address
#define PATCH_LENGTH 6                 // Bytes to patch (minimum 5 for JMP)
```

### Hook Flow
1. **Backup Original Bytes**: Store the original 6 bytes being overwritten
2. **Install Hook**: MinHook replaces target with jump to our handler
3. **Execute Handler**: Show message box when hook is triggered
4. **Restore & Continue**: Execute original bytes and return to function

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

1. **Build**: The module builds as `minhook_test.dll`
2. **Inject**: Use any DLL injection method to load into target process
3. **Trigger**: When the hooked instruction is executed, a message box appears
4. **Observe**: Check that the original functionality continues to work

## Requirements

- **MinHook Library**: Must be installed and accessible to CMake
- **x86 Target**: Designed for 32-bit applications
- **MSVC Compiler**: Uses Microsoft-specific inline assembly syntax

## Installation

The CMake configuration automatically searches for MinHook in common locations:
- `C:/Program Files/MinHook/`
- `C:/MinHook/`
- `libs/minhook/`

If not found automatically, adjust the paths in `CMakeLists.txt`.

## Warning

⚠️ **This is demonstration code for educational purposes**
- The hardcoded offset (`0x47D2A0 + 0x41`) is specific to a particular binary
- Modify `PATCH_OFFSET` to target your specific application
- Always verify instruction boundaries before patching
- Test thoroughly to ensure stability

## License

This module follows the same license as the parent FF8 Script Loader project. 