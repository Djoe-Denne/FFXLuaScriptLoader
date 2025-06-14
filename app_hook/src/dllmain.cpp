#include <Windows.h>
#include <string>
#include "hook/hook_manager.hpp"
#include "hook/hook_factory.hpp"
#include "util/logger.hpp"

// Global hook manager
app_hook::hook::HookManager g_hook_manager;

void InstallHooks() {
    // Initialize logging first
    if (!app_hook::util::initialize_logging("logs/app_hook.log", spdlog::level::debug)) {
        MessageBoxA(NULL, "Failed to initialize logging system", "Logger Error", MB_OK);
        return;
    }
    
    LOG_INFO("================================");
    LOG_INFO("Application Hook DLL - InstallHooks thread started");
    LOG_INFO("Process ID: {}", GetCurrentProcessId());
    LOG_INFO("Thread ID: {}", GetCurrentThreadId());
    LOG_INFO("================================");
    
    // Add a small delay to ensure the process is fully loaded
    Sleep(100);
    
    LOG_INFO("Initializing hook system...");
    
    // Load configuration and create hooks
    const std::string tasks_config_path = "config/tasks.toml";
    LOG_INFO("Loading tasks configuration from: {}", tasks_config_path);
    
    if (auto result = app_hook::hook::HookFactory::create_hooks_from_tasks(tasks_config_path, g_hook_manager); !result) {
        LOG_ERROR("Failed to create hooks from configuration");
        MessageBoxA(NULL, "Failed to create hooks from configuration\nCheck logs/app_hook.log for details", "Config Error", MB_OK);
        return;
    }
    
    LOG_INFO("Created {} hook(s) with {} total task(s)", 
             g_hook_manager.hook_count(), g_hook_manager.total_task_count());
    
    // Install all hooks
    LOG_INFO("Installing hooks...");
    if (auto result = g_hook_manager.install_all(); !result) {
        LOG_ERROR("Failed to install hooks");
        MessageBoxA(NULL, "Failed to install hooks\nCheck logs/app_hook.log for details", "Hook Error", MB_OK);
        return;
    }
    
    const auto hook_count = g_hook_manager.hook_count();
    const auto task_count = g_hook_manager.total_task_count();
    
    LOG_INFO("Successfully installed {} hook(s) with {} task(s)", hook_count, task_count);
    
    const std::string success_msg = "Hooks installed successfully!\n" +
                                  std::to_string(hook_count) + " hook(s) with " +
                                  std::to_string(task_count) + " task(s)\n\n" +
                                  "Check logs/app_hook.log for detailed information.";
    MessageBoxA(NULL, success_msg.c_str(), "Success", MB_OK);
    
    LOG_INFO("InstallHooks thread completed successfully");
}

void UninstallHooks() {
    LOG_INFO("Uninstalling hooks...");
    g_hook_manager.uninstall_all();
    LOG_INFO("Application Hook DLL shutting down");
    
    // Shutdown logging system
    app_hook::util::shutdown_logging();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            // Quick logging initialization for DllMain logging
            app_hook::util::initialize_logging("logs/app_hook.log", spdlog::level::debug);
            
            LOG_INFO("DLL_PROCESS_ATTACH - DLL loaded into process");
            LOG_INFO("Module handle: 0x{:X}", reinterpret_cast<uintptr_t>(hModule));
            
            DisableThreadLibraryCalls(hModule);
            
            HANDLE thread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)InstallHooks, nullptr, 0, nullptr);
            if (thread == NULL) {
                LOG_ERROR("Failed to create InstallHooks thread, error: {}", GetLastError());
                MessageBoxA(NULL, "Failed to create hook installation thread", "Thread Error", MB_OK);
            } else {
                LOG_INFO("InstallHooks thread created successfully, handle: 0x{:X}", reinterpret_cast<uintptr_t>(thread));
                CloseHandle(thread); // We don't need to keep the handle
            }
            break;
        }
            
        case DLL_PROCESS_DETACH:
            LOG_INFO("DLL_PROCESS_DETACH - DLL being unloaded from process");
            UninstallHooks();
            break;
    }
    return TRUE;
}
