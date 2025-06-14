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
git clone https://github.com/Djoe-Denne/LegacySoftwareExtension.git
cd LegacySoftwareExtension

# Configure using preset (includes testing by default)
cmake --preset=default-x32

# Build
cmake --build build --config Release
```

Output files:
- `build/bin/app_hook.dll` - Main hook library
- `build/bin/app_injector.exe` - DLL injector

## Running

1. Inject into target application:
```bash
app_injector.exe <process_name> [dll_name]
```

Examples:
```bash
# Inject app_hook.dll into myapp.exe
app_injector.exe myapp.exe

# Inject custom_hook.dll into game.exe
app_injector.exe game.exe custom_hook.dll

# Launch target application after starting injector
```

2. The injector will wait for the target process to start and automatically inject the DLL

## Testing

### Unit Tests

Build and run the unit test suite:

```bash
# Configure using preset (testing enabled by default)
cmake --preset=default-x32

# Build test executable
cmake --build build --target app_hook_tests

# Run all tests
build/bin/Debug/app_hook_tests.exe
```

### Integration Test

Run the integration test to verify DLL injection and plugin loading work correctly:

```bash
# Build everything including integration test
cmake --preset=default-x32
cmake --build build --config Release

# Run automated build and test
build_and_test.bat
```

Or run manually:

```bash
# Navigate to integration test directory
cd integration-test

# Run the test
run_test.bat
```

The integration test:
- Starts a test application that waits for termination signal
- Injects the main DLL and test plugin
- Triggers task execution via Windows events
- Verifies the plugin can successfully signal app termination
- Reports TEST PASSED/FAILED based on results

Expected output on success:
```
=========================================
    Integration Test - DLL Injection
=========================================
Starting test_app.exe...
Running injector...
Waiting for test completion...
TEST PASSED - App terminated successfully!
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