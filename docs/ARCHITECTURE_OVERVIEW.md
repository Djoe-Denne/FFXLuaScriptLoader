# Architecture Overview

**Version:** Expert Technical Deep Dive

This document provides a granular, code‑level exploration of our DLL injection framework and plugin subsystem, detailing the Injector, Host DLL internals, core\_hook components, memory\_plugin implementation, and the FF8 Kernel‑Bypass case study.

---

## Table of Contents

1. [High‑Level Architecture](#high-level-architecture)
2. [Injector Component](#injector-component)
3. [Host DLL Internals](#host-dll-internals)

   * 3.1 [HookManager (core\_hook)](#hookmanager-core_hook)
   * 3.2 [PluginManager & TaskFactory](#pluginmanager--taskfactory)
   * 3.3 [Configuration Subsystem (Config Loaders)](#configuration-subsystem-config-loaders)
   * 3.4 [TaskScheduler & Execution Context](#taskscheduler--execution-context)
4. [memory\_plugin: Implementation Details](#memory_plugin-implementation-details)

   * 4.1 [CopyMemoryTask](#copymemorytask)
   * 4.2 [PatchMemoryTask](#patchmemorytask)
   * 4.3 [LoadInMemoryTask](#loadinmemorytask)
5. [Case Study: FF8 Kernel‑Bypass Revisited](#case-study-ff8-kernel-bypass-revisited)
6. [Summary](#summary)

---

> **Warning:** Windows may enable Address Space Layout Randomization (ASLR), which can cause loaded module and data addresses to vary between executions. Although fixed addresses have worked in our tests, ASLR can break hooks or memory offsets on some systems. Contributors should test on varied configurations and report any anomalies.

* **Injector**: A user‑land executable (`injector.exe`) that uses Win32 APIs (`OpenProcess`, `VirtualAllocEx`, `WriteProcessMemory`, `CreateRemoteThread(LoadLibraryW)`) to inject **Host.dll** into the target process.
* **Host DLL**: Upon load, initializes the hook subsystem, reads `tasks.toml`, registers config‐loaders and plugins, then schedules tasks via hooks.
* **Plugins**: Implement `IPlugin` interface, register **ConfigLoaders** and **tasks** via `PluginHostInterface`.
* **Task Types**:

  * **Memory**: Relocate data (stack/static → heap).
  * **Patch**: Overwrite instruction operands to reference new regions.
  * **Load**: Overwrite relocated regions with new binary blobs.

---

## 2. Injector Component

```cpp
// injector/src/injector.cpp
int main(int argc, char** argv) {
    DWORD pid = ParseTargetPID();
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    std::wstring hostPath = ResolveDLLPath("Host.dll");
    LPVOID remoteMem = VirtualAllocEx(hProc, nullptr, (hostPath.size()+1)*sizeof(wchar_t), MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(hProc, remoteMem, hostPath.c_str(), ...);
    CreateRemoteThread(hProc, nullptr, 0,
        (LPTHREAD_START_ROUTINE) GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"),
        remoteMem, 0, nullptr);
    // Optionally inject plugin DLLs similarly
}
```

---

## 3. Host DLL Internals

Upon `DLL_PROCESS_ATTACH`, `Host.dll` runs:

```cpp
BOOL WINAPI DllMain(HMODULE, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    HookManager::Initialize();
    ConfigManager::Load("tasks.toml");
    PluginManager::LoadAll("plugins/");
    TaskFactory::RegisterConfigLoaders();
    TaskScheduler::Start();
  }
  return TRUE;
}
```

### 3.1 HookManager (core\_hook)

* Wraps [MinHook](https://github.com/TsudaKageyu/minhook):

  * `MH_Initialize()`
  * `MH_CreateHook(targetAddr, detourFn, &trampolinePtr)`
  * `MH_EnableHook()`
* **Trampoline**: Holds original bytes + a jump back to `targetAddr + overwrittenLength`.
* **hook\_manager.cpp** orchestrates hook lifecycles and exposes:

  ```cpp
  class HookManager {
    MH_STATUS Initialize();
    bool CreateHook(void* target, void* detour, void** trampoline);
  };
  ```

### 3.2 PluginManager & TaskFactory

* **PluginManager** scans `./plugins/*.dll`, calls `LoadLibraryEx`, and retrieves `CreatePlugin`:

  ```cpp
  IPlugin* plugin = CreatePlugin();
  plugin->setHost(hostInterface);
  plugin->get_plugin_info();
  plugin->load_configurations(configPath);
  plugin->register_tasks();
  ```
* **TaskFactory** maps config sections to task instances:

  * MemoryTaskConfig ➔ `CopyMemoryTask`
  * PatchTaskConfig  ➔ `PatchMemoryTask`
  * LoadTaskConfig   ➔ `LoadInMemoryTask`

### 3.3 Configuration Subsystem (Config Loaders)

* Each plugin provides `IConfigLoader` implementations to parse TOML into config structs.
* E.g., **MemoryConfigLoader**:

  ```cpp
  struct CopyMemoryConfig { uint64_t address; size_t originalSize, newSize; std::string key; };
  class MemoryConfigLoader : public IConfigLoader<CopyMemoryConfig> { ... };
  ```
* Registered during `Plugin::register_configurations()`, then invoked by the `ConfigManager` when reading `tasks.toml`.

### 3.4 TaskScheduler & Execution Context

* **TaskScheduler** builds a DAG of tasks based on `followBy` ordering in `tasks.toml`.
* **HookTask** (core\_hook/task/hook\_task.cpp) wraps a `Task` and is registered via `HookManager` at the specified install address.
* On interception, a `ModContext` is constructed, containing registers, memory pointers, and references to named memory regions.
* Handler calls `Task::execute(ModContext&)`, then returns control via trampoline.

---

## 4. memory\_plugin: Implementation Details

The `memory_plugin` DLL implements three tasks. All classes live under `app_hook::memory`.

### 4.1 CopyMemoryTask

```cpp
// copy_memory.hpp
class CopyMemoryTask : public task::ITask {
  CopyMemoryConfig config_;
  std::shared_ptr<MemoryRegion> region_;
  TaskResult execute(ModContext& ctx) override {
    // 1. Allocate new heap block
    void* newPtr = HeapAlloc(GetProcessHeap(), 0, config_.newSize);
    // 2. memcpy from ctx.readMemory(config_.address) into newPtr (config_.originalSize)
    // 3. ctx.registerRegion(config_.key, newPtr, config_.newSize)
    return TaskResult::Ok;
  }
};
```

* Registered by `MemoryConfigLoader`, which reads `address`, `originalSize`, `newSize`, `key`.
* Creates a `HookTask` at the first read site to trigger the allocation/copy.

### 4.2 PatchMemoryTask

```cpp
// patch_memory.hpp
class PatchMemoryTask : public task::ITask {
  PatchConfig config_;
  vector<PatchInstruction> patches_;
  TaskResult execute(ModContext& ctx) override {
    for (auto &p : patches_) {
      uint8_t* instr = ctx.base() + p.offset;
      // write config_.getContextPtr() into operand at instr+p.opOffset
    }
    return TaskResult::Ok;
  }
};
```

* **PatchConfigLoader** parses an array of `{ address, offset, bytesTemplate }` entries.
* Patches are applied at injection time (before main loop) or on first hook invocation.

### 4.3 LoadInMemoryTask

```cpp
// load_in_memory.hpp
class LoadInMemoryTask : public task::ITask {
  LoadConfig config_;
  TaskResult execute(ModContext& ctx) override {
    // Read binary file from config_.binaryPath
    auto blob = readFile(config_.binary);
    void* region = ctx.getRegion(config_.readFromContext);
    memcpy(region + config_.offsetSecurity, blob.data(), blob.size());
    return TaskResult::Ok;
  }
};
```

* Hooks the final `push` or loader call to overwrite the relocated region with new data.
* Ensures checksums/sizes are updated if necessary.

---

## 5. Case Study: FF8 Kernel‑Bypass In Detail

Below is a step‑by‑step execution timeline showing how each plugin and task transforms the original FF8 magic‑load flow. Refer to the diagram (FF8\_kernel\_bypass.drawio.png) for visual context.

1. **Initial Stack Reads (Original Behavior)**

   * **Addresses**:

     * Magic logic blob at `0x01CF4064` (size 0xD64 bytes)
     * Magic text resources at `0x01CF8FD0` (size 0x630 bytes)
   * **Instructions**:

     * `mov esi, [0x01CF4064]`  (magic data)
     * `mov edx, [0x01CF8FD0]`  (text data)
   * **Outcome**: Data lives on the stack; final push combines both before disk read of `kernel.bin`.

2. **CopyMemoryPlugin → CopyMagicData Task**

   * **Hook Site**: First magic‑related read at `0x0048D774`.
   * **Config**: `copy_magic_memory_config.toml`:

     ```toml
     [memory.K_MAGIC]
     address       = "0x01CF4064"
     originalSize  = 3420          # 0xD64
     newSize       = 4096          # allocate 4 KiB heap
     copyAfter     = "0x0047D340" # next instruction
     writeInContext = { name = "ff8.magic.k_magic_data", enabled = true }
     ```
   * **Execution**:

     1. Trampoline to `CopyMemoryTask::execute()`.
     2. `HeapAlloc(4096)` → newRegion.
     3. `memcpy(newRegion, 0x01CF4064, 3420)`.
     4. Register context key `ff8.magic.k_magic_data` pointing to newRegion.
     5. Resume original code via trampoline.

3. **PatchMemoryPlugin → PatchMagicEngine Task**

   * **Hook Site**: Injection at engine init (before all magic‑related loops).
   * **Config**: `magic_patch.toml` (partial):

     ```toml
     [metadata]
     memory_base     = "0x01CF4064"
     readFromContext = "ff8.magic.k_magic_data"

     [instructions.0x0048D774]
     bytes           = "8B 86 XX XX XX XX"  # mov eax, [base+offset]
     offset          = "0x2A"
     # ... total 67 instructions patched
     ```
   * **Execution**:

     * On host startup, `PatchMemoryTask` iterates through each listed address.
     * For each, computes `newPtr + (origAddr - memory_base)` and writes into instruction operand.
     * Ensures all reads now target heap region.

4. **CopyMemoryPlugin → CopyMagicText Task**

   * **Hook Site**: First text read at `0x0048D78A`.
   * **Config**: `copy_magic_text_memory_config.toml`:

     ```toml
     [memory.TEXT_MAGIC]
     address       = "0x01CF8FD0"
     originalSize  = 1584          # 0x630
     newSize       = 4096
     copyAfter     = "0x0047D343"
     writeInContext = { name = "ff8.magic.text_data", enabled = true }
     ```
   * **Execution**: Identical steps to CopyMagicData but for text data.

5. **PatchMemoryPlugin → PatchTextEngine Task**

   * **Hook Site**: Batch patch at engine start.
   * **Config**: `magic_text_patch.toml`:

     ```toml
     [metadata]
     readFromContext = "ff8.magic.text_data"

     [instructions.0x01CF3ECC]
     bytes           = "8B 05 XX XX XX XX"  # mov eax, [text_base]
     offset          = "-0x01CF3E48"
     # plus ~20 more text references
     ```
   * **Execution**: Overwrites instruction operands to point into text heap region.

6. **LoadInMemoryPlugin → LoadNewMagic Task**

   * **Hook Site**: Final push of kernel blob at `0x0047D350`.
   * **Config**: `load_new_magic.toml`:

     ```toml
     [load.NEW_MAGIC]
     binary          = "./mods/xtender/magic/new_magic.bin"
     offsetSecurity  = "0x0"
     readFromContext = "ff8.magic.k_magic_data"
     ```
   * **Execution**:

     1. Read `new_magic.bin` into buffer.
     2. `memcpy(region_base, buffer, buffer.size())`.
     3. Optionally update checksum if engine validates size.

7. **LoadInMemoryPlugin → LoadNewMagicText Task**

   * **Hook Site**: Same push, but text offset `0x1FF` within heap block.
   * **Config**: `load_new_magic_text.toml`:

     ```toml
     [load.NEW_MAGIC_TEXT]
     binary          = "./mods/xtender/magic/new_magic_english.resources.bin"
     offsetSecurity  = "0x1FF"
     readFromContext = "ff8.magic.text_data"
     ```
   * **Execution**: Overwrites text region payload with new localized strings.

8. **Result**:

   * Game logic reads entirely from our heap‑based blobs.
   * No on‑disk `kernel.bin` reads occur.
   * New spells and text seamlessly integrated at runtime.

---

## 6. Summary Summary

This deep dive has traced the entire flow:

* How **injector** loads the **Host DLL**
* How **MinHook** and trampolines intercept execution
* How **PluginManager**, **Config Loaders**, and **TaskFactory** wire TOML → Task instances
* How **TaskScheduler** and **HookTask** invoke C++ `execute()` routines
* The three `memory_plugin` tasks in production

Developers can follow these patterns to implement additional plugins (Lua, item injection, game‑specific logic) by leveraging the same host interfaces and hook/region infrastructure.
