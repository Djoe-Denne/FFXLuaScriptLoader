#include <Windows.h>
#include <string>
#include <hook/hook_manager.hpp>
#include <hook/hook_factory.hpp>
#include <util/logger.hpp>
#include <plugin/plugin_manager.hpp>

// Global hook manager
app_hook::hook::HookManager g_hook_manager;

// Global plugin manager
app_hook::plugin::PluginManager g_plugin_manager;

void InstallHooks() {
    // Initialize logging first
    if (!app_hook::util::initialize_logging("logs/app_hook.log", 1)) {  // 1 = debug level
        MessageBoxA(NULL, "Failed to initialize logging system", "Logger Error", MB_OK);
        return;
    }
    
    LOG_INFO("================================");
    LOG_INFO("Application Hook DLL - InstallHooks thread started");
    LOG_INFO("Process ID: {}", GetCurrentProcessId());
    LOG_INFO("Thread ID: {}", GetCurrentThreadId());
    LOG_INFO("================================");
    
    LOG_INFO("Initializing hook system...");
    
    // Initialize plugin system
    LOG_INFO("Initializing plugin system...");
    auto config_registry = [&](std::shared_ptr<app_hook::config::ConfigBase> config) {
        // Register configuration with the hook system
        // This would typically be integrated with the existing configuration system
        LOG_INFO("Plugin registered configuration: {} ({})", config->key(), config->name());
    };
    
    if (auto result = g_plugin_manager.initialize("data", std::move(config_registry)); result != app_hook::plugin::PluginResult::Success) {
        LOG_ERROR("Failed to initialize plugin manager");
        MessageBoxA(NULL, "Failed to initialize plugin manager\nCheck logs/app_hook.log for details", "Plugin Error", MB_OK);
        return;
    }
    
    // Load plugins from tasks directory
    LOG_INFO("Loading plugins from directory: mods/xtender/tasks/");
    const auto loaded_plugins = g_plugin_manager.load_plugins_from_directory("mods/xtender/tasks");
    LOG_INFO("Loaded {} plugin(s)", loaded_plugins);
    
    // Initialize plugins with configuration
    if (auto result = g_plugin_manager.initialize_plugins("config"); result != app_hook::plugin::PluginResult::Success) {
        LOG_ERROR("Failed to initialize plugins");
        MessageBoxA(NULL, "Failed to initialize plugins\nCheck logs/app_hook.log for details", "Plugin Error", MB_OK);
        return;
    }
    
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
    
    // Unload plugins
    LOG_INFO("Unloading plugins...");
    g_plugin_manager.unload_all_plugins();
    
    LOG_INFO("Application Hook DLL shutting down");
    
    // Shutdown logging system
    app_hook::util::shutdown_logging();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            // Quick logging initialization for DllMain logging
            app_hook::util::initialize_logging("logs/app_hook.log", 1);  // 1 = debug level
            
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
