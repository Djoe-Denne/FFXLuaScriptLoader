#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <filesystem>
#include <algorithm>
#include <format>
#include <string_view>
#include <fstream>

namespace injector {

    /**
     * @brief Find process ID by name (case-insensitive)
     * @param processName Name of the process to find
     * @return Process ID or 0 if not found
     */
    [[nodiscard]] DWORD FindProcessId(std::string_view processName) {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            return 0;
        }

        PROCESSENTRY32 pe32{};
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (!Process32First(hSnapshot, &pe32)) {
            CloseHandle(hSnapshot);
            return 0;
        }

        DWORD processId = 0;
        do {
            std::string currentProcess = pe32.szExeFile;
            // Convert to lowercase for case-insensitive comparison
            std::transform(currentProcess.begin(), currentProcess.end(), currentProcess.begin(), ::tolower);
            
            std::string targetProcess{processName};
            std::transform(targetProcess.begin(), targetProcess.end(), targetProcess.begin(), ::tolower);

            if (currentProcess == targetProcess) {
                processId = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));

        CloseHandle(hSnapshot);
        return processId;
    }

    /**
     * @brief Check if a process is 64-bit
     * @param processId Process ID to check
     * @return true if 64-bit, false if 32-bit or error
     */
    [[nodiscard]] bool IsProcess64Bit(DWORD processId) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
        if (!hProcess) return false;
        
        BOOL isWow64 = FALSE;
        if (IsWow64Process(hProcess, &isWow64)) {
            CloseHandle(hProcess);
            // If IsWow64Process returns TRUE, the process is 32-bit running on 64-bit Windows
            // If it returns FALSE, the process matches the OS architecture
            #ifdef _WIN64
                return !isWow64;  // On 64-bit build: TRUE if not WOW64 (so it's 64-bit)
            #else
                return false;     // On 32-bit build: always return false (32-bit)
            #endif
        }
        
        CloseHandle(hProcess);
        return false;
    }

    /**
     * @brief Inject DLL into target process
     * @param processId Target process ID
     * @param dllPath Path to DLL to inject
     * @return true if successful, false otherwise
     */
    [[nodiscard]] bool InjectDLL(DWORD processId, std::string_view dllPath) {
        HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | 
                                     PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, 
                                     FALSE, processId);
        
        if (!hProcess) {
            std::cerr << std::format("Failed to open target process. Error: {}\n", GetLastError());
            return false;
        }

        // Allocate memory in target process for DLL path
        SIZE_T pathSize = dllPath.length() + 1;
        LPVOID pDllPath = VirtualAllocEx(hProcess, NULL, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        
        if (!pDllPath) {
            std::cerr << std::format("Failed to allocate memory in target process. Error: {}\n", GetLastError());
            CloseHandle(hProcess);
            return false;
        }

        // Write DLL path to allocated memory
        if (!WriteProcessMemory(hProcess, pDllPath, dllPath.data(), pathSize, NULL)) {
            std::cerr << std::format("Failed to write DLL path to target process. Error: {}\n", GetLastError());
            VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }

        // Get address of LoadLibraryA
        HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
        LPTHREAD_START_ROUTINE pLoadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
            GetProcAddress(hKernel32, "LoadLibraryA"));
        
        if (!pLoadLibrary) {
            std::cerr << std::format("Failed to get LoadLibraryA address. Error: {}\n", GetLastError());
            VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }

        // Create remote thread to load the DLL
        HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, pLoadLibrary, pDllPath, 0, NULL);
        
        if (!hThread) {
            std::cerr << std::format("Failed to create remote thread. Error: {}\n", GetLastError());
            VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }

        // Wait for thread to complete
        WaitForSingleObject(hThread, INFINITE);

        // Get thread exit code (should be the module handle if successful)
        DWORD exitCode;
        GetExitCodeThread(hThread, &exitCode);

        // Cleanup
        CloseHandle(hThread);
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);

        if (exitCode == 0) {
            std::cerr << "DLL injection failed - LoadLibrary returned NULL\n";
            std::cerr << "Possible causes:\n";
            std::cerr << "  1. DLL dependencies not found (e.g., Visual C++ Redistributables)\n";
            std::cerr << "  2. Architecture mismatch (32-bit vs 64-bit)\n";
            std::cerr << "  3. DLL has initialization errors\n";
            std::cerr << "  4. Insufficient privileges\n";
            
            // Try to load the DLL in our own process to get a better error
            HMODULE testLoad = LoadLibraryA(std::string{dllPath}.c_str());
            if (!testLoad) {
                DWORD loadError = GetLastError();
                std::cerr << std::format("  Test load in injector failed with error: {}\n", loadError);
                
                // Common error codes
                switch (loadError) {
                    case 126: // ERROR_MOD_NOT_FOUND
                        std::cerr << "  ERROR_MOD_NOT_FOUND: DLL or its dependencies not found\n";
                        break;
                    case 193: // ERROR_BAD_EXE_FORMAT  
                        std::cerr << "  ERROR_BAD_EXE_FORMAT: Architecture mismatch or corrupted file\n";
                        break;
                    case 1114: // ERROR_DLL_INIT_FAILED
                        std::cerr << "  ERROR_DLL_INIT_FAILED: DLL initialization failed\n";
                        break;
                    default:
                        std::cerr << std::format("  Unknown error code: {}\n", loadError);
                        break;
                }
            } else {
                std::cerr << "  DLL loads fine in injector process - target process issue\n";
                FreeLibrary(testLoad);
            }
            
            return false;
        }

        std::cout << std::format("LoadLibrary succeeded in target process (handle: 0x{:X})\n", exitCode);
        return true;
    }

    /**
     * @brief Display usage information
     * @param programName Name of the program executable
     */
    void ShowUsage(const char* programName) {
        std::cout << "Usage: " << programName << " <process_name> [dll_name] [--config-dir <path>] [--plugin-dir <path>]\n\n";
        std::cout << "Arguments:\n";
        std::cout << "  process_name  Name of the target process (e.g., myapp.exe)\n";
        std::cout << "  dll_name      Name of DLL to inject (default: app_hook.dll)\n\n";
        std::cout << "Options:\n";
        std::cout << "  --config-dir  Directory for configuration files (default: config)\n";
        std::cout << "  --plugin-dir  Directory for plugin tasks (default: mods/xtender/tasks)\n\n";
        std::cout << "Examples:\n";
        std::cout << "  " << programName << " myapp.exe\n";
        std::cout << "  " << programName << " game.exe custom_hook.dll\n";
        std::cout << "  " << programName << " app.exe app_hook.dll --config-dir custom_config --plugin-dir custom_plugins\n";
    }

    /**
     * @brief Parse command line arguments
     * @param argc Argument count
     * @param argv Argument values
     * @param processName [out] Target process name
     * @param dllName [out] DLL name to inject
     * @param configDir [out] Configuration directory path
     * @param pluginDir [out] Plugin directory path
     * @return true if parsing successful, false otherwise
     */
    bool ParseArguments(int argc, char* argv[], std::string& processName, std::string& dllName, 
                       std::string& configDir, std::string& pluginDir) {
        if (argc < 2) {
            return false;
        }
        
        processName = argv[1];
        dllName = "app_hook.dll";  // default
        configDir = "config";      // default
        pluginDir = "tasks";  // default

        // Parse remaining arguments
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            
            if (arg == "--config-dir" && i + 1 < argc) {
                configDir = argv[++i];
            }
            else if (arg == "--plugin-dir" && i + 1 < argc) {
                pluginDir = argv[++i];
            }
            else if (!arg.starts_with("--")) {
                // Assume it's the DLL name (for backward compatibility)
                dllName = arg;
            }
            else {
                std::cerr << std::format("Unknown option: {}\n", arg);
                return false;
            }
        }
        
        return true;
    }

    /**
     * @brief Write configuration paths to a temporary file for the DLL to read
     * @param configDir Configuration directory path
     * @param pluginDir Plugin directory path
     * @return true if successful, false otherwise
     */
    [[nodiscard]] bool WriteConfigFile(const std::string& configDir, const std::string& pluginDir) {
        try {
            // Create temp directory if it doesn't exist
            std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "ffscript_loader";
            std::filesystem::create_directories(tempDir);
            
            // Write configuration to temporary file
            std::filesystem::path configFile = tempDir / "injector_config.txt";
            std::ofstream outFile(configFile);
            
            if (!outFile.is_open()) {
                std::cerr << std::format("Failed to create config file: {}\n", configFile.string());
                return false;
            }
            
            outFile << "config_dir=" << configDir << "\n";
            outFile << "plugin_dir=" << pluginDir << "\n";
            outFile.close();
            
            std::cout << std::format("Configuration written to: {}\n", configFile.string());
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << std::format("Exception writing config file: {}\n", e.what());
            return false;
        }
    }

} // namespace injector

int main(int argc, char* argv[]) {
    using namespace injector;
    
    std::cout << "Generic DLL Injector for Legacy Software Extension\n";
    std::cout << "=================================================\n\n";

    // Parse command line arguments
    std::string processName;
    std::string dllName;
    std::string configDir;
    std::string pluginDir;

    if (!ParseArguments(argc, argv, processName, dllName, configDir, pluginDir)) {
        ShowUsage(argv[0]);
        system("pause");
        return 1;
    }

    // Get the current directory and construct DLL path
    std::filesystem::path currentPath = std::filesystem::current_path();
    std::filesystem::path dllPath = currentPath / dllName;
    
    // Convert to absolute path
    dllPath = std::filesystem::absolute(dllPath);
    std::string dllPathStr = dllPath.string();
    
    // Check if DLL exists
    if (!std::filesystem::exists(dllPath)) {
        std::cerr << std::format("Error: {} not found in current directory!\n", dllName);
        std::cerr << std::format("Expected path: {}\n", dllPathStr);
        system("pause");
        return 1;
    }
    
    std::cout << std::format("Target process: {}\n", processName);
    std::cout << std::format("DLL found at: {}\n", dllPathStr);
    std::cout << std::format("Config directory: {}\n", configDir);
    std::cout << std::format("Plugin directory: {}\n\n", pluginDir);

    std::cout << std::format("Looking for {} process...\n", processName);

    // Keep trying to find the process
    DWORD processId = 0;
    int attempts = 0;
    constexpr int maxAttempts = 30; // Wait up to 30 seconds

    while (processId == 0 && attempts < maxAttempts) {
        processId = FindProcessId(processName);
        if (processId == 0) {
            std::cout << std::format("Process not found, waiting... ({}/{})\n", attempts + 1, maxAttempts);
            Sleep(1000); // Wait 1 second
            attempts++;
        }
    }

    if (processId == 0) {
        std::cerr << std::format("Error: Could not find {} process after {} attempts!\n", processName, maxAttempts);
        std::cerr << "Make sure the target application is running.\n";
        system("pause");
        return 1;
    }

    std::cout << std::format("Found {} (PID: {})\n", processName, processId);
    
    // Check process architecture
    bool targetIs64Bit = IsProcess64Bit(processId);
    std::cout << std::format("Target process architecture: {}\n", targetIs64Bit ? "64-bit" : "32-bit");
    
    #ifdef _WIN64
        std::cout << "Injector architecture: 64-bit\n";
        if (!targetIs64Bit) {
            std::cerr << "ERROR: Architecture mismatch!\n";
            std::cerr << "Target process is 32-bit but injector is 64-bit.\n";
            std::cerr << "You need to build a 32-bit version of the injector and DLL.\n";
            system("pause");
            return 1;
        }
    #else
        std::cout << "Injector architecture: 32-bit\n";
        if (targetIs64Bit) {
            std::cerr << "ERROR: Architecture mismatch!\n";
            std::cerr << "Target process is 64-bit but injector is 32-bit.\n";
            std::cerr << "You need to build a 64-bit version of the injector and DLL.\n";
            system("pause");
            return 1;
        }
    #endif
    
    // Write configuration file for DLL to read
    std::cout << "Writing configuration file for DLL...\n";
    if (!WriteConfigFile(configDir, pluginDir)) {
        std::cerr << "\nFailed to write configuration file!\n";
        system("pause");
        return 1;
    }

    std::cout << std::format("\nInjecting DLL: {}\n", dllPathStr);

    if (!InjectDLL(processId, dllPathStr)) {
        std::cerr << "\nDLL injection failed!\n";
        system("pause");
        return 1;
    }

    std::cout << "\nDLL injection successful!\n";
    std::cout << "The injected DLL will handle plugin loading using its plugin manager.\n";
    std::cout << "Check logs/app_hook.log for detailed logs.\n";

    system("pause");
    return 0;
} 