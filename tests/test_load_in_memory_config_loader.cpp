#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../memory_plugin/include/config/load_in_memory_config_loader.hpp"
#include "../memory_plugin/include/config/load_in_memory_config.hpp"
#include "mock_plugin_host.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace memory_plugin;
using namespace app_hook::config;
using namespace testing;

class LoadInMemoryConfigLoaderTest : public Test {
protected:
    void SetUp() override {
        loader_ = std::make_unique<LoadInMemoryConfigLoader>();
        mock_host_ = std::make_unique<MockPluginHost>();
        loader_->setHost(mock_host_.get());
        
        // Create temporary directory for test files
        temp_dir_ = std::filesystem::temp_directory_path() / "loadinmemory_test";
        std::filesystem::create_directories(temp_dir_);
    }

    void TearDown() override {
        // Clean up test files
        if (std::filesystem::exists(temp_dir_)) {
            std::filesystem::remove_all(temp_dir_);
        }
    }

    void create_test_file(const std::string& filename, const std::string& content) {
        auto file_path = temp_dir_ / filename;
        std::ofstream file(file_path);
        file << content;
        file.close();
    }

    std::string get_test_file_path(const std::string& filename) {
        return (temp_dir_ / filename).string();
    }

    std::unique_ptr<LoadInMemoryConfigLoader> loader_;
    std::unique_ptr<MockPluginHost> mock_host_;
    std::filesystem::path temp_dir_;
};

TEST_F(LoadInMemoryConfigLoaderTest, SupportedTypes) {
    auto types = loader_->supported_types();
    
    ASSERT_EQ(types.size(), 1);
    EXPECT_EQ(types[0], ConfigType::Load);
}

TEST_F(LoadInMemoryConfigLoaderTest, GetNameAndVersion) {
    EXPECT_EQ(loader_->get_name(), "Load In Memory Operations Loader");
    EXPECT_EQ(loader_->get_version(), "1.0.0");
}

TEST_F(LoadInMemoryConfigLoaderTest, LoadConfigsWithUnsupportedType) {
    std::string config_content = R"(
        [load.test]
        name = "test_load"
        binary = "test.bin"
    )";
    
    create_test_file("test_unsupported.toml", config_content);
    
    auto result = loader_->load_configs(ConfigType::Memory, get_test_file_path("test_unsupported.toml"), "test_task");
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ConfigError::invalid_format);
}

TEST_F(LoadInMemoryConfigLoaderTest, LoadConfigsFileNotFound) {
    auto result = loader_->load_configs(ConfigType::Load, "non_existent_file.toml", "test_task");
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ConfigError::file_not_found);
}

TEST_F(LoadInMemoryConfigLoaderTest, LoadConfigsInvalidToml) {
    std::string invalid_toml = R"(
        [load.test
        name = "test_load"
        binary = "test.bin"
    )";
    
    create_test_file("invalid.toml", invalid_toml);
    
    auto result = loader_->load_configs(ConfigType::Load, get_test_file_path("invalid.toml"), "test_task");
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ConfigError::parse_error);
}

TEST_F(LoadInMemoryConfigLoaderTest, LoadConfigsArrayFormat) {
    std::string config_content = R"(
        [[load]]
        name = "test_load1"
        binary = "test1.bin"
        description = "Test load operation 1"
        offsetSecurity = "0x100"
        
        [[load]]
        name = "test_load2" 
        binary = "test2.bin"
        offsetSecurity = 256
    )";
    
    create_test_file("array_format.toml", config_content);
    
    auto result = loader_->load_configs(ConfigType::Load, get_test_file_path("array_format.toml"), "test_task");
    
    ASSERT_TRUE(result.has_value());
    auto& configs = result.value();
    
    ASSERT_EQ(configs.size(), 2);
    
    // Check first config
    auto* config1 = static_cast<LoadInMemoryConfig*>(configs[0].get());
    EXPECT_EQ(config1->key(), "test_task_test_load1");
    EXPECT_EQ(config1->name(), "test_load1");
    EXPECT_EQ(config1->binary_path(), "test1.bin");
    EXPECT_EQ(config1->description(), "Test load operation 1");
    EXPECT_EQ(config1->offset_security(), 0x100);
    
    // Check second config
    auto* config2 = static_cast<LoadInMemoryConfig*>(configs[1].get());
    EXPECT_EQ(config2->key(), "test_task_test_load2");
    EXPECT_EQ(config2->name(), "test_load2");
    EXPECT_EQ(config2->binary_path(), "test2.bin");
    EXPECT_EQ(config2->offset_security(), 256);
}

TEST_F(LoadInMemoryConfigLoaderTest, LoadConfigsTableFormat) {
    std::string config_content = R"(
        [load.magic_data]
        binary = "magic.bin"
        description = "Magic data loader"
        offsetSecurity = "0x200"
        
        [load.spell_data]
        binary = "spells.bin"
        offsetSecurity = 512
    )";
    
    create_test_file("table_format.toml", config_content);
    
    auto result = loader_->load_configs(ConfigType::Load, get_test_file_path("table_format.toml"), "test_task");
    
    ASSERT_TRUE(result.has_value());
    auto& configs = result.value();
    
    ASSERT_EQ(configs.size(), 2);
    
    // Configs can be in any order, so find them by name
    LoadInMemoryConfig* magic_config = nullptr;
    LoadInMemoryConfig* spell_config = nullptr;
    
    for (auto& config : configs) {
        auto* load_config = static_cast<LoadInMemoryConfig*>(config.get());
        if (load_config->name() == "magic_data") {
            magic_config = load_config;
        } else if (load_config->name() == "spell_data") {
            spell_config = load_config;
        }
    }
    
    ASSERT_NE(magic_config, nullptr);
    ASSERT_NE(spell_config, nullptr);
    
    EXPECT_EQ(magic_config->key(), "test_task_magic_data");
    EXPECT_EQ(magic_config->binary_path(), "magic.bin");
    EXPECT_EQ(magic_config->description(), "Magic data loader");
    EXPECT_EQ(magic_config->offset_security(), 0x200);
    
    EXPECT_EQ(spell_config->key(), "test_task_spell_data");
    EXPECT_EQ(spell_config->binary_path(), "spells.bin");
    EXPECT_EQ(spell_config->offset_security(), 512);
}

TEST_F(LoadInMemoryConfigLoaderTest, LoadConfigsWithContextConfiguration) {
    std::string config_content = R"(
        [load.context_test]
        binary = "test.bin"
        readFromContext = "memory_region"
        
        [load.context_test.writeInContext]
        enabled = true
        name = "loaded_data"
    )";
    
    create_test_file("context_config.toml", config_content);
    
    auto result = loader_->load_configs(ConfigType::Load, get_test_file_path("context_config.toml"), "test_task");
    
    ASSERT_TRUE(result.has_value());
    auto& configs = result.value();
    
    ASSERT_EQ(configs.size(), 1);
    
    auto* config = static_cast<LoadInMemoryConfig*>(configs[0].get());
    EXPECT_EQ(config->read_from_context(), "memory_region");
    EXPECT_TRUE(config->writes_to_context());
    EXPECT_EQ(config->write_in_context().name, "loaded_data");
    EXPECT_TRUE(config->write_in_context().enabled);
}

TEST_F(LoadInMemoryConfigLoaderTest, LoadConfigsMissingBinaryField) {
    std::string config_content = R"(
        [[load]]
        name = "invalid_load"
        description = "Missing binary field"
    )";
    
    create_test_file("missing_binary.toml", config_content);
    
    auto result = loader_->load_configs(ConfigType::Load, get_test_file_path("missing_binary.toml"), "test_task");
    
    ASSERT_TRUE(result.has_value());
    auto& configs = result.value();
    
    // Should have no configs because parsing failed
    EXPECT_EQ(configs.size(), 0);
}

TEST_F(LoadInMemoryConfigLoaderTest, LoadConfigsMissingNameInArrayFormat) {
    std::string config_content = R"(
        [[load]]
        binary = "test.bin"
        description = "Missing name field"
    )";
    
    create_test_file("missing_name.toml", config_content);
    
    auto result = loader_->load_configs(ConfigType::Load, get_test_file_path("missing_name.toml"), "test_task");
    
    ASSERT_TRUE(result.has_value());
    auto& configs = result.value();
    
    // Should have no configs because parsing failed
    EXPECT_EQ(configs.size(), 0);
}

TEST_F(LoadInMemoryConfigLoaderTest, AddressParsing) {
    std::string config_content = R"(
        [load.hex_test]
        binary = "test.bin"
        offsetSecurity = "0xABCD1234"
        
        [load.dec_test]
        binary = "test.bin"
        offsetSecurity = 12345
    )";
    
    create_test_file("address_parsing.toml", config_content);
    
    auto result = loader_->load_configs(ConfigType::Load, get_test_file_path("address_parsing.toml"), "test_task");
    
    ASSERT_TRUE(result.has_value());
    auto& configs = result.value();
    
    ASSERT_EQ(configs.size(), 2);
    
    for (auto& config : configs) {
        auto* load_config = static_cast<LoadInMemoryConfig*>(config.get());
        if (load_config->name() == "hex_test") {
            EXPECT_EQ(load_config->offset_security(), 0xABCD1234);
        } else if (load_config->name() == "dec_test") {
            EXPECT_EQ(load_config->offset_security(), 12345);
        }
    }
}

TEST_F(LoadInMemoryConfigLoaderTest, EmptyLoadSection) {
    std::string config_content = R"(
        [other_section]
        value = "test"
    )";
    
    create_test_file("empty_load.toml", config_content);
    
    auto result = loader_->load_configs(ConfigType::Load, get_test_file_path("empty_load.toml"), "test_task");
    
    ASSERT_TRUE(result.has_value());
    auto& configs = result.value();
    
    EXPECT_EQ(configs.size(), 0);
}

TEST_F(LoadInMemoryConfigLoaderTest, InvalidOffsetSecurityType) {
    std::string config_content = R"(
        [load.test]
        binary = "test.bin"
        offsetSecurity = ["invalid", "array"]
    )";
    
    create_test_file("invalid_offset.toml", config_content);
    
    auto result = loader_->load_configs(ConfigType::Load, get_test_file_path("invalid_offset.toml"), "test_task");
    
    ASSERT_TRUE(result.has_value());
    auto& configs = result.value();
    
    ASSERT_EQ(configs.size(), 1);
    
    auto* config = static_cast<LoadInMemoryConfig*>(configs[0].get());
    // Should use default value (0) for invalid offset security
    EXPECT_EQ(config->offset_security(), 0);
} 