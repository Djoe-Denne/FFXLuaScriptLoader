# How to Run Integration Test

## Quick Start

1. **Build the project**:
   ```bash
   cmake -B build -A Win32 -DBUILD_TESTING=ON
   cmake --build build --config Release
   ```

2. **Run the integration test**:
   ```bash
   cd build/integration-test
   .\run_test.bat
   ```

That's it! The test will:
- Start the test application
- Inject the main DLL and plugins
- Trigger task execution
- Verify the plugin system works

## What Happens

- ✅ **Test PASSED** = DLL injection and plugin system working
- ❌ **Test FAILED** = Something is broken, check logs

## Troubleshooting

If the test fails:
1. Check `logs/app_hook.log` for detailed error messages
2. Make sure you built in Release mode
3. Ensure Visual C++ Redistributables are installed
4. Try running as Administrator if permission issues

## Manual Test

You can also run the test manually:
```bash
cd build/integration-test
.\run_integration_test.ps1
```

Or run components individually for debugging:
```bash
# Start test app
.\test_app.exe

# In another terminal, inject DLL
.\app_injector.exe test_app.exe
``` 