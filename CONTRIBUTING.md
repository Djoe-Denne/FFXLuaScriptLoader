# Contributing to Legacy Software Extension Framework

Thank you for your interest in contributing to the Legacy Software Extension Framework! This document outlines the standards and guidelines for contributing to this project.

## Code Standards

### Modern C++ 23

This project uses modern C++ 23 features and best practices:

- **Use C++ 23 standard library features** when available
- **Prefer `std::ranges`** over traditional loops where applicable
- **Use `std::expected`** for error handling instead of exceptions
- **Leverage concepts** for template constraints and clear interfaces
- **Use `std::format`** for string formatting
- **Apply RAII principles** consistently
- **Prefer smart pointers** (`std::unique_ptr`, `std::shared_ptr`) over raw pointers
- **Use `constexpr` and `consteval`** where possible for compile-time evaluation
- **Embrace structured bindings** and auto type deduction appropriately

### Architecture Respect

This framework follows a plugin-based architecture with clear separation of concerns:

- **Core Hook System**: Generic hooking and injection mechanisms
- **Plugin Interface**: Well-defined interfaces for extensibility
- **Configuration System**: TOML-based configuration without hardcoded values
- **Task Management**: Modular task execution system

#### Architectural Guidelines

1. **Maintain Plugin Boundaries**: Don't directly access internal plugin details from other plugins
2. **Use Interfaces**: Always program against interfaces, not implementations
3. **Follow Dependency Injection**: Use the existing factory patterns and dependency injection
4. **Respect the Configuration Layer**: All runtime behavior should be configurable via TOML
5. **Keep Core Generic**: Don't add game-specific logic to core components

#### Adding New Components

When adding new functionality:

1. **Create appropriate interfaces** in `core_hook/include/`
2. **Implement in separate plugins** when game-specific
3. **Use existing factories** for object creation
4. **Follow the established naming conventions**
5. **Add configuration support** for new features

## Testing Requirements

All contributions must include comprehensive tests:

### Unit Tests

- **Coverage**: New code must have >90% test coverage
- **Test Framework**: Use the existing test framework (located in `tests/`)
- **Test Organization**: Group tests by component/feature
- **Mocking**: Use the existing mock framework for dependencies

### Test Structure

```cpp
#include <gtest/gtest.h>
#include "mock_plugin_host.hpp"

class MyComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup
    }
    
    void TearDown() override {
        // Test cleanup
    }
    
    MockPluginHost mock_host;
};

TEST_F(MyComponentTest, ShouldHandleValidInput) {
    // Arrange
    auto component = createComponent();
    
    // Act
    auto result = component.process(valid_input);
    
    // Assert
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(expected_output, result.value());
}
```

### Integration Tests

- **End-to-End**: Test complete workflows where applicable
- **Configuration**: Test with various TOML configurations
- **Error Handling**: Test failure scenarios and edge cases

### Running Tests

```bash
# Build tests
cmake --build build --target app_hook_tests

# Run all tests
build/bin/tests/Debug/app_hook_tests.exe

# Run specific test suite
build/bin/tests/Debug/app_hook_tests.exe --gtest_filter="MyComponentTest.*"
```

## Documentation Requirements

### Code Documentation

- **Header Documentation**: All public APIs must have comprehensive header documentation
- **Inline Comments**: Complex algorithms and business logic should be commented
- **Example Usage**: Provide usage examples for new features
- **Architecture Documentation**: Update architecture docs when adding new components

### Documentation Standards

```cpp
/**
 * @brief Processes memory region data according to configuration
 * 
 * This function takes a memory region and applies the configured transformations
 * based on the provided TOML configuration. It handles both expansion and
 * patching operations.
 * 
 * @param region The memory region to process
 * @param config The TOML configuration containing transformation rules
 * @return std::expected<ProcessedRegion, std::string> The processed region or error message
 * 
 * @example
 * ```cpp
 * auto config = load_config("memory_config.toml");
 * auto result = process_memory_region(region, config);
 * if (result.has_value()) {
 *     // Use result.value()
 * }
 * ```
 */
auto process_memory_region(const MemoryRegion& region, const TomlConfig& config) 
    -> std::expected<ProcessedRegion, std::string>;
```

### Documentation Updates

When contributing:

1. **Update relevant documentation** in the `docs/` directory
2. **Add examples** to demonstrate new features
3. **Update architecture diagrams** if structural changes are made
4. **Keep README.md current** with new usage patterns

## Generic Development Principles

This framework is designed to be **generic and reusable**. Keep these principles in mind:

### Keep It Generic

- **No Game-Specific Logic**: Don't add game-specific code to the core framework
- **Configurable Behavior**: Make functionality configurable rather than hardcoded
- **Extensible Design**: Design components to be easily extended without modification
- **Platform Agnostic**: Where possible, avoid platform-specific dependencies

### Game-Specific Implementations

If you want to implement functionality for a specific game:

1. **Create a Dedicated Repository**: Game-specific plugins should be in separate repos
2. **Use the Plugin Interface**: Implement the framework's plugin interfaces
3. **Reference This Framework**: Use this framework as a dependency/submodule
4. **Follow Naming Conventions**: Use clear, game-specific naming (e.g., `ff8-memory-plugin`)

## Contribution Process

### Before Contributing

1. **Check existing issues** for similar work
2. **Create an issue** to discuss major changes
3. **Fork the repository** and create a feature branch
4. **Follow the coding standards** outlined above

### **Branch Strategy**
```bash
# Feature development
git checkout -b feature/magic-status-effects
git checkout -b feature/ui-improvements

# Bug fixes
git checkout -b bugfix/binary-parsing-issue
git checkout -b bugfix/validation-error

# Documentation
git checkout -b docs/contributing-guide
git checkout -b docs/architecture-update
```

### **Commit Messages**
```bash
# ✅ Good commit messages
feat: add support for custom status effect combinations
fix: resolve binary parsing issue with 48-bit status effects
docs: update architecture documentation with new patterns
test: add comprehensive tests for magic validation service
refactor: extract status effect parsing to separate service

# ❌ Bad commit messages
fix stuff
update code
changes
```

## Code Review Standards

All contributions will be reviewed for:

- **Code Quality**: Adherence to C++ 23 standards
- **Architecture Compliance**: Proper use of existing patterns
- **Test Coverage**: Comprehensive testing of new functionality
- **Documentation**: Clear and complete documentation
- **Generic Design**: Avoidance of game-specific implementations

## Getting Help

- **Create an issue** for questions about the architecture
- **Check existing documentation** in the `docs/` directory
- **Review existing code** for patterns and examples
- **Ask questions** in pull request reviews

## License

By contributing to this project, you agree that your contributions will be licensed under the MIT License. 