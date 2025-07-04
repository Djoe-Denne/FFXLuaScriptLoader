# Legacy Software Extension Framework

A dynamic memory expansion and automated instruction patching system for extending legacy x32 software behavior through DLL injection.

## What It Does

Legacy Software Extension Framework enables runtime modification of legacy applications through:

- **Dynamic Memory Expansion**: Copies fixed-size data structures to larger memory regions
- **Automated Instruction Patching**: Redirects memory references using TOML configuration
- **Configuration-Driven**: No hardcoded addresses - everything driven by TOML files
- **Generic DLL Injection**: Works with any x32 process by specifying the target executable

Example: Expands data structures from fixed sizes to larger memory regions by automatically patching multiple instructions.

## Prerequisites

- Windows 10/11 (32-bit compatible)
- Visual Studio 2019+ with C++23 support  
- CMake 3.25+
- Target legacy software (32-bit)

## Building

```bash
git clone https://github.com/Djoe-Denne/SoftwareExtension.git
cd SoftwareExtension

cmake -B build -A Win32 -DBUILD_TESTING=OFF

# Build
cmake --build build --config Release
```

Output files:
- `build/bin/app_hook.dll` - Main hook library
- `build/bin/app_injector.exe` - DLL injector

2. The injector will wait for the target process to start and automatically inject the DLL

## Testing

### Unit Tests

Build and run the unit test suite:

```bash
# Build test executable
cmake --build build --target app_hook_tests

# Run all tests
build/bin/tests/Debug/app_hook_tests.exe
```

## Configuration

Place TOML configuration files in the `config/` directory:

- `memory_config.toml` - Defines memory regions to expand
- `patch_config.toml` - Instruction patches for memory expansion
- `tasks.toml` - Task definitions for the target application

Configuration files in the `@/config` directory remain target-specific and should not be modified when making the framework generic.

## Project Structure

```
LegacySoftwareExtension/
├── app_hook/              # Main hook DLL source
├── injector/              # DLL injector executable
├── tests/                 # Unit test suite
├── config/                # TOML configuration files
└── script/                # Patch generation tools
```

## Architecture Support

This framework is designed for legacy 32-bit software. The build system enforces x32 architecture:

- Both the injector and DLL are built as 32-bit
- Architecture compatibility is verified at injection time
- Mismatch between injector/DLL and target process architecture will result in clear error messages

## Usage Patterns

### For Game Modding
Configure memory expansions and patches for game data structures.

### For Legacy Business Software
Extend functionality by hooking into existing routines and expanding data storage.

### For Educational Purposes
Study and modify legacy software behavior in a controlled environment.

## License

MIT License - Educational and modding purposes only. 