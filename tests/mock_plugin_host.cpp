#include "mock_plugin_host.hpp"
#include "context/mod_context.hpp"
#include <iostream>
#include <windows.h>

MockPluginHost::MockPluginHost() : data_path_("test_data/") {
    // Initialize with default mock behavior
    ON_CALL(*this, register_config(::testing::_))
        .WillByDefault(::testing::Return(app_hook::plugin::PluginResult::Success));
        
    ON_CALL(*this, register_config_loader(::testing::_))
        .WillByDefault(::testing::Return(app_hook::plugin::PluginResult::Success));
        
    ON_CALL(*this, register_task_creator(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(app_hook::plugin::PluginResult::Success));
        
    ON_CALL(*this, get_plugin_data_path())
        .WillByDefault(::testing::Return(data_path_));
        
    ON_CALL(*this, get_process_info())
        .WillByDefault(::testing::Return(std::make_pair(GetCurrentProcess(), GetCurrentProcessId())));
        
    ON_CALL(*this, get_mod_context())
        .WillByDefault(::testing::ReturnRef(app_hook::context::ModContext::instance()));
}

void MockPluginHost::log_message(int level, const std::string& message) {
    log_messages_.emplace_back(level, message);
    // Also output to console for debugging
    std::cout << "[MOCK LOG " << level << "] " << message << std::endl;
}

void MockPluginHost::log_message_with_location(int level, const std::string& message, const std::source_location& location) {
    std::string full_message = message + " [" + location.file_name() + ":" + std::to_string(location.line()) + "]";
    log_message(level, full_message);
}

const std::vector<std::pair<int, std::string>>& MockPluginHost::get_log_messages() const {
    return log_messages_;
}

void MockPluginHost::clear_log_messages() {
    log_messages_.clear();
}

void MockPluginHost::set_data_path(const std::string& path) {
    data_path_ = path;
} 