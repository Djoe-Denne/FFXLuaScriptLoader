#pragma once

#include <gmock/gmock.h>
#include "plugin/plugin_interface.hpp"
#include <vector>
#include <string>
#include <source_location>

/// @brief Mock plugin host for testing
class MockPluginHost : public app_hook::plugin::IPluginHost {
public:
    MockPluginHost();
    ~MockPluginHost() override = default;

    // Mock methods
    MOCK_METHOD(app_hook::plugin::PluginResult, register_config, 
                (std::unique_ptr<app_hook::config::ConfigBase> config), (override));
    
    MOCK_METHOD(app_hook::plugin::PluginResult, register_config_loader, 
                (std::unique_ptr<app_hook::config::ConfigLoaderBase> loader), (override));
    
    MOCK_METHOD(app_hook::plugin::PluginResult, register_task_creator, 
                (const std::string& config_type_name, 
                 std::function<std::unique_ptr<app_hook::task::IHookTask>(const app_hook::config::ConfigBase&)> creator), 
                (override));
    
    MOCK_METHOD(std::string, get_plugin_data_path, (), (const, override));
    
    MOCK_METHOD((std::pair<void*, std::uint32_t>), get_process_info, (), (const, override));
    
    MOCK_METHOD(app_hook::context::ModContext&, get_mod_context, (), (override));

    // Non-mock implementations for logging (need to capture messages)
    void log_message(int level, const std::string& message) override;
    void log_message_with_location(int level, const std::string& message, const std::source_location& location) override;

    // Test utilities
    const std::vector<std::pair<int, std::string>>& get_log_messages() const;
    void clear_log_messages();
    void set_data_path(const std::string& path);

private:
    std::vector<std::pair<int, std::string>> log_messages_;
    std::string data_path_;
}; 