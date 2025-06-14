# New Task Development Guide

A concise guide for developing new tasks and configurations in the FF8 Hook system.

## System Architecture

The FF8 Hook system follows this flow:
```
tasks.toml → TaskLoader → ConfigFactory → SpecificLoader → ConfigClass → TaskClass → HookManager
```

### Core Components
- **ConfigBase**: Polymorphic base for all configurations
- **IHookTask**: Interface for all task implementations  
- **TaskLoader**: Orchestrates loading from tasks.toml
- **ConfigFactory**: Creates configurations based on type
- **HookManager**: Manages hook lifecycle and task execution

## Implementation Steps

### 1. Add Configuration Type

Update `ConfigType` enum in `config_base.hpp`:
```cpp
enum class ConfigType : std::uint8_t {
    Memory, Patch, Script, Audio, Graphics,
    Network  // Add your new type
};
```

Update `to_string()` and `from_string()` functions accordingly.

### 2. Create Configuration Class

**Pattern**: Inherit from `ConfigBase`, follow existing examples like `CopyMemoryConfig` or `PatchConfig`.

**Key requirements**:
- Constructor takes `(std::string key, std::string name)`
- Implement `is_valid()` override
- Use C++23 conventions: `[[nodiscard]]`, `constexpr`, `noexcept`
- Provide copy constructor if needed

**Example structure**:
```cpp
class NetworkConfig : public ConfigBase {
public:
    NetworkConfig(std::string key, std::string name);
    
    // Accessors with proper attributes
    [[nodiscard]] constexpr std::uint16_t port() const noexcept;
    
    // Validation
    [[nodiscard]] bool is_valid() const noexcept override;
};
```

### 3. Create Task Implementation

**Pattern**: Inherit from `IHookTask`, follow `CopyMemoryTask` example.

**Key requirements**:
- Implement all pure virtual methods: `execute()`, `name()`, `description()`
- Use `std::expected<void, TaskError>` for `execute()` return
- Validate configuration in `execute()`
- Add comprehensive logging

**Example structure**:
```cpp
class NetworkTask final : public task::IHookTask {
public:
    explicit NetworkTask(NetworkConfig config) noexcept;
    
    [[nodiscard]] task::TaskResult execute() override;
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string description() const override;
};
```

### 4. Create Configuration Loader

**Pattern**: Follow `CopyMemoryLoader` or `PatchConfigLoader` examples.

**Key requirements**:
- Static method `load_[type]_configs(const std::string& file_path)`
- Return `ConfigResult<std::vector<YourConfig>>`
- Parse TOML using toml++ library
- Validate each configuration before adding to results

### 5. Integrate with System

**ConfigFactory integration**:
- Add case to `load_configs()` switch statement
- Create private method `load_[type]_configs()`

**HookFactory integration** (if needed):
- Add case to `process_task_with_dependencies()`
- Handle task creation and hook assignment

### 6. Testing

**Create unit tests** following existing patterns in `tests/`:
- Test configuration validation
- Test loader functionality
- Test task execution
- Mock external dependencies

**Configuration files**:
- Create TOML test configurations
- Add to `tasks.toml` for integration testing

## Essential Patterns

### Configuration Validation
```cpp
[[nodiscard]] bool is_valid() const noexcept override {
    return ConfigBase::is_valid() && 
           !server_address_.empty() && 
           port_ > 0;
}
```

### Error Handling in Tasks
```cpp
if (!config_.is_valid()) {
    LOG_ERROR("Invalid configuration for {}", config_.key());
    return std::unexpected(task::TaskError::invalid_config);
}
```

### TOML Parsing Pattern
```cpp
if (auto value = table["field_name"].value<type>()) {
    config.set_field(*value);
} else {
    LOG_ERROR("Missing required field: field_name");
    return std::nullopt;
}
```

### Factory Integration
```cpp
case ConfigType::YourType:
    return load_your_configs(config_file, task_name);
```

## C++23 Best Practices

- Use `[[nodiscard]]` for all getters and important functions
- Use `constexpr` and `noexcept` where appropriate
- Prefer `std::expected` over exceptions for error handling
- Use move semantics: `std::move()` for expensive copies
- Follow RAII: automatic resource management with smart pointers

## Quick Reference

**Look at existing implementations**:
- Config: `CopyMemoryConfig`, `PatchConfig`
- Task: `CopyMemoryTask`, `PatchMemoryTask`  
- Loader: `CopyMemoryLoader`, `PatchConfigLoader`
- Tests: `test_config_base.cpp`, `test_copy_memory.cpp`

**Common TaskError types**:
- `invalid_config`: Configuration validation failed
- `memory_allocation_failed`: Memory allocation issues
- `dependency_not_met`: Required dependencies missing
- `unknown_error`: Catch-all for exceptions

**Logging levels**:
- `LOG_DEBUG`: Detailed flow information
- `LOG_INFO`: Important milestones
- `LOG_WARNING`: Recoverable issues
- `LOG_ERROR`: Critical failures

## Checklist

- [ ] Added to `ConfigType` enum and conversion functions
- [ ] Configuration class inherits from `ConfigBase` 
- [ ] Task class inherits from `IHookTask`
- [ ] Loader follows established static method pattern
- [ ] Integration with `ConfigFactory` and `HookFactory`
- [ ] Unit tests for all components
- [ ] TOML configuration files created
- [ ] C++23 conventions followed (`[[nodiscard]]`, `constexpr`, etc.)
- [ ] Proper error handling with `std::expected`
- [ ] Comprehensive logging at appropriate levels 