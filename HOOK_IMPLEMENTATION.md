# Hook Implementation Documentation

## Overview

This document explains the custom hook implementation for FF8 Script Loader, detailing the approach used to create unique hook handlers for each hooked address and the technical challenges overcome.

## Problem Statement

The initial implementation faced a critical issue: **multiple hooks couldn't coexist** because they all shared the same generic hook handler function. When multiple addresses were hooked, they would all trigger the same handler, making it impossible to determine which specific hook was triggered.

## Solution: Per-Hook Instance Generation

We implemented a system that creates **unique executable code instances** for each hook, allowing multiple hooks to coexist while maintaining proper isolation.

## Technical Implementation

### 1. Hook Stub Template

```cpp
unsigned char hook_stub[] = {
    0x60,                               // pushad - save all general-purpose registers
    0x9C,                               // pushfd - save flags register
    0xB8, 0, 0, 0, 0,                   // mov eax, address (to be patched)
    0x50,                               // push eax - pass address as parameter
    0xE8, 0, 0, 0, 0,                   // call execute_hook_by_address (offset to be patched)
    0x83, 0xC4, 0x04,                   // add esp, 4 - cleanup stack (remove parameter)
    0x9D,                               // popfd - restore flags
    0x61,                               // popad - restore all registers
    0xFF, 0, 0, 0, 0                    // jmp to trampoline (to be patched)
};
```

### 2. Dynamic Code Generation (`create_hook_instance()`)

For each hook, we:

1. **Allocate executable memory** using `VirtualAlloc()` with `PAGE_EXECUTE_READWRITE`
2. **Copy the template** to the new memory location
3. **Patch three critical locations:**

#### Patch 1: Handler Address (offset 3)
```cpp
std::uintptr_t funcAddr = reinterpret_cast<std::uintptr_t>(newFunc);
memcpy(reinterpret_cast<char*>(newFunc) + 3, &funcAddr, 4);
```
- **Why**: The handler needs to pass its own address to identify which hook was triggered
- **Location**: `mov eax, address` instruction operand

#### Patch 2: Function Call Offset (offset 9)
```cpp
std::uintptr_t callSite = reinterpret_cast<std::uintptr_t>(newFunc) + 8 + 5;
std::uintptr_t targetAddr = reinterpret_cast<std::uintptr_t>(&execute_hook_by_address);
std::int32_t callOffset = static_cast<std::int32_t>(targetAddr - callSite);
memcpy(reinterpret_cast<char*>(newFunc) + 9, &callOffset, 4);
```
- **Why**: x86 `call` instruction uses relative addressing; we must calculate the offset from the instruction pointer to the target function
- **Calculation**: `target_address - (current_instruction_address + instruction_length)`

### 3. Hook Registration Strategy

```cpp
// Map handler addresses to hook instances (NOT original addresses)
g_hook_map[reinterpret_cast<std::uintptr_t>(handler)] = &hook;
```

**Critical Change**: We register hooks using the **handler address** as the key, not the original hooked address. This allows `execute_hook_by_address()` to correctly identify which hook instance was triggered.

### 4. Trampoline Patching (`patch_hook()`)

After MinHook creates the trampoline:

```cpp
void patch_hook(void* handler, void* trampoline) {
    char* hookCode = reinterpret_cast<char*>(handler);
    
    // Calculate relative offset for direct jmp (0xE9)
    std::uintptr_t jmpSite = reinterpret_cast<std::uintptr_t>(handler) + 18 + 5;
    std::uintptr_t trampolineAddr = reinterpret_cast<std::uintptr_t>(trampoline);
    std::int32_t jmpOffset = static_cast<std::int32_t>(trampolineAddr - jmpSite);
    
    // Patch: 0xE9 (direct jmp) + 4-byte relative offset
    hookCode[18] = 0xE9;
    memcpy(hookCode + 19, &jmpOffset, 4);
}
```

- **Why**: Each hook needs to jump to its specific trampoline after execution
- **When**: Called after `MH_CreateHook()` but before `MH_EnableHook()`

## Execution Flow

1. **Hook Trigger**: Original function call redirects to our unique handler
2. **Register Preservation**: `pushad`/`pushfd` save CPU state
3. **Handler Identification**: Handler passes its own address to `execute_hook_by_address()`
4. **Hook Lookup**: `g_hook_map` finds the correct hook instance using handler address
5. **Task Execution**: Hook-specific tasks are executed
6. **Register Restoration**: `popfd`/`popad` restore CPU state
7. **Continuation**: Direct jump to the specific trampoline continues normal execution

## Key Design Decisions

### Why Not Use the Original Address as Key?
- **Problem**: Multiple hooks would collide in the map
- **Solution**: Use unique handler addresses as keys for perfect isolation

### Why Relative Addressing?
- **x86 Requirement**: `call` and `jmp` instructions use relative offsets, not absolute addresses
- **Portability**: Works regardless of where code is loaded in memory

### Why Patch After Trampoline Creation?
- **Dependency**: We need the trampoline address before we can patch the final jump
- **Timing**: MinHook creates trampolines during `MH_CreateHook()`

## Benefits of This Approach

1. **Scalability**: Unlimited number of hooks can coexist
2. **Isolation**: Each hook has its own execution context
3. **Performance**: Direct jumps minimize overhead
4. **Reliability**: No shared state between different hooks
5. **Maintainability**: Clear separation of concerns

## Memory Management

- **Allocation**: `VirtualAlloc()` with execute permissions
- **Cleanup**: Handlers are registered in `g_handler_registry` for potential cleanup
- **Size**: Each handler instance is exactly `sizeof(hook_stub)` bytes

This implementation provides a robust foundation for the FF8 Script Loader's hooking system, enabling complex scripting scenarios with multiple concurrent hooks. 