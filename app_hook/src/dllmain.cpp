#include <Windows.h>
#include <string>
#include <fstream>
#include <filesystem>
#include <hook/hook_manager.hpp>
#include <hook/hook_factory.hpp>
#include <util/logger.hpp>
#include <plugin/plugin_manager.hpp>

// Global hook manager
app_hook::hook::HookManager g_hook_manager;

// Global plugin manager
app_hook::plugin::PluginManager g_plugin_manager;

/**
 * @brief Read configuration from injector config file
 * @param configDir [out] Configuration directory path
 * @param pluginDir [out] Plugin directory path
 * @return true if config file was read successfully, false otherwise
 */
bool ReadInjectorConfig(std::string& configDir, std::string& pluginDir) {
    try {
        // Look for config file in temp directory
        std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "ffscript_loader";
        std::filesystem::path configFile = tempDir / "injector_config.txt";
        
        if (!std::filesystem::exists(configFile)) {
            return false;
        }
        
        std::ifstream inFile(configFile);
        if (!inFile.is_open()) {
            return false;
        }
        
        std::string line;
        while (std::getline(inFile, line)) {
            size_t equalPos = line.find('=');
            if (equalPos != std::string::npos) {
                std::string key = line.substr(0, equalPos);
                std::string value = line.substr(equalPos + 1);
                
                if (key == "config_dir") {
                    configDir = value;
                } else if (key == "plugin_dir") {
                    pluginDir = value;
                }
            }
        }
        
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

/**
 * @brief Get configuration directory from injector config or use default
 * @return Configuration directory path
 */
std::string GetConfigDirectory() {
    std::string configDir, pluginDir;
    if (ReadInjectorConfig(configDir, pluginDir) && !configDir.empty()) {
        return configDir;
    } else {
        // Fall back to default
        return "config";
    }
}

/**
 * @brief Get plugin directory from injector config or use default
 * @return Plugin directory path
 */
std::string GetPluginDirectory() {
    std::string configDir, pluginDir;
    if (ReadInjectorConfig(configDir, pluginDir) && !pluginDir.empty()) {
        return pluginDir;
    } else {
        // Fall back to default
        return "tasks";
    }
}

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
    
    // Get configuration paths from injector config file or defaults
    const std::string config_dir = GetConfigDirectory();
    const std::string plugin_dir = GetPluginDirectory();
    
    // Check if configuration was loaded from injector
    std::string injectorConfigDir, injectorPluginDir;
    bool configFromInjector = ReadInjectorConfig(injectorConfigDir, injectorPluginDir);
    
    if (configFromInjector) {
        LOG_INFO("Configuration loaded from injector:");
        LOG_INFO("  Config directory: {} (from injector)", config_dir);
        LOG_INFO("  Plugin directory: {} (from injector)", plugin_dir);
    } else {
        LOG_INFO("Using default configuration (no injector config found):");
        LOG_INFO("  Config directory: {} (default)", config_dir);
        LOG_INFO("  Plugin directory: {} (default)", plugin_dir);
    }
    
    // Initialize plugin system
    LOG_INFO("Initializing plugin system...");
    auto config_registry = [&](std::shared_ptr<app_hook::config::ConfigBase> config) {
        // Register configuration with the hook system
        // This would typically be integrated with the existing configuration system
        LOG_INFO("Plugin registered configuration: {} ({})", config->key(), config->name());
    };
    
    try {
        LOG_INFO("About to initialize plugin manager...");
        if (auto result = g_plugin_manager.initialize("data", std::move(config_registry)); result != app_hook::plugin::PluginResult::Success) {
            LOG_ERROR("Failed to initialize plugin manager with result: {}", static_cast<int>(result));
            MessageBoxA(NULL, "Failed to initialize plugin manager\nCheck logs/app_hook.log for details", "Plugin Error", MB_OK);
            return;
        }
        LOG_INFO("Plugin manager initialized successfully");
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception during plugin manager initialization: {}", e.what());
        MessageBoxA(NULL, "Exception during plugin manager initialization\nCheck logs/app_hook.log for details", "Plugin Error", MB_OK);
        return;
    }
    catch (...) {
        LOG_ERROR("Unknown exception during plugin manager initialization");
        MessageBoxA(NULL, "Unknown exception during plugin manager initialization\nCheck logs/app_hook.log for details", "Plugin Error", MB_OK);
        return;
    }
    
    // Load plugins from tasks directory
    LOG_INFO("Loading plugins from directory: {}/", plugin_dir);
    std::size_t loaded_plugins = 0;
    
    try {
        LOG_INFO("About to call load_plugins_from_directory...");
        loaded_plugins = g_plugin_manager.load_plugins_from_directory(plugin_dir);
        LOG_INFO("load_plugins_from_directory returned: {}", loaded_plugins);
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception during plugin loading: {}", e.what());
        MessageBoxA(NULL, "Exception during plugin loading\nCheck logs/app_hook.log for details", "Plugin Error", MB_OK);
        return;
    }
    catch (...) {
        LOG_ERROR("Unknown exception during plugin loading");
        MessageBoxA(NULL, "Unknown exception during plugin loading\nCheck logs/app_hook.log for details", "Plugin Error", MB_OK);
        return;
    }
    
    LOG_INFO("Loaded {} plugin(s) successfully", loaded_plugins);
    
    // Initialize plugins with configuration
    LOG_INFO("About to initialize plugins with configuration...");
    try {
        if (auto result = g_plugin_manager.initialize_plugins(config_dir); result != app_hook::plugin::PluginResult::Success) {
            LOG_ERROR("Failed to initialize plugins with result: {}", static_cast<int>(result));
            MessageBoxA(NULL, "Failed to initialize plugins\nCheck logs/app_hook.log for details", "Plugin Error", MB_OK);
            return;
        }
        LOG_INFO("Plugins initialized successfully");
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception during plugin initialization: {}", e.what());
        MessageBoxA(NULL, "Exception during plugin initialization\nCheck logs/app_hook.log for details", "Plugin Error", MB_OK);
        return;
    }
    catch (...) {
        LOG_ERROR("Unknown exception during plugin initialization");
        MessageBoxA(NULL, "Unknown exception during plugin initialization\nCheck logs/app_hook.log for details", "Plugin Error", MB_OK);
        return;
    }
    
    // Load configuration and create hooks
    const std::string tasks_config_path = config_dir + "/tasks.toml";
    LOG_INFO("Loading tasks configuration from: {}", tasks_config_path);
    
    try {
        LOG_INFO("About to create hooks from configuration...");
        if (auto result = app_hook::hook::HookFactory::create_hooks_from_tasks(tasks_config_path, g_hook_manager); !result) {
            LOG_ERROR("Failed to create hooks from configuration");
            MessageBoxA(NULL, "Failed to create hooks from configuration\nCheck logs/app_hook.log for details", "Config Error", MB_OK);
            return;
        }
        LOG_INFO("Hooks created successfully from configuration");
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception during hook creation: {}", e.what());
        MessageBoxA(NULL, "Exception during hook creation\nCheck logs/app_hook.log for details", "Config Error", MB_OK);
        return;
    }
    catch (...) {
        LOG_ERROR("Unknown exception during hook creation");
        MessageBoxA(NULL, "Unknown exception during hook creation\nCheck logs/app_hook.log for details", "Config Error", MB_OK);
        return;
    }
    
    LOG_INFO("Created {} hook(s) with {} total task(s)", 
             g_hook_manager.hook_count(), g_hook_manager.total_task_count());
    
    // Install all hooks
    LOG_INFO("Installing hooks...");
    try {
        if (auto result = g_hook_manager.install_all(); !result) {
            LOG_ERROR("Failed to install hooks");
            MessageBoxA(NULL, "Failed to install hooks\nCheck logs/app_hook.log for details", "Hook Error", MB_OK);
            return;
        }
        LOG_INFO("Hooks installed successfully");
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception during hook installation: {}", e.what());
        MessageBoxA(NULL, "Exception during hook installation\nCheck logs/app_hook.log for details", "Hook Error", MB_OK);
        return;
    }
    catch (...) {
        LOG_ERROR("Unknown exception during hook installation");
        MessageBoxA(NULL, "Unknown exception during hook installation\nCheck logs/app_hook.log for details", "Hook Error", MB_OK);
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
            try {
                // Quick logging initialization for DllMain logging
                app_hook::util::initialize_logging("logs/app_hook.log", 1);  // 1 = debug level
                
                LOG_INFO("DLL_PROCESS_ATTACH - DLL loaded into process");
                LOG_INFO("Module handle: 0x{:X}", reinterpret_cast<uintptr_t>(hModule));
                
                DisableThreadLibraryCalls(hModule);
                
                LOG_INFO("About to create InstallHooks thread...");
                HANDLE thread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)InstallHooks, nullptr, 0, nullptr);
                if (thread == NULL) {
                    DWORD error = GetLastError();
                    LOG_ERROR("Failed to create InstallHooks thread, error: {}", error);
                    MessageBoxA(NULL, "Failed to create hook installation thread", "Thread Error", MB_OK);
                } else {
                    LOG_INFO("InstallHooks thread created successfully, handle: 0x{:X}", reinterpret_cast<uintptr_t>(thread));
                    CloseHandle(thread); // We don't need to keep the handle
                }
            }
            catch (const std::exception& e) {
                MessageBoxA(NULL, ("Exception in DLL_PROCESS_ATTACH: " + std::string(e.what())).c_str(), "DLL Error", MB_OK);
                return FALSE;
            }
            catch (...) {
                MessageBoxA(NULL, "Unknown exception in DLL_PROCESS_ATTACH", "DLL Error", MB_OK);
                return FALSE;
            }
            break;
        }
            
        case DLL_PROCESS_DETACH:
            try {
                LOG_INFO("DLL_PROCESS_DETACH - DLL being unloaded from process");
                UninstallHooks();
            }
            catch (const std::exception& e) {
                // Log the error but don't show message box during shutdown
                try {
                    LOG_ERROR("Exception in DLL_PROCESS_DETACH: {}", e.what());
                } catch (...) {
                    // Ignore logging errors during shutdown
                }
            }
            catch (...) {
                // Log the error but don't show message box during shutdown
                try {
                    LOG_ERROR("Unknown exception in DLL_PROCESS_DETACH");
                } catch (...) {
                    // Ignore logging errors during shutdown
                }
            }
            break;
    }
    return TRUE;
}
