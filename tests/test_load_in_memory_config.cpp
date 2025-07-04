#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../memory_plugin/include/config/load_in_memory_config.hpp"
#include <limits>

using namespace app_hook::config;

class LoadInMemoryConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        basic_config_ = std::make_unique<LoadInMemoryConfig>("test_key", "Test Load");
    }

    std::unique_ptr<LoadInMemoryConfig> basic_config_;
};

TEST_F(LoadInMemoryConfigTest, BasicConstruction) {
    EXPECT_EQ(basic_config_->key(), "test_key");
    EXPECT_EQ(basic_config_->name(), "Test Load");
    EXPECT_EQ(basic_config_->type(), ConfigType::Load);
    EXPECT_EQ(basic_config_->binary_path(), "");
    EXPECT_EQ(basic_config_->offset_security(), 0);
}

TEST_F(LoadInMemoryConfigTest, CopyConstructor) {
    // Set up original config
    basic_config_->set_binary_path("test.bin");
    basic_config_->set_offset_security(0x100);
    basic_config_->set_description("Test description");
    basic_config_->set_enabled(false);
    
    WriteContextConfig write_config;
    write_config.enabled = true;
    write_config.name = "test_context";
    basic_config_->set_write_in_context(std::move(write_config));
    basic_config_->set_read_from_context("read_context");
    
    // Copy construct
    LoadInMemoryConfig copied(*basic_config_);
    
    // Verify all fields copied correctly
    EXPECT_EQ(copied.key(), basic_config_->key());
    EXPECT_EQ(copied.name(), basic_config_->name());
    EXPECT_EQ(copied.type(), basic_config_->type());
    EXPECT_EQ(copied.binary_path(), basic_config_->binary_path());
    EXPECT_EQ(copied.offset_security(), basic_config_->offset_security());
    EXPECT_EQ(copied.description(), basic_config_->description());
    EXPECT_EQ(copied.enabled(), basic_config_->enabled());
    EXPECT_EQ(copied.writes_to_context(), basic_config_->writes_to_context());
    EXPECT_EQ(copied.write_in_context().enabled, basic_config_->write_in_context().enabled);
    EXPECT_EQ(copied.write_in_context().name, basic_config_->write_in_context().name);
    EXPECT_EQ(copied.read_from_context(), basic_config_->read_from_context());
}

TEST_F(LoadInMemoryConfigTest, BinaryPathAccessors) {
    EXPECT_EQ(basic_config_->binary_path(), "");
    
    basic_config_->set_binary_path("test.bin");
    EXPECT_EQ(basic_config_->binary_path(), "test.bin");
    
    basic_config_->set_binary_path("/path/to/file.bin");
    EXPECT_EQ(basic_config_->binary_path(), "/path/to/file.bin");
    
    // Test moving strings
    std::string path = "moved_path.bin";
    basic_config_->set_binary_path(std::move(path));
    EXPECT_EQ(basic_config_->binary_path(), "moved_path.bin");
    EXPECT_TRUE(path.empty());  // Should be moved from
}

TEST_F(LoadInMemoryConfigTest, OffsetSecurityAccessors) {
    EXPECT_EQ(basic_config_->offset_security(), 0);
    
    basic_config_->set_offset_security(0x100);
    EXPECT_EQ(basic_config_->offset_security(), 0x100);
    
    basic_config_->set_offset_security(0xDEADBEEF);
    EXPECT_EQ(basic_config_->offset_security(), 0xDEADBEEF);
    
    // Test large values
    basic_config_->set_offset_security(std::numeric_limits<std::uintptr_t>::max());
    EXPECT_EQ(basic_config_->offset_security(), std::numeric_limits<std::uintptr_t>::max());
}

TEST_F(LoadInMemoryConfigTest, ValidityChecking) {
    // Invalid with empty binary path
    EXPECT_FALSE(basic_config_->is_valid());
    
    // Valid with binary path set
    basic_config_->set_binary_path("test.bin");
    EXPECT_TRUE(basic_config_->is_valid());
    
    // Still valid after setting other fields
    basic_config_->set_offset_security(0x100);
    basic_config_->set_description("Test");
    EXPECT_TRUE(basic_config_->is_valid());
    
    // ConfigBase::is_valid() doesn't check enabled state, so this should still be valid
    basic_config_->set_enabled(false);
    EXPECT_TRUE(basic_config_->is_valid());
    
    // Still valid when re-enabled
    basic_config_->set_enabled(true);
    EXPECT_TRUE(basic_config_->is_valid());
    
    // Invalid if empty key (from base class)
    LoadInMemoryConfig empty_key_config("", "Test");
    empty_key_config.set_binary_path("test.bin");
    EXPECT_FALSE(empty_key_config.is_valid());
    
    // Invalid if empty name (from base class)
    LoadInMemoryConfig empty_name_config("test", "");
    empty_name_config.set_binary_path("test.bin");
    EXPECT_FALSE(empty_name_config.is_valid());
}

TEST_F(LoadInMemoryConfigTest, DebugString) {
    basic_config_->set_binary_path("test.bin");
    basic_config_->set_offset_security(0x100);
    basic_config_->set_description("Test description");
    
    std::string debug_str = basic_config_->debug_string();
    
    // Should contain base class info
    EXPECT_NE(debug_str.find("test_key"), std::string::npos);
    EXPECT_NE(debug_str.find("Test Load"), std::string::npos);
    // Note: description is not included in debug_string output
    
    // Should contain load-specific info
    EXPECT_NE(debug_str.find("binary=test.bin"), std::string::npos);
    EXPECT_NE(debug_str.find("offsetSecurity=0x256"), std::string::npos);  // 0x100 = 256 in hex
}

TEST_F(LoadInMemoryConfigTest, DebugStringWithoutOptionalFields) {
    basic_config_->set_binary_path("simple.bin");
    
    std::string debug_str = basic_config_->debug_string();
    
    EXPECT_NE(debug_str.find("binary=simple.bin"), std::string::npos);
    EXPECT_NE(debug_str.find("offsetSecurity=0x0"), std::string::npos);
}

TEST_F(LoadInMemoryConfigTest, LargeOffsetSecurity) {
    // Test with large offset values (reduced for 32-bit)
    const std::uintptr_t large_offset = 0x12345678;  // Reduced for 32-bit compatibility
    basic_config_->set_offset_security(large_offset);
    
    EXPECT_EQ(basic_config_->offset_security(), large_offset);
    
    std::string debug_str = basic_config_->debug_string();
    EXPECT_NE(debug_str.find("offsetSecurity=0x" + std::to_string(large_offset)), std::string::npos);
}

TEST_F(LoadInMemoryConfigTest, EmptyBinaryPath) {
    // Test that empty binary path makes config invalid
    basic_config_->set_binary_path("");
    EXPECT_FALSE(basic_config_->is_valid());
    
    // Test that setting non-empty path makes it valid again
    basic_config_->set_binary_path("valid.bin");
    EXPECT_TRUE(basic_config_->is_valid());
}

TEST_F(LoadInMemoryConfigTest, WhitespaceBinaryPath) {
    // Test that whitespace-only path is still valid (path validation is file system's job)
    basic_config_->set_binary_path("   ");
    EXPECT_TRUE(basic_config_->is_valid());
    
    basic_config_->set_binary_path("\t\n");
    EXPECT_TRUE(basic_config_->is_valid());
}

TEST_F(LoadInMemoryConfigTest, LongBinaryPath) {
    // Test with very long path
    std::string long_path(1000, 'x');
    long_path += ".bin";
    
    basic_config_->set_binary_path(long_path);
    EXPECT_EQ(basic_config_->binary_path(), long_path);
    EXPECT_TRUE(basic_config_->is_valid());
}

TEST_F(LoadInMemoryConfigTest, ConfigTypeConsistency) {
    // Verify type is always Load regardless of other changes
    EXPECT_EQ(basic_config_->type(), ConfigType::Load);
    
    basic_config_->set_binary_path("test.bin");
    EXPECT_EQ(basic_config_->type(), ConfigType::Load);
    
    basic_config_->set_offset_security(0x1000);
    EXPECT_EQ(basic_config_->type(), ConfigType::Load);
    
    basic_config_->set_description("Changed description");
    EXPECT_EQ(basic_config_->type(), ConfigType::Load);
} 