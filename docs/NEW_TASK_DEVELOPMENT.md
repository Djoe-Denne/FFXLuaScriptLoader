# New Task Development Guide

A comprehensive guide for developing new tasks, configurations, and integrating them into the FF8 Hook system following C++23 conventions.

## Table of Contents

1. [System Architecture Overview](#system-architecture-overview)
2. [Creating a New Configuration Class](#creating-a-new-configuration-class)
3. [Creating a New Task Implementation](#creating-a-new-task-implementation)
4. [Configuration Loader Development](#configuration-loader-development)
5. [Integration with Task System](#integration-with-task-system)
6. [Testing Your Implementation](#testing-your-implementation)
7. [Best Practices and Conventions](#best-practices-and-conventions)
8. [Example: Complete Audio Task Implementation](#example-complete-audio-task-implementation)

## System Architecture Overview

The FF8 Hook system follows a modern C++23 architecture with these key components:

- **ConfigBase**: Polymorphic base class for all configurations
- **IHookTask**: Interface for all task implementations  
- **TaskLoader**: Orchestrates loading from tasks.toml
- **ConfigFactory**: Creates configurations based on type
- **HookManager**: Manages hook lifecycle and task execution

### Task System Flow

```cpp
tasks.toml → TaskLoader → ConfigFactory → SpecificLoader → ConfigClass → TaskClass → HookManager
```

## Creating a New Configuration Class

### Step 1: Define Your Configuration Type

Add your new type to the `ConfigType` enum in `config_base.hpp`:

```cpp
enum class ConfigType : std::uint8_t {
    Unknown = 0,
    Memory,     ///< Memory expansion configuration
    Patch,      ///< Instruction patch configuration
    Script,     ///< Script configuration
    Audio,      ///< Audio configuration  
    Graphics,   ///< Graphics configuration
    Network     ///< NEW: Network configuration
};
```

Update the conversion functions:

```cpp
[[nodiscard]] constexpr std::string_view to_string(ConfigType type) noexcept {
    switch (type) {
        case ConfigType::Memory:   return "memory";
        case ConfigType::Patch:    return "patch";
        case ConfigType::Script:   return "script";
        case ConfigType::Audio:    return "audio";
        case ConfigType::Graphics: return "graphics";
        case ConfigType::Network:  return "network";    // NEW
        case ConfigType::Unknown:  
        default:                   return "unknown";
    }
}

[[nodiscard]] constexpr ConfigType from_string(std::string_view type_str) noexcept {
    if (type_str == "memory")   return ConfigType::Memory;
    if (type_str == "patch")    return ConfigType::Patch;
    if (type_str == "script")   return ConfigType::Script;
    if (type_str == "audio")    return ConfigType::Audio;
    if (type_str == "graphics") return ConfigType::Graphics;
    if (type_str == "network")  return ConfigType::Network;  // NEW
    return ConfigType::Unknown;
}
```

### Step 2: Create Your Configuration Header

Create `include/config/network_config.hpp`:

```cpp
#pragma once

#include "config_base.hpp"
#include <string>
#include <cstdint>
#include <vector>

namespace ff8_hook::config {

/// @brief Configuration for network operations
class NetworkConfig : public ConfigBase {
public:
    /// @brief Constructor
    /// @param key Configuration key
    /// @param name Display name
    NetworkConfig(std::string key, std::string name)
        : ConfigBase(ConfigType::Network, std::move(key), std::move(name))
        , server_address_{}
        , port_(0)
        , timeout_ms_(5000)
        , retry_count_(3)
        , use_ssl_(false) {}

    /// @brief Copy constructor
    NetworkConfig(const NetworkConfig& other)
        : ConfigBase(ConfigType::Network, other.key(), other.name())
        , server_address_(other.server_address_)
        , port_(other.port_)
        , timeout_ms_(other.timeout_ms_)
        , retry_count_(other.retry_count_)
        , use_ssl_(other.use_ssl_) {
        set_description(other.description());
        set_enabled(other.enabled());
    }

    /// @brief Default destructor
    ~NetworkConfig() override = default;

    // Accessors (C++23 conventions)
    [[nodiscard]] const std::string& server_address() const noexcept { return server_address_; }
    [[nodiscard]] constexpr std::uint16_t port() const noexcept { return port_; }
    [[nodiscard]] constexpr std::uint32_t timeout_ms() const noexcept { return timeout_ms_; }
    [[nodiscard]] constexpr std::uint8_t retry_count() const noexcept { return retry_count_; }
    [[nodiscard]] constexpr bool use_ssl() const noexcept { return use_ssl_; }

    // Mutators
    void set_server_address(std::string address) { server_address_ = std::move(address); }
    void set_port(std::uint16_t port) noexcept { port_ = port; }
    void set_timeout_ms(std::uint32_t timeout) noexcept { timeout_ms_ = timeout; }
    void set_retry_count(std::uint8_t count) noexcept { retry_count_ = count; }
    void set_use_ssl(bool use_ssl) noexcept { use_ssl_ = use_ssl; }

    /// @brief Check if this configuration is valid
    /// @return True if all required fields are properly set
    [[nodiscard]] bool is_valid() const noexcept override {
        return ConfigBase::is_valid() && 
               !server_address_.empty() && 
               port_ > 0 && 
               timeout_ms_ > 0;
    }

    /// @brief Get debug string representation
    /// @return Debug string
    [[nodiscard]] std::string debug_string() const override {
        return ConfigBase::debug_string() + 
               " server=" + server_address_ + 
               " port=" + std::to_string(port_) +
               " ssl=" + (use_ssl_ ? "true" : "false");
    }

private:
    std::string server_address_;
    std::uint16_t port_;
    std::uint32_t timeout_ms_;
    std::uint8_t retry_count_;
    bool use_ssl_;
};

} // namespace ff8_hook::config
```

## Creating a New Task Implementation

### Step 1: Create Your Task Header

Create `include/network/network_task.hpp`:

```cpp
#pragma once

#include "../task/hook_task.hpp"
#include "../context/mod_context.hpp"
#include "../util/logger.hpp"
#include "../config/network_config.hpp"
#include <string>
#include <future>
#include <chrono>

namespace ff8_hook::network {

// Use the config structure from the config namespace
using NetworkConfig = config::NetworkConfig;

/// @brief Task that performs network operations
class NetworkTask final : public task::IHookTask {
public:
    /// @brief Construct a network task
    /// @param config Configuration for the network operation
    explicit NetworkTask(NetworkConfig config) noexcept
        : config_(std::move(config)) {}

    /// @brief Execute the network operation
    /// @return Task result indicating success or failure
    [[nodiscard]] task::TaskResult execute() override {
        LOG_DEBUG("Executing NetworkTask for key '{}'", config_.key());
        LOG_DEBUG("Connecting to {}:{}", config_.server_address(), config_.port());

        if (!config_.is_valid()) {
            LOG_ERROR("Invalid configuration for NetworkTask '{}'", config_.key());
            return std::unexpected(task::TaskError::invalid_config);
        }

        try {
            // Perform network operation with timeout
            auto future = std::async(std::launch::async, [this]() {
                return perform_network_operation();
            });

            const auto timeout = std::chrono::milliseconds(config_.timeout_ms());
            if (future.wait_for(timeout) == std::future_status::timeout) {
                LOG_ERROR("Network operation timed out for NetworkTask '{}'", config_.key());
                return std::unexpected(task::TaskError::dependency_not_met);
            }

            const auto result = future.get();
            if (!result) {
                LOG_ERROR("Network operation failed for NetworkTask '{}'", config_.key());
                return std::unexpected(task::TaskError::unknown_error);
            }

            LOG_INFO("Successfully executed NetworkTask for key '{}'", config_.key());
            return {};

        } catch (const std::exception& e) {
            LOG_ERROR("Exception in NetworkTask '{}': {}", config_.key(), e.what());
            return std::unexpected(task::TaskError::unknown_error);
        }
    }

    /// @brief Get the task name
    /// @return Task name
    [[nodiscard]] std::string name() const override {
        return "Network";
    }

    /// @brief Get the task description
    /// @return Task description
    [[nodiscard]] std::string description() const override {
        return "Network operation for '" + config_.key() + "' to " + 
               config_.server_address() + ":" + std::to_string(config_.port());
    }

    /// @brief Get the configuration
    /// @return Network configuration
    [[nodiscard]] const NetworkConfig& config() const noexcept {
        return config_;
    }

private:
    NetworkConfig config_;

    /// @brief Perform the actual network operation
    /// @return True if successful
    [[nodiscard]] bool perform_network_operation() {
        std::uint8_t attempts = 0;
        
        while (attempts < config_.retry_count()) {
            try {
                LOG_DEBUG("Network attempt {} for '{}'", attempts + 1, config_.key());
                
                // Your actual network implementation here
                // For example: HTTP request, TCP connection, etc.
                
                return true; // Success
                
            } catch (const std::exception& e) {
                LOG_WARNING("Network attempt {} failed for '{}': {}", 
                           attempts + 1, config_.key(), e.what());
                ++attempts;
                
                if (attempts < config_.retry_count()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
            }
        }
        
        return false; // All attempts failed
    }
};

} // namespace ff8_hook::network
```

## Configuration Loader Development

### Step 1: Create Your Loader Header

Create `include/config/network_loader.hpp`:

```cpp
#pragma once

#include "config_common.hpp"
#include "network_config.hpp"
#include <toml++/toml.h>
#include <vector>
#include <string>

namespace ff8_hook::config {

/// @brief Loader for network configuration files
class NetworkLoader {
public:
    NetworkLoader() = default;
    ~NetworkLoader() = default;
    
    // Non-copyable but movable (C++23 best practices)
    NetworkLoader(const NetworkLoader&) = delete;
    NetworkLoader& operator=(const NetworkLoader&) = delete;
    NetworkLoader(NetworkLoader&&) = default;
    NetworkLoader& operator=(NetworkLoader&&) = default;

    /// @brief Load network configurations from a TOML file
    /// @param file_path Path to the configuration file
    /// @return Vector of network configurations or error
    [[nodiscard]] static ConfigResult<std::vector<NetworkConfig>> 
    load_network_configs(const std::string& file_path) {
        LOG_INFO("Loading network configurations from: {}", file_path);

        try {
            const auto data = toml::parse_file(file_path);
            std::vector<NetworkConfig> configs;

            // Iterate through network configurations
            if (auto network_table = data["network"].as_table()) {
                for (const auto& [key, value] : *network_table) {
                    const auto config_key = std::string(key.str());
                    LOG_DEBUG("Processing network config: {}", config_key);

                    if (auto config_table = value.as_table()) {
                        if (auto config = parse_network_config(config_key, *config_table)) {
                            configs.push_back(std::move(*config));
                            LOG_DEBUG("Successfully parsed network config: {}", config_key);
                        } else {
                            LOG_WARNING("Failed to parse network config: {}", config_key);
                        }
                    }
                }
            }

            LOG_INFO("Successfully loaded {} network configuration(s)", configs.size());
            return configs;

        } catch (const toml::parse_error& e) {
            LOG_ERROR("TOML parse error in '{}': {}", file_path, e.what());
            return std::unexpected(ConfigError::parse_error);
        } catch (const std::exception& e) {
            LOG_ERROR("Error loading network configs from '{}': {}", file_path, e.what());
            return std::unexpected(ConfigError::invalid_format);
        }
    }

private:
    /// @brief Parse a single network configuration from TOML table
    /// @param key Configuration key
    /// @param table TOML table containing configuration data
    /// @return Parsed configuration or nullopt on error
    [[nodiscard]] static std::optional<NetworkConfig> 
    parse_network_config(const std::string& key, const toml::table& table) {
        try {
            // Extract required fields
            const auto name = table["name"].value_or<std::string>("");
            if (name.empty()) {
                LOG_ERROR("Network config '{}' missing required 'name' field", key);
                return std::nullopt;
            }

            NetworkConfig config(key, name);

            // Extract network-specific fields
            if (auto server = table["server"].value<std::string>()) {
                config.set_server_address(*server);
            } else {
                LOG_ERROR("Network config '{}' missing required 'server' field", key);
                return std::nullopt;
            }

            if (auto port = table["port"].value<std::uint16_t>()) {
                config.set_port(*port);
            } else {
                LOG_ERROR("Network config '{}' missing required 'port' field", key);
                return std::nullopt;
            }

            // Extract optional fields
            if (auto timeout = table["timeout_ms"].value<std::uint32_t>()) {
                config.set_timeout_ms(*timeout);
            }

            if (auto retry = table["retry_count"].value<std::uint8_t>()) {
                config.set_retry_count(*retry);
            }

            if (auto ssl = table["use_ssl"].value<bool>()) {
                config.set_use_ssl(*ssl);
            }

            // Extract common fields
            if (auto description = table["description"].value<std::string>()) {
                config.set_description(*description);
            }

            if (auto enabled = table["enabled"].value<bool>()) {
                config.set_enabled(*enabled);
            }

            if (!config.is_valid()) {
                LOG_ERROR("Network config '{}' validation failed", key);
                return std::nullopt;
            }

            return config;

        } catch (const std::exception& e) {
            LOG_ERROR("Exception parsing network config '{}': {}", key, e.what());
            return std::nullopt;
        }
    }
};

} // namespace ff8_hook::config
```

## Integration with Task System

### Step 1: Update ConfigFactory

Add your new configuration type to `config_factory.hpp`:

```cpp
/// @brief Load configuration from file based on type
[[nodiscard]] static ConfigResult<std::vector<ConfigPtr>> 
load_configs(ConfigType type, const std::string& config_file, const std::string& task_name) {
    LOG_INFO("Loading {} configs from file: {} for task: {}", 
             to_string(type), config_file, task_name);

    switch (type) {
        case ConfigType::Memory:
            return load_memory_configs(config_file, task_name);
        
        case ConfigType::Patch:
            return load_patch_configs(config_file, task_name);
        
        case ConfigType::Network:  // NEW
            return load_network_configs(config_file, task_name);
        
        case ConfigType::Script:
        case ConfigType::Audio:
        case ConfigType::Graphics:
            LOG_WARNING("Configuration type '{}' is not yet implemented", to_string(type));
            return std::vector<ConfigPtr>{};
        
        case ConfigType::Unknown:
        default:
            LOG_ERROR("Unknown configuration type: {}", static_cast<int>(type));
            return std::unexpected(ConfigError::invalid_format);
    }
}

private:
    /// @brief Load network configurations from file
    /// @param config_file Path to the configuration file
    /// @param task_name Name of the task
    /// @return Vector of network configuration objects or error
    [[nodiscard]] static ConfigResult<std::vector<ConfigPtr>> 
    load_network_configs(const std::string& config_file, const std::string& task_name) {
        auto network_configs_result = NetworkLoader::load_network_configs(config_file);
        if (!network_configs_result) {
            return std::unexpected(network_configs_result.error());
        }

        std::vector<ConfigPtr> configs;
        configs.reserve(network_configs_result->size());

        for (const auto& network_config : *network_configs_result) {
            auto config = std::make_unique<NetworkConfig>(network_config);
            
            if (config->is_valid()) {
                configs.push_back(std::move(config));
                LOG_DEBUG("Created valid network config: {}", config->debug_string());
            } else {
                LOG_WARNING("Invalid network config for key: {}", network_config.key());
            }
        }

        LOG_INFO("Successfully created {} network configuration object(s)", configs.size());
        return configs;
    }
```

### Step 2: Update HookFactory

Add task creation logic to `hook_factory.hpp`:

```cpp
[[nodiscard]] static FactoryResult process_task_with_dependencies(
    const config::TaskInfo& task,
    std::unordered_map<std::string, std::uintptr_t>& task_hook_addresses,
    HookManager& manager) {
    
    LOG_DEBUG("Processing task '{}' of type '{}'", task.name, to_string(task.type));
    
    if (task.type == config::ConfigType::Memory) {
        // Existing memory task logic...
    } else if (task.type == config::ConfigType::Patch) {
        // Existing patch task logic...
    } else if (task.type == config::ConfigType::Network) {  // NEW
        // Load network configurations
        auto network_configs = config::ConfigFactory::load_configs(task.type, task.config_file, task.name);
        if (!network_configs) {
            LOG_ERROR("Failed to load network configs for task '{}'", task.name);
            return std::unexpected(FactoryError::config_load_failed);
        }
        
        // For network tasks, use a default hook address or dependency resolution
        std::uintptr_t hook_address = 0x12345678; // Example address
        
        // Add network tasks to the hook
        for (const auto& config : *network_configs) {
            if (auto* network_config = dynamic_cast<config::NetworkConfig*>(config.get())) {
                LOG_DEBUG("Creating NetworkTask for config '{}'", network_config->key());
                
                auto network_task = task::make_task<network::NetworkTask>(*network_config);
                
                if (auto result = manager.add_task_to_hook(hook_address, std::move(network_task)); !result) {
                    LOG_ERROR("Failed to add NetworkTask '{}' to hook at address 0x{:X}", 
                             network_config->key(), hook_address);
                    return std::unexpected(FactoryError::hook_creation_failed);
                }
                
                LOG_DEBUG("Successfully added NetworkTask '{}' to hook at address 0x{:X}", 
                         network_config->key(), hook_address);
            }
        }
    } else {
        LOG_WARNING("Unsupported task type '{}' for task '{}'", to_string(task.type), task.name);
    }
    
    return {};
}
```

## Testing Your Implementation

### Step 1: Create Unit Tests

Create `tests/test_network_config.cpp`:

```cpp
/**
 * @file test_network_config.cpp
 * @brief Unit tests for NetworkConfig class and NetworkLoader
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "config/network_config.hpp"
#include "config/network_loader.hpp"
#include <memory>
#include <fstream>
#include <filesystem>

using namespace ff8_hook::config;
using namespace ::testing;

class NetworkConfigTest : public Test {
protected:
    void SetUp() override {
        config_ = std::make_unique<NetworkConfig>("test_network", "Test Network Config");
    }

    std::unique_ptr<NetworkConfig> config_;
};

TEST_F(NetworkConfigTest, BasicConstruction) {
    EXPECT_EQ(config_->type(), ConfigType::Network);
    EXPECT_EQ(config_->key(), "test_network");
    EXPECT_EQ(config_->name(), "Test Network Config");
    EXPECT_FALSE(config_->is_valid()); // Should be invalid without server/port
}

TEST_F(NetworkConfigTest, ConfigurationValidation) {
    // Invalid configuration
    EXPECT_FALSE(config_->is_valid());
    
    // Set required fields
    config_->set_server_address("example.com");
    config_->set_port(8080);
    
    // Should now be valid
    EXPECT_TRUE(config_->is_valid());
}

TEST_F(NetworkConfigTest, ConfigurationFields) {
    config_->set_server_address("test.server.com");
    config_->set_port(443);
    config_->set_use_ssl(true);
    config_->set_timeout_ms(10000);
    config_->set_retry_count(5);
    
    EXPECT_EQ(config_->server_address(), "test.server.com");
    EXPECT_EQ(config_->port(), 443);
    EXPECT_TRUE(config_->use_ssl());
    EXPECT_EQ(config_->timeout_ms(), 10000);
    EXPECT_EQ(config_->retry_count(), 5);
}

// Add more tests for NetworkLoader, NetworkTask, etc.
```

### Step 2: Create Configuration Files

Create test configuration `test_configs/network_test.toml`:

```toml
[network.api_server]
name = "API Server Connection"
description = "Connect to game API server"
server = "api.ff8.game.com"
port = 443
use_ssl = true
timeout_ms = 5000
retry_count = 3
enabled = true

[network.stats_server]
name = "Statistics Server"
description = "Upload player statistics"
server = "stats.ff8.game.com"
port = 8080
use_ssl = false
timeout_ms = 3000
retry_count = 1
enabled = false
```

### Step 3: Add to tasks.toml

```toml
[network_integration]
type = "Network"
config_file = "network_config.toml"
description = "Network integration tasks"
enabled = true
follow_by = ["memory_expansion"]
```

## Best Practices and Conventions

### C++23 Modern Conventions

1. **Use `constexpr` and `noexcept` appropriately**:
   ```cpp
   [[nodiscard]] constexpr bool is_enabled() const noexcept { return enabled_; }
   ```

2. **Prefer `std::expected` for error handling**:
   ```cpp
   [[nodiscard]] task::TaskResult execute() override;
   ```

3. **Use concepts for type safety**:
   ```cpp
   template<ConfigurationConcept ConfigT>
   [[nodiscard]] ConfigPtr make_config(Args&&... args);
   ```

4. **Follow RAII principles**:
   ```cpp
   class NetworkTask {
   private:
       std::unique_ptr<Connection> connection_;  // Automatic cleanup
   };
   ```

### Naming Conventions

- **Classes**: PascalCase (`NetworkConfig`, `NetworkTask`)
- **Methods**: snake_case (`server_address()`, `execute()`)
- **Constants**: SCREAMING_SNAKE_CASE (`MAX_RETRY_COUNT`)
- **Namespaces**: snake_case (`ff8_hook::network`)

### Error Handling

1. **Use appropriate TaskError types**:
   ```cpp
   return std::unexpected(task::TaskError::invalid_config);
   ```

2. **Always validate configurations**:
   ```cpp
   if (!config_.is_valid()) {
       return std::unexpected(task::TaskError::invalid_config);
   }
   ```

3. **Log appropriately**:
   ```cpp
   LOG_DEBUG("Detailed debugging information");
   LOG_INFO("Important status updates");
   LOG_WARNING("Recoverable issues");
   LOG_ERROR("Critical failures");
   ```

### Memory Management

1. **Use smart pointers**:
   ```cpp
   std::unique_ptr<ConfigBase> config = make_config<NetworkConfig>("key", "name");
   ```

2. **Avoid raw pointers except for non-owning references**:
   ```cpp
   const auto* network_config = dynamic_cast<NetworkConfig*>(config.get());
   ```

3. **Use move semantics**:
   ```cpp
   NetworkConfig config(std::move(key), std::move(name));
   ```

### Key Requirements Checklist

- [ ] Configuration class inherits from `ConfigBase`
- [ ] Task class inherits from `IHookTask` and implements all pure virtual methods
- [ ] Loader class follows the established pattern with static methods
- [ ] All classes use C++23 features (`constexpr`, `noexcept`, `[[nodiscard]]`)
- [ ] Proper error handling with `std::expected`
- [ ] Comprehensive logging at appropriate levels
- [ ] Unit tests covering all major functionality
- [ ] TOML configuration files with proper validation
- [ ] Integration with ConfigFactory and HookFactory
- [ ] Documentation following existing patterns

This guide provides everything needed to implement new tasks following the established patterns and modern C++23 conventions used throughout the FF8 Hook system. 