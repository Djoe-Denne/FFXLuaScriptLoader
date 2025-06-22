#pragma once

#include "../config/load_in_memory_config.hpp"
#include "../memory/memory_region.hpp"
#include "../../core_hook/include/task/hook_task.hpp"
#include "../../core_hook/include/context/mod_context.hpp"

// Forward declaration
namespace app_hook::plugin { class IPluginHost; }

namespace app_hook::memory {

// Use the config structure from the config namespace
using LoadInMemoryConfig = config::LoadInMemoryConfig;

/// @brief Task that loads binary data into memory
class LoadInMemoryTask final : public task::IHookTask {
public:
    /// @brief Construct a load in memory task
    /// @param config Configuration for the load operation
    explicit LoadInMemoryTask(LoadInMemoryConfig config) noexcept
        : config_(std::move(config)), host_(nullptr) {}
    
    /// @brief Set the plugin host for logging
    void setHost(app_hook::plugin::IPluginHost* host) { host_ = host; }
    
    /// @brief Set the plugin host for logging (base interface override)
    void setHost(void* host) override { 
        host_ = static_cast<app_hook::plugin::IPluginHost*>(host); 
    }
    
    /// @brief Execute the load in memory operation
    /// @return Task result indicating success or failure
    [[nodiscard]] task::TaskResult execute() override;
    
    /// @brief Get the task name
    /// @return Task name
    [[nodiscard]] std::string name() const override {
        return "LoadInMemory";
    }
    
    /// @brief Get the task description
    /// @return Task description
    [[nodiscard]] std::string description() const override {
        return "Load binary data '" + config_.key() + "' from file: " + config_.binary_path();
    }
    
    /// @brief Get the configuration
    /// @return Load in memory configuration
    [[nodiscard]] const LoadInMemoryConfig& config() const noexcept {
        return config_;
    }
    
private:
    LoadInMemoryConfig config_;                ///< Configuration for the load operation
    app_hook::plugin::IPluginHost* host_;      ///< Plugin host for logging and context access
};

} // namespace app_hook::memory 