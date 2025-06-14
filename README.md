# FF8 Script Loader

A dynamic memory expansion and automated instruction patching system for Final Fantasy VIII.

## What It Does

FF8 Script Loader enables runtime modification of Final Fantasy VIII through:

- **Dynamic Memory Expansion**: Copies fixed-size data structures to larger memory regions
- **Automated Instruction Patching**: Redirects memory references using TOML configuration
- **Configuration-Driven**: No hardcoded addresses - everything driven by TOML files

Example: Expands K_MAGIC structure from 2850 → 4096 bytes by automatically patching 67+ instructions.

## Prerequisites

- Windows 10/11 (32-bit compatible)
- Visual Studio 2019+ with C++23 support  
- CMake 3.25+
- Final Fantasy VIII (PC version)

## Building

```bash
git clone https://github.com/Djoe-Denne/FFScriptLoader.git
cd FFScriptLoader
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A Win32
cmake --build . --config Release
```

Output files:
- `build/bin/ff8_hook.dll` - Main hook library
- `build/bin/ff8_injector.exe` - DLL injector

## Running

1. Inject into FF8:
```bash
ff8_injector.exe FF8.exe ff8_hook.dll
```

2. Launch Final Fantasy VIII

## Testing

Build and run the test suite (93 tests):

```bash
# Configure with testing enabled
cmake -B build -DBUILD_TESTING=ON

# Build test executable
cmake --build build --target ff8_hook_tests

# Run all tests
build/bin/Debug/ff8_hook_tests.exe
```

## Configuration

Place TOML configuration files in the `config/` directory:

- `memory_config.toml` - Defines memory regions to expand
- `magic_patch.toml` - Instruction patches for K_MAGIC expansion

## Project Structure

```
FFScriptLoader/
├── ff8_hook/              # Main hook DLL source
├── tests/                 # Unit test suite (93 tests)
├── config/                # TOML configuration files
└── script/                # Patch generation tools
```

## License

MIT License - Educational and modding purposes only. 