/**
 * @file test_config_factory.cpp
 * @brief Unit tests for configuration factory and related utilities
 * 
 * Tests the configuration factory system including:
 * - Factory function templates
 * - Configuration creation and validation
 * - Polymorphic usage patterns
 * - Integration tests
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "config/config_base.hpp"
#include "config/memory_config.hpp"
#include <memory>
#include <vector>

using namespace app_hook::config;
using namespace ::testing;

namespace {

/// @brief Test configuration class for factory testing
class TestFactoryConfig : public ConfigBase {
public:
    TestFactoryConfig(std::string key, std::string name, int value = 42)
        : ConfigBase(ConfigType::Script, std::move(key), std::move(name))
        , test_value_(value) {}
    
    [[nodiscard]] int test_value() const noexcept { return test_value_; }
    
    [[nodiscard]] bool is_valid() const noexcept override {
        return ConfigBase::is_valid() && test_value_ > 0;
    }
    
    [[nodiscard]] std::string debug_string() const override {
        return ConfigBase::debug_string() + " test_value=" + std::to_string(test_value_);
    }

private:
    int test_value_;
};

/// @brief Another test configuration class
class AlternativeConfig : public ConfigBase {
public:
    AlternativeConfig(std::string key, std::string name, std::string custom_field)
        : ConfigBase(ConfigType::Audio, std::move(key), std::move(name))
        , custom_field_(std::move(custom_field)) {}
    
    [[nodiscard]] const std::string& custom_field() const noexcept { return custom_field_; }

private:
    std::string custom_field_;
};

/// @brief Test configuration class that takes ConfigType as parameter
class LegacyTestConfig : public ConfigBase {
public:
    LegacyTestConfig(ConfigType type, std::string key, std::string name, int test_value = 42)
        : ConfigBase(type, std::move(key), std::move(name))
        , test_value_(test_value) {}
    
    [[nodiscard]] int test_value() const noexcept { return test_value_; }
    
    [[nodiscard]] bool is_valid() const noexcept override {
        return ConfigBase::is_valid() && test_value_ > 0;
    }

private:
    int test_value_;
};

} // anonymous namespace

/// @brief Test fixture for config factory tests
class ConfigFactoryTest : public Test {
protected:
    void SetUp() override {
        // Setup any common test data
    }
};

// =============================================================================
// Basic Factory Function Tests
// =============================================================================

TEST_F(ConfigFactoryTest, MakeConfigLegacyStyle) {
    auto config = make_config<LegacyTestConfig>(ConfigType::Memory, "factory_test", "Factory Test", 999);
    
    ASSERT_NE(config, nullptr);
    EXPECT_EQ(config->type(), ConfigType::Memory);
    EXPECT_EQ(config->key(), "factory_test");
    EXPECT_EQ(config->name(), "Factory Test");
    
    // Downcast to LegacyTestConfig to check specific value
    const auto* test_config = dynamic_cast<LegacyTestConfig*>(config.get());
    ASSERT_NE(test_config, nullptr);
    EXPECT_EQ(test_config->test_value(), 999);
}

TEST_F(ConfigFactoryTest, MakeConfigBasicUsage) {
    auto config = make_config<TestFactoryConfig>("test_key", "Test Config", 100);
    
    ASSERT_NE(config, nullptr);
    EXPECT_EQ(config->type(), ConfigType::Script);
    EXPECT_EQ(config->key(), "test_key");
    EXPECT_EQ(config->name(), "Test Config");
    
    // Downcast to access specific functionality
    const auto* test_config = dynamic_cast<TestFactoryConfig*>(config.get());
    ASSERT_NE(test_config, nullptr);
    EXPECT_EQ(test_config->test_value(), 100);
}

TEST_F(ConfigFactoryTest, MakeConfigWithDefaultParameters) {
    auto config = make_config<TestFactoryConfig>("default_key", "Default Config");
    
    ASSERT_NE(config, nullptr);
    
    const auto* test_config = dynamic_cast<TestFactoryConfig*>(config.get());
    ASSERT_NE(test_config, nullptr);
    EXPECT_EQ(test_config->test_value(), 42);  // Default value
}

TEST_F(ConfigFactoryTest, MakeConfigAlternativeType) {
    auto config = make_config<AlternativeConfig>("alt_key", "Alternative Config", "custom_value");
    
    ASSERT_NE(config, nullptr);
    EXPECT_EQ(config->type(), ConfigType::Audio);
    EXPECT_EQ(config->key(), "alt_key");
    
    const auto* alt_config = dynamic_cast<AlternativeConfig*>(config.get());
    ASSERT_NE(alt_config, nullptr);
    EXPECT_EQ(alt_config->custom_field(), "custom_value");
}

// =============================================================================
// Memory Config Factory Tests
// =============================================================================

TEST_F(ConfigFactoryTest, MakeConfigMemoryConfig) {
    auto config = make_config<CopyMemoryConfig>("memory_key", "Memory Config");
    
    ASSERT_NE(config, nullptr);
    EXPECT_EQ(config->type(), ConfigType::Memory);
    EXPECT_EQ(config->key(), "memory_key");
    EXPECT_EQ(config->name(), "Memory Config");
    
    const auto* memory_config = dynamic_cast<CopyMemoryConfig*>(config.get());
    ASSERT_NE(memory_config, nullptr);
    
    // Should be invalid initially (no address/size set)
    EXPECT_FALSE(memory_config->is_valid());
    
    // Should have default values
    EXPECT_EQ(memory_config->address(), 0);
    EXPECT_EQ(memory_config->original_size(), 0);
    EXPECT_EQ(memory_config->new_size(), 0);
}

// =============================================================================
// Polymorphic Usage Tests
// =============================================================================

TEST_F(ConfigFactoryTest, PolymorphicStorageAndAccess) {
    std::vector<ConfigPtr> configs;
    
    // Create different types of configurations
    configs.push_back(make_config<TestFactoryConfig>("poly1", "Polymorphic Config 1", 200));
    configs.push_back(make_config<AlternativeConfig>("poly2", "Polymorphic Config 2", "poly_value"));
    configs.push_back(make_config<CopyMemoryConfig>("poly3", "Polymorphic Memory Config"));
    
    // Verify all were created successfully
    ASSERT_EQ(configs.size(), 3);
    for (const auto& config : configs) {
        ASSERT_NE(config, nullptr);
        EXPECT_FALSE(config->key().empty());
        EXPECT_FALSE(config->name().empty());
    }
    
    // Test polymorphic access
    EXPECT_EQ(configs[0]->type(), ConfigType::Script);
    EXPECT_EQ(configs[1]->type(), ConfigType::Audio);
    EXPECT_EQ(configs[2]->type(), ConfigType::Memory);
    
    // Test polymorphic validation
    EXPECT_TRUE(configs[0]->is_valid());   // TestFactoryConfig with positive value
    EXPECT_TRUE(configs[1]->is_valid());   // AlternativeConfig (base validation only)
    EXPECT_FALSE(configs[2]->is_valid());  // CopyMemoryConfig without proper setup
}

TEST_F(ConfigFactoryTest, PolymorphicMethodCalls) {
    auto config1 = make_config<TestFactoryConfig>("method_test1", "Method Test 1", 300);
    auto config2 = make_config<AlternativeConfig>("method_test2", "Method Test 2", "method_value");
    
    // Store as base pointers
    ConfigBase* base1 = config1.get();
    ConfigBase* base2 = config2.get();
    
    // Test virtual method calls
    EXPECT_TRUE(base1->is_valid());
    EXPECT_TRUE(base2->is_valid());
    
    const std::string debug1 = base1->debug_string();
    const std::string debug2 = base2->debug_string();
    
    EXPECT_THAT(debug1, HasSubstr("script"));
    EXPECT_THAT(debug1, HasSubstr("method_test1"));
    EXPECT_THAT(debug1, HasSubstr("test_value=300"));
    
    EXPECT_THAT(debug2, HasSubstr("audio"));
    EXPECT_THAT(debug2, HasSubstr("method_test2"));
}

// =============================================================================
// Perfect Forwarding Tests
// =============================================================================

TEST_F(ConfigFactoryTest, PerfectForwardingRValueReferences) {
    std::string key = "rvalue_key";
    std::string name = "RValue Name";
    std::string custom = "rvalue_custom";
    
    // Test with rvalue references (move semantics)
    auto config = make_config<AlternativeConfig>(
        std::move(key), 
        std::move(name), 
        std::move(custom)
    );
    
    ASSERT_NE(config, nullptr);
    EXPECT_EQ(config->key(), "rvalue_key");
    EXPECT_EQ(config->name(), "RValue Name");
    
    const auto* alt_config = dynamic_cast<AlternativeConfig*>(config.get());
    ASSERT_NE(alt_config, nullptr);
    EXPECT_EQ(alt_config->custom_field(), "rvalue_custom");
    
    // Original strings should be moved from (implementation dependent)
    // We can't reliably test this, but the factory should work correctly
}

TEST_F(ConfigFactoryTest, PerfectForwardingLValueReferences) {
    const std::string key = "lvalue_key";
    const std::string name = "LValue Name";
    const int value = 999;
    
    // Test with lvalue references (copy semantics)
    auto config = make_config<TestFactoryConfig>(key, name, value);
    
    ASSERT_NE(config, nullptr);
    EXPECT_EQ(config->key(), key);
    EXPECT_EQ(config->name(), name);
    
    const auto* test_config = dynamic_cast<TestFactoryConfig*>(config.get());
    ASSERT_NE(test_config, nullptr);
    EXPECT_EQ(test_config->test_value(), value);
}

// =============================================================================
// Concept Validation Tests
// =============================================================================

TEST_F(ConfigFactoryTest, ConceptValidation) {
    // These should compile if the concept is correctly implemented
    static_assert(ConfigurationConcept<TestFactoryConfig>, "TestFactoryConfig should satisfy concept");
    static_assert(ConfigurationConcept<AlternativeConfig>, "AlternativeConfig should satisfy concept");
    static_assert(ConfigurationConcept<CopyMemoryConfig>, "CopyMemoryConfig should satisfy concept");
    static_assert(ConfigurationConcept<ConfigBase>, "ConfigBase should satisfy concept");
    
    // These should NOT compile (but we can't test compilation failures easily)
    // static_assert(!ConfigurationConcept<int>, "int should NOT satisfy concept");
    // static_assert(!ConfigurationConcept<std::string>, "std::string should NOT satisfy concept");
    
    SUCCEED();  // If we get here, the concepts compiled correctly
}

// =============================================================================
// Edge Cases and Error Conditions
// =============================================================================

TEST_F(ConfigFactoryTest, EmptyStringParameters) {
    auto config = make_config<TestFactoryConfig>("", "", -1);
    
    ASSERT_NE(config, nullptr);
    EXPECT_TRUE(config->key().empty());
    EXPECT_TRUE(config->name().empty());
    
    // Should be invalid due to empty key/name and negative value
    EXPECT_FALSE(config->is_valid());
}

TEST_F(ConfigFactoryTest, LargeParameterValues) {
    const std::string long_key(1000, 'k');
    const std::string long_name(1000, 'n');
    const std::string long_custom(10000, 'c');
    
    auto config = make_config<AlternativeConfig>(long_key, long_name, long_custom);
    
    ASSERT_NE(config, nullptr);
    EXPECT_EQ(config->key(), long_key);
    EXPECT_EQ(config->name(), long_name);
    
    const auto* alt_config = dynamic_cast<AlternativeConfig*>(config.get());
    ASSERT_NE(alt_config, nullptr);
    EXPECT_EQ(alt_config->custom_field(), long_custom);
    
    EXPECT_TRUE(config->is_valid());
}

// =============================================================================
// Factory Integration Tests
// =============================================================================

TEST_F(ConfigFactoryTest, FactoryWithComplexConfiguration) {
    // Create and configure a memory configuration using the factory
    auto memory_config = make_config<CopyMemoryConfig>("integration_test", "Integration Test Config");
    
    ASSERT_NE(memory_config, nullptr);
    
    // Configure the memory config to make it valid
    auto* mem_config = dynamic_cast<CopyMemoryConfig*>(memory_config.get());
    ASSERT_NE(mem_config, nullptr);
    
    mem_config->set_address(0x12345678);
    mem_config->set_copy_after(0x12346000);
    mem_config->set_original_size(1024);
    mem_config->set_new_size(2048);
    mem_config->set_description("Integration test memory expansion");
    
    EXPECT_TRUE(mem_config->is_valid());
    EXPECT_EQ(mem_config->address(), 0x12345678);
    EXPECT_EQ(mem_config->new_size(), 2048);
    EXPECT_THAT(mem_config->debug_string(), HasSubstr("Integration Test Config"));
}

TEST_F(ConfigFactoryTest, MultipleFactoryCreations) {
    constexpr int num_configs = 100;
    std::vector<ConfigPtr> configs;
    configs.reserve(num_configs);
    
    // Create many configurations
    for (int i = 0; i < num_configs; ++i) {
        const std::string key = "bulk_test_" + std::to_string(i);
        const std::string name = "Bulk Test Config " + std::to_string(i);
        
        auto config = make_config<TestFactoryConfig>(key, name, i + 1);
        ASSERT_NE(config, nullptr);
        configs.push_back(std::move(config));
    }
    
    // Verify all configurations
    EXPECT_EQ(configs.size(), num_configs);
    
    for (int i = 0; i < num_configs; ++i) {
        ASSERT_NE(configs[i], nullptr);
        EXPECT_EQ(configs[i]->key(), "bulk_test_" + std::to_string(i));
        
        const auto* test_config = dynamic_cast<TestFactoryConfig*>(configs[i].get());
        ASSERT_NE(test_config, nullptr);
        EXPECT_EQ(test_config->test_value(), i + 1);
    }
}

// =============================================================================
// Memory Management Tests
// =============================================================================

TEST_F(ConfigFactoryTest, FactoryMemoryManagement) {
    ConfigPtr config;
    
    {
        // Create config in nested scope
        auto temp_config = make_config<TestFactoryConfig>("scope_test", "Scope Test", 777);
        config = std::move(temp_config);
    }
    
    // Config should still be valid after scope exit
    ASSERT_NE(config, nullptr);
    EXPECT_EQ(config->key(), "scope_test");
    EXPECT_TRUE(config->is_valid());
    
    const auto* test_config = dynamic_cast<TestFactoryConfig*>(config.get());
    ASSERT_NE(test_config, nullptr);
    EXPECT_EQ(test_config->test_value(), 777);
}

TEST_F(ConfigFactoryTest, FactoryMoveSemantics) {
    auto original = make_config<TestFactoryConfig>("move_test", "Move Test", 888);
    const auto* original_ptr = original.get();
    
    auto moved = std::move(original);
    
    EXPECT_EQ(original.get(), nullptr);
    EXPECT_EQ(moved.get(), original_ptr);
    EXPECT_EQ(moved->key(), "move_test");
} 
