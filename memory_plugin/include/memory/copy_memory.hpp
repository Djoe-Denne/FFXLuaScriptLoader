#pragma once

#include "../config/memory_config.hpp"
#include "../../core_hook/include/task/hook_task.hpp"
#include "../../core_hook/include/context/mod_context.hpp"

// Forward declaration
namespace app_hook::plugin { class IPluginHost; }

namespace app_hook::memory {

// Use the config structure from the config namespace
using CopyMemoryConfig = config::CopyMemoryConfig;

/// @brief Task that copies memory from one location to an expanded buffer
class CopyMemoryTask final : public task::IHookTask {
public:
    /// @brief Construct a memory copy task
    /// @param config Configuration for the copy operation
    explicit CopyMemoryTask(CopyMemoryConfig config) noexcept
        : config_(std::move(config)), host_(nullptr) {}
    
    /// @brief Set the plugin host for logging
    void setHost(app_hook::plugin::IPluginHost* host) { host_ = host; }
    
    /// @brief Set the plugin host for logging (base interface override)
    void setHost(void* host) override { 
        host_ = static_cast<app_hook::plugin::IPluginHost*>(host); 
    }
    
    /// @brief Execute the memory copy operation
    /// @return Task result indicating success or failure
    [[nodiscard]] task::TaskResult execute() override;
    
    /// @brief Get the task name
    /// @return Task name
    [[nodiscard]] std::string name() const override {
        return "CopyMemory";
    }
    
    /// @brief Get the task description
    /// @return Task description
    [[nodiscard]] std::string description() const override {
        return "Copy memory region '" + config_.key() + "' from " + 
               std::to_string(config_.original_size()) + " to " + 
               std::to_string(config_.new_size()) + " bytes";
    }
    

    
    /// @brief Get the configuration
    /// @return Copy memory configuration
    [[nodiscard]] const CopyMemoryConfig& config() const noexcept {
        return config_;
    }
    
private:
    CopyMemoryConfig config_;
    app_hook::plugin::IPluginHost* host_ = nullptr;
};

} // namespace app_hook::memory
