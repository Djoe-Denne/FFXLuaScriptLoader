# Integration Test

This directory contains integration tests to verify that DLL injection and plugin loading are working correctly.

## Overview

The integration test verifies the following functionality:

1. **Main DLL Injection**: The `app_hook.dll` can be successfully injected into a target process
2. **Plugin Loading**: Plugins are loaded from the `tasks/` directory after main DLL injection
3. **Task Execution**: Plugin tasks can be executed and perform their intended functions
4. **Event System**: The event-based triggering of task execution works correctly

## Components

### Test Application (`test_app.exe`)
- A simple console application that waits for a termination signal
- Creates a named event for receiving termination signals
- Exits with code 0 if termination signal is received (test passes)  
- Exits with code 1 if timeout occurs without signal (test fails)

### Test Plugin (`test_plugin.dll`)
- Implements the `IPlugin` interface
- Registers a simple configuration and task loader
- Contains a `TerminationTask` that sends a termination signal to the test app
- Demonstrates plugin registration and task execution

### Test Script (`run_integration_test.ps1`)
- PowerShell script that orchestrates the entire test
- Starts the test application
- Runs the injector to inject main DLL and plugins
- Triggers task execution via Windows events
- Verifies test completion within timeout

## How It Works

1. **Test App Startup**: The test app starts and creates a termination event
2. **DLL Injection**: The injector injects `app_hook.dll` into the test process
3. **Plugin Injection**: The injector injects `test_plugin.dll` from the `tasks/` directory
4. **Initialization**: The main DLL initializes plugins and registers their configurations
5. **Task Registration**: The test plugin registers a termination task
6. **Trigger Event**: The test script signals a task execution trigger event
7. **Task Execution**: The main DLL receives the trigger and executes all tasks manually
8. **Termination**: The test plugin's task signals the test app to terminate
9. **Verification**: The test script verifies the app terminated successfully

## Running the Test

### Prerequisites

1. Build the project with testing enabled:
   ```bash
   cmake -B build -A Win32 -DBUILD_TESTING=ON
   cmake --build build --config Release
   ```

2. Ensure all required files are built:
   - `build/app_hook.dll` (main hook DLL)
   - `build/app_injector.exe` (injector executable)
   - `build/integration-test/test_app.exe` (test application)
   - `build/integration-test/tasks/test_plugin.dll` (test plugin)

### Run the Test

Navigate to the integration-test directory and run:

```powershell
.\run_tests.bat
```


### Expected Output

**Success (Test Passes):**
```
=========================================
    Integration Test for DLL Injection
=========================================

Checking required files...
✓ Found: build\integration-test\test_app.exe
✓ Found: build\app_hook.dll
✓ Found: build\integration-test\tasks\test_plugin.dll
✓ Found: build\app_injector.exe

Starting test application...
Test app started with PID: 12345

Running injector to inject main DLL and plugins...
Injection completed successfully!

Triggering task execution...
Task execution triggered successfully!

Waiting for test application to terminate...
✓ TEST PASSED!
  - DLL injection successful
  - Plugin loading successful  
  - Task execution successful
  - Termination signal received

Integration test completed!
```

**Failure (Test Fails):**
```
✗ TEST FAILED!
  Test app did not terminate within timeout
  This indicates plugin injection or task execution is not working
```

## Troubleshooting

### Test Fails - Check Logs
- Review `logs/app_hook.log` for detailed information about DLL loading and plugin initialization
- Look for error messages related to plugin loading or task execution

### Common Issues

1. **Missing Files**: Ensure all required files are built and in correct locations
2. **Architecture Mismatch**: Verify both test app and DLLs are built for the same architecture (x32)
3. **Dependencies**: Check that Visual C++ Redistributables are installed
4. **Permissions**: Run with sufficient privileges for DLL injection

### Manual Testing

You can also run components manually for debugging:

1. **Start test app manually**:
   ```bash
   build\integration-test\test_app.exe
   ```

2. **Run injector manually**:
   ```bash
   cd build
   app_injector.exe test_app.exe
   ```

3. **Check logs**: Review `logs/app_hook.log` after injection

## Event Names

The integration test uses the following Windows named events:

- `IntegrationTest_Terminate_{PID}`: Test app listens for termination signal
- `IntegrationTest_TriggerTasks_{PID}`: Main DLL listens for task execution trigger
- `FFScriptLoader_Ready_{PID}`: Standard synchronization event used by injector

Where `{PID}` is the process ID of the test application.

## Test Flow Diagram

```
Test Script
    │
    ├─► Start test_app.exe
    │   └─► Creates termination event
    │
    ├─► Run app_injector.exe
    │   ├─► Inject app_hook.dll
    │   ├─► Inject test_plugin.dll
    │   └─► Signal plugin readiness
    │
    ├─► Signal task execution trigger
    │   └─► Main DLL executes tasks manually
    │       └─► Test plugin sends termination signal
    │
    └─► Verify test app terminated (SUCCESS/FAIL)
```

This integration test provides confidence that the core DLL injection and plugin system is working correctly. 