# FF8 Script Loader

A dynamic memory expansion and script injection system for Final Fantasy VIII, allowing deep modification of game logic without assembly code caves or static patches.

## 🎯 Overview

FF8 Script Loader is a DLL injection system that enables runtime modification of Final Fantasy VIII by:

- **Dynamic Memory Expansion**: Copies fixed-size data structures from original FF8 memory to dynamically allocated, larger memory regions
- **Automatic Patching**: Redirects all original memory accesses to the new expanded memory locations  
- **Lua Script Integration**: *(Planned)* Creates trampolines for in-memory Lua scripts to modify game behavior
- **No Code Caves**: Eliminates the need for assembly code caves or manual binary patching

This approach allows for extensive game modifications while maintaining compatibility and avoiding the complexity of traditional memory hacking techniques.

## 🏗️ Architecture

The system consists of three main components:

### 1. **FF8 Hook DLL** (`ff8_hook.dll`)
- Core hooking engine using MinHook library
- Memory region expansion and data copying
- Dynamic code generation for per-hook instances
- Configuration-driven patch application

### 2. **DLL Injector** (`ff8_injector.exe`)  
- Injects the hook DLL into the FF8 process
- Handles process targeting and injection timing
- 32-bit compatible for FF8 compatibility

### 3. **Configuration System**
- **Memory Config**: Defines memory regions to expand (`memory_config.toml`)
- **Patch Templates**: Pre-generated instruction patches (`magic_patch.toml`)
- **Script Integration**: *(Planned)* Lua script management

## 📋 Features

### ✅ Implemented
- [x] Dynamic hook generation with unique handlers per address
- [x] Memory region expansion (K_MAGIC data structure example)
- [x] Automated instruction patching via configuration
- [x] MinHook-based hooking with custom trampoline management
- [x] Python script for generating patch configurations from IDA Pro analysis

### 🚧 In Development  
- [ ] Automatic memory access patching (replaces original addresses with new ones)
- [ ] In-memory Lua script execution via trampolines
- [ ] Runtime script reloading and hot-swapping
- [ ] Advanced memory region management

## 🛠️ Technical Details

### Memory Expansion Process

1. **Configuration Loading**: Read memory regions from `config/memory_config.toml`
```toml
[memory.K_MAGIC]
address = "0x01CF4064"        # Original FF8 memory location
originalSize = 2850           # Current data size
newSize = 4096               # Expanded size
copyAfter = "0x0047D343"     # Hook installation point
patch = "magic_patch.toml"   # Patch configuration file
```

2. **Dynamic Allocation**: Allocate larger memory region with `VirtualAlloc()`

3. **Data Migration**: Copy original data to new location preserving structure

4. **Patch Application**: Replace all memory references using pre-generated patches

### Hook System Architecture

The project implements a sophisticated hooking system that creates **unique executable code instances** for each hook:

```cpp
// Dynamic hook template (per hook instance)
unsigned char hook_stub[] = {
    0x60,                    // pushad - save registers
    0x9C,                    // pushfd - save flags  
    0xB8, 0, 0, 0, 0,       // mov eax, address (patched)
    0x50,                    // push eax
    0xE8, 0, 0, 0, 0,       // call execute_hook_by_address (patched)
    0x83, 0xC4, 0x04,       // add esp, 4
    0x9D,                    // popfd - restore flags
    0x61,                    // popad - restore registers
    0xFF, 0, 0, 0, 0        // jmp to trampoline (patched)
};
```

This approach allows unlimited concurrent hooks with proper isolation.

## 🚀 Quick Start

### Prerequisites

- Windows 10/11 (32-bit compatible)
- Visual Studio 2019+ with C++20 support
- CMake 3.25+
- Final Fantasy VIII (PC version)

### Building

1. **Clone the repository**
```bash
git clone https://github.com/Djoe-Denne/FFXLuaScriptLoader.git
cd FFScriptLoader
```

2. **Configure and build**
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A Win32
cmake --build . --config Release
```

3. **Output files**
- `build/bin/ff8_hook.dll` - Main hook library
- `build/bin/ff8_injector.exe` - DLL injector

### Usage

1. **Prepare configuration** (see `config/memory_config.toml`)

2. **Inject into FF8**
```bash
ff8_injector.exe FF8.exe ff8_hook.dll
```

3. **Launch Final Fantasy VIII** - The hooks will activate automatically

## 📁 Project Structure

```
FFScriptLoader/
├── ff8_hook/              # Main hook DLL source
│   ├── src/              # Hook implementation
│   ├── include/          # Header files
│   └── CMakeLists.txt    # Build configuration
├── injector/             # DLL injector source  
│   ├── src/              # Injector implementation
│   └── CMakeLists.txt    # Build configuration
├── config/               # Configuration files
│   ├── memory_config.toml    # Memory regions to expand
│   └── magic_patch.toml      # Pre-generated patches
├── script/               # Python utilities
│   ├── patch_memory_usage.py    # IDA Pro → patch generator
│   └── ida_usage_memory.csv     # Example IDA analysis
└── docs/
    ├── HOOK_IMPLEMENTATION.md   # Technical documentation
    └── README_patch_memory_usage.md  # Python script docs
```

## 🔧 Configuration

### Memory Region Configuration

Define memory regions to expand in `config/memory_config.toml`:

```toml
[memory.K_MAGIC]
address = "0x01CF4064"      # Original address
originalSize = 2850         # Current size  
newSize = 4096             # Target size
copyAfter = "0x0047D343"   # When to install hook
patch = "magic_patch.toml" # Patch instructions file
description = "K_MAGIC data structure - expanded for custom spells"

[memory.ANOTHER_REGION]
address = "0x01234567"
originalSize = 1000
newSize = 2000
# ... additional regions
```

### Generating Patch Files

Use the Python script to generate patch configurations from IDA Pro analysis:

```bash
cd script/
python patch_memory_usage.py \
    --csv ida_memory_usage.csv \
    --binary FF8_EN.exe \
    --memory-base 0x01CF4064 \
    --output magic_patch.toml
```

## 🎮 Example: Magic System Expansion

The included example expands FF8's magic system data structure:

1. **Original**: `K_MAGIC` at `0x01CF4064`, 2850 bytes (limited spells)
2. **Expanded**: New allocation of 4096 bytes (room for custom spells)  
3. **Patched**: 67 instructions automatically redirected to new memory
4. **Result**: Can add custom spells without overwriting existing data

## 🤝 Contributing

Contributions are welcome! Areas of particular interest:

- **Lua Integration**: Implementing the planned Lua scripting system
- **Additional Games**: Adapting the system for other Final Fantasy titles
- **UI Tools**: Creating user-friendly configuration editors
- **Documentation**: Expanding guides and examples

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## ⚠️ Disclaimer

This project is for educational and modding purposes only. It requires a legitimate copy of Final Fantasy VIII. The authors are not responsible for any damage to game files or save data.

## 🔗 Related Projects

- [MinHook](https://github.com/TsudaKageyu/minhook) - The hooking library used by this project
- [Capstone](https://www.capstone-engine.org/) - Disassembly framework used in patch generation
- [OpenVIII](https://github.com/MaKiPL/OpenVIII) - Open source FF8 engine reimplementation

---

**FF8 Script Loader** - Enabling deep Final Fantasy VIII modifications without the complexity of traditional memory hacking. 