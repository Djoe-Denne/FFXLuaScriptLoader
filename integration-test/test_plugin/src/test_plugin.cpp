#include <plugin/plugin_interface.hpp>
#include <config/config_base.hpp>
#include <config/config_loader_base.hpp>
#include <task/hook_task.hpp>
#include <Windows.h>
#include <string>
#include <memory>
#include <vector>
#include <format>

using namespace app_hook::plugin;
using namespace app_hook::config;
using namespace app_hook::task;

// Test configuration class
class TestConfig : public ConfigBase {
public:
    TestConfig() : ConfigBase(ConfigType::Unknown, "test_termination", "Integration Test Termination Config"), 
                   target_process_id_(0) {
        set_description("Configuration for integration test termination signal");
    }

    void set_target_process_id(DWORD pid) {
        target_process_id_ = pid;
    }

    [[nodiscard]] DWORD get_target_process_id() const {
        return target_process_id_;
    }

private:
    DWORD target_process_id_;
};

// Test task that sends termination signal
class TerminationTask : public IHookTask {
public:
    explicit TerminationTask(std::shared_ptr<TestConfig> config)
        : config_(std::move(config)) {}

    [[nodiscard]] TaskResult execute() override {
        if (!config_) {
            return std::unexpected(TaskError::invalid_config);
        }

        DWORD pid = config_->get_target_process_id();
        if (pid == 0) {
            pid = GetCurrentProcessId(); // Fallback to current process
        }

        std::string eventName = std::format("IntegrationTest_Terminate_{}", pid);
        
        // Open the termination event
        HANDLE hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, eventName.c_str());
        if (!hEvent) {
            // Event doesn't exist yet, which might be normal during startup
            return std::unexpected(TaskError::dependency_not_met);
        }

        // Signal the termination event
        if (!SetEvent(hEvent)) {
            CloseHandle(hEvent);
            return std::unexpected(TaskError::unknown_error);
        }

        CloseHandle(hEvent);
        return {}; // Success
    }

    [[nodiscard]] std::string name() const override {
        return "TerminationTask";
    }

    [[nodiscard]] std::string description() const override {
        return "Task that sends termination signal to test application";
    }

private:
    std::shared_ptr<TestConfig> config_;
};

// Test configuration loader
class TestConfigLoader : public ConfigLoaderBase {
public:
    [[nodiscard]] std::vector<ConfigType> supported_types() const override {
        return {ConfigType::Unknown};
    }

    [[nodiscard]] ConfigResult<std::vector<ConfigPtr>> load_configs(
        ConfigType type, 
        const std::string& file_path, 
        const std::string& task_name) override {
        
        (void)type; (void)file_path; (void)task_name; // Suppress unused parameter warnings
        
        // For integration test, we create a simple test config
        // In a real implementation, this would parse the TOML file
        auto config = std::make_shared<TestConfig>();
        config->set_target_process_id(GetCurrentProcessId());

        std::vector<ConfigPtr> configs;
        configs.push_back(config);
        
        return configs;
    }

    [[nodiscard]] std::string get_name() const override {
        return "TestConfigLoader";
    }

    [[nodiscard]] std::string get_version() const override {
        return "1.0.0";
    }
};

// Main test plugin class
class TestPlugin : public IPlugin {
public:
    TestPlugin() : host_(nullptr) {}

    [[nodiscard]] PluginInfo get_plugin_info() const override {
        return {
            "Integration Test Plugin",
            "1.0.0",
            "Plugin for testing DLL injection and plugin loading",
            PLUGIN_API_VERSION
        };
    }

    [[nodiscard]] PluginResult initialize(IPluginHost* host) override {
        if (!host) {
            return PluginResult::Failed;
        }

        host_ = host;
        
        PLUGIN_LOG_INFO("TestPlugin initializing...");
        
        // Register our configuration loader
        auto loader = std::make_unique<TestConfigLoader>();
        if (auto result = host_->register_config_loader(std::move(loader)); 
            result != PluginResult::Success) {
            PLUGIN_LOG_ERROR("Failed to register config loader");
            return result;
        }

        PLUGIN_LOG_INFO("TestPlugin initialized successfully");
        return PluginResult::Success;
    }

    [[nodiscard]] PluginResult load_configurations(const std::string& config_path) override {
        if (!host_) {
            return PluginResult::Failed;
        }

        PLUGIN_LOG_INFO("Loading test configuration from: {}", config_path);

        // Create and register test configuration
        auto test_config_raw = new TestConfig();
        test_config_raw->set_target_process_id(GetCurrentProcessId());

        // Store the config for task creation (keep a reference)
        test_config_ = std::shared_ptr<TestConfig>(test_config_raw);

        // Create a unique_ptr for registration (copy constructor should work)
        auto config_for_registration = std::make_unique<TestConfig>(*test_config_raw);

        if (auto result = host_->register_config(std::move(config_for_registration)); 
            result != PluginResult::Success) {
            PLUGIN_LOG_ERROR("Failed to register test configuration");
            return result;
        }

        PLUGIN_LOG_INFO("Test configuration loaded successfully");
        return PluginResult::Success;
    }

    void shutdown() override {
        if (host_) {
            PLUGIN_LOG_INFO("TestPlugin shutting down");
        }
        host_ = nullptr;
    }

private:
    IPluginHost* host_;
    std::shared_ptr<TestConfig> test_config_;
};

// Export the plugin
extern "C" {
    PLUGIN_EXPORT app_hook::plugin::IPlugin* CreatePlugin() {
        return new TestPlugin();
    }
    
    PLUGIN_EXPORT void DestroyPlugin(app_hook::plugin::IPlugin* plugin) {
        delete plugin;
    }
} 