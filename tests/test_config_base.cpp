/**
 * @file test_config_base.cpp
 * @brief Unit tests for ConfigBase class and related utilities
 * 
 * Tests the base configuration class functionality including:
 * - Type conversions
 * - Configuration validation
 * - Move/copy semantics
 * - Polymorphic behavior
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <config/config_base.hpp>
#include <memory>
#include <string>

using namespace app_hook::config;
using namespace ::testing;

namespace {

/// @brief Mock derived configuration class for testing
class MockConfig : public ConfigBase {
public:
    MockConfig(ConfigType type, std::string key, std::string name)
        : ConfigBase(type, std::move(key), std::move(name)) {}
    
    MOCK_METHOD(bool, is_valid, (), (const, noexcept, override));
    MOCK_METHOD(std::string, debug_string, (), (const, override));
};

/// @brief Test configuration class for move/copy semantics testing
class TestConfig : public ConfigBase {
public:
    TestConfig(ConfigType type, std::string key, std::string name, int test_value = 42)
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

/// @brief Test fixture for ConfigBase tests
class ConfigBaseTest : public Test {
protected:
    void SetUp() override {
        // Create test configurations
        memory_config_ = std::make_unique<TestConfig>(
            ConfigType::Memory, "test_memory", "Test Memory Config", 100);
        patch_config_ = std::make_unique<TestConfig>(
            ConfigType::Patch, "test_patch", "Test Patch Config", 200);
    }

    std::unique_ptr<TestConfig> memory_config_;
    std::unique_ptr<TestConfig> patch_config_;
};

// =============================================================================
// ConfigType Enum Tests
// =============================================================================

TEST(ConfigTypeTest, ToStringConversion) {
    EXPECT_EQ(to_string(ConfigType::Memory), "memory");
    EXPECT_EQ(to_string(ConfigType::Patch), "patch");
    EXPECT_EQ(to_string(ConfigType::Script), "script");
    EXPECT_EQ(to_string(ConfigType::Audio), "audio");
    EXPECT_EQ(to_string(ConfigType::Graphics), "graphics");
    EXPECT_EQ(to_string(ConfigType::Unknown), "unknown");
}

TEST(ConfigTypeTest, FromStringConversion) {
    EXPECT_EQ(from_string("memory"), ConfigType::Memory);
    EXPECT_EQ(from_string("patch"), ConfigType::Patch);
    EXPECT_EQ(from_string("script"), ConfigType::Script);
    EXPECT_EQ(from_string("audio"), ConfigType::Audio);
    EXPECT_EQ(from_string("graphics"), ConfigType::Graphics);
    EXPECT_EQ(from_string("unknown"), ConfigType::Unknown);
    EXPECT_EQ(from_string("invalid"), ConfigType::Unknown);
    EXPECT_EQ(from_string(""), ConfigType::Unknown);
}

TEST(ConfigTypeTest, RoundTripConversion) {
    const auto types = {ConfigType::Memory, ConfigType::Patch, ConfigType::Script, 
                       ConfigType::Audio, ConfigType::Graphics, ConfigType::Unknown};
    
    for (const auto type : types) {
        EXPECT_EQ(from_string(to_string(type)), type);
    }
}

// =============================================================================
// ConfigBase Basic Functionality Tests
// =============================================================================

TEST_F(ConfigBaseTest, BasicConstruction) {
    EXPECT_EQ(memory_config_->type(), ConfigType::Memory);
    EXPECT_EQ(memory_config_->key(), "test_memory");
    EXPECT_EQ(memory_config_->name(), "Test Memory Config");
    EXPECT_EQ(memory_config_->type_string(), "memory");
    EXPECT_TRUE(memory_config_->enabled());
    EXPECT_TRUE(memory_config_->description().empty());
}

TEST_F(ConfigBaseTest, SettersAndGetters) {
    memory_config_->set_description("Test description");
    EXPECT_EQ(memory_config_->description(), "Test description");
    
    memory_config_->set_enabled(false);
    EXPECT_FALSE(memory_config_->enabled());
    
    memory_config_->set_enabled(true);
    EXPECT_TRUE(memory_config_->enabled());
}

TEST_F(ConfigBaseTest, Validation) {
    // Test valid configuration
    EXPECT_TRUE(memory_config_->is_valid());
    
    // Test invalid configuration (empty key/name)
    TestConfig invalid_config(ConfigType::Memory, "", "", -1);
    EXPECT_FALSE(invalid_config.is_valid());
    
    // Test configuration with negative test value
    TestConfig negative_config(ConfigType::Memory, "key", "name", -1);
    EXPECT_FALSE(negative_config.is_valid());
}

TEST_F(ConfigBaseTest, DebugString) {
    const std::string debug_str = memory_config_->debug_string();
    EXPECT_THAT(debug_str, HasSubstr("memory"));
    EXPECT_THAT(debug_str, HasSubstr("test_memory"));
    EXPECT_THAT(debug_str, HasSubstr("Test Memory Config"));
    EXPECT_THAT(debug_str, HasSubstr("enabled: true"));
}

// =============================================================================
// Move and Copy Semantics Tests
// =============================================================================

TEST_F(ConfigBaseTest, MoveConstruction) {
    auto original_config = std::make_unique<TestConfig>(
        ConfigType::Script, "move_test", "Move Test Config", 300);
    
    const auto original_value = original_config->test_value();
    const auto original_key = original_config->key();
    
    // Move construct
    TestConfig moved_config = std::move(*original_config);
    
    EXPECT_EQ(moved_config.type(), ConfigType::Script);
    EXPECT_EQ(moved_config.key(), original_key);
    EXPECT_EQ(moved_config.test_value(), original_value);
}

TEST_F(ConfigBaseTest, MoveAssignment) {
    TestConfig target_config(ConfigType::Audio, "target", "Target Config");
    TestConfig source_config(ConfigType::Graphics, "source", "Source Config", 500);
    
    const auto source_value = source_config.test_value();
    const auto source_key = source_config.key();
    
    // Move assign
    target_config = std::move(source_config);
    
    EXPECT_EQ(target_config.type(), ConfigType::Graphics);
    EXPECT_EQ(target_config.key(), source_key);
    EXPECT_EQ(target_config.test_value(), source_value);
}

TEST_F(ConfigBaseTest, CopyConstruction) {
    const TestConfig copy_config(*memory_config_);
    
    EXPECT_EQ(copy_config.type(), memory_config_->type());
    EXPECT_EQ(copy_config.key(), memory_config_->key());
    EXPECT_EQ(copy_config.name(), memory_config_->name());
    EXPECT_EQ(copy_config.test_value(), memory_config_->test_value());
}

TEST_F(ConfigBaseTest, CopyAssignment) {
    TestConfig target_config(ConfigType::Audio, "target", "Target Config");
    
    target_config = *patch_config_;
    
    EXPECT_EQ(target_config.type(), patch_config_->type());
    EXPECT_EQ(target_config.key(), patch_config_->key());
    EXPECT_EQ(target_config.name(), patch_config_->name());
    EXPECT_EQ(target_config.test_value(), patch_config_->test_value());
}

// Factory function tests moved to test_config_factory.cpp

// =============================================================================
// Polymorphic Behavior Tests
// =============================================================================

TEST(ConfigPolymorphismTest, VirtualFunctionCalls) {
    auto mock_config = std::make_unique<MockConfig>(
        ConfigType::Memory, "mock_test", "Mock Test Config");
    
    EXPECT_CALL(*mock_config, is_valid())
        .WillOnce(Return(true))
        .WillOnce(Return(false));
        
    EXPECT_CALL(*mock_config, debug_string())
        .WillOnce(Return("Mock debug string"));
    
    // Test polymorphic calls
    ConfigBase* base_ptr = mock_config.get();
    
    EXPECT_TRUE(base_ptr->is_valid());
    EXPECT_FALSE(base_ptr->is_valid());
    EXPECT_EQ(base_ptr->debug_string(), "Mock debug string");
}

// =============================================================================
// Edge Cases and Error Conditions
// =============================================================================

TEST(ConfigBaseEdgeCasesTest, EmptyStringInputs) {
    TestConfig empty_key_config(ConfigType::Memory, "", "Valid Name");
    TestConfig empty_name_config(ConfigType::Memory, "valid_key", "");
    TestConfig empty_both_config(ConfigType::Memory, "", "");
    
    EXPECT_FALSE(empty_key_config.is_valid());
    EXPECT_FALSE(empty_name_config.is_valid());
    EXPECT_FALSE(empty_both_config.is_valid());
}

TEST(ConfigBaseEdgeCasesTest, LongStrings) {
    const std::string long_key(1000, 'k');
    const std::string long_name(1000, 'n');
    const std::string long_description(10000, 'd');
    
    TestConfig long_config(ConfigType::Memory, long_key, long_name);
    long_config.set_description(long_description);
    
    EXPECT_EQ(long_config.key(), long_key);
    EXPECT_EQ(long_config.name(), long_name);
    EXPECT_EQ(long_config.description(), long_description);
    EXPECT_TRUE(long_config.is_valid());
}

TEST(ConfigBaseEdgeCasesTest, SpecialCharacters) {
    const std::string special_key = "key_with_特殊文字_and_émojis_🎮";
    const std::string special_name = "Config with special chars: !@#$%^&*()";
    
    TestConfig special_config(ConfigType::Memory, special_key, special_name);
    
    EXPECT_EQ(special_config.key(), special_key);
    EXPECT_EQ(special_config.name(), special_name);
    EXPECT_TRUE(special_config.is_valid());
} 
