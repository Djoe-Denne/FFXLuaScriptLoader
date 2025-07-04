#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "config/config_factory.hpp"
#include "config/config_base.hpp"
#include "config/config_loader_base.hpp"

namespace app_hook::config {

// Mock config loader for testing
class MockConfigLoader : public ConfigLoaderBase {
public:
    MockConfigLoader(std::string name, ConfigType supported_type)
        : name_(std::move(name)), supported_type_(supported_type) {}
    
    std::string get_name() const override { return name_; }
    
    std::string get_version() const override { return "1.0.0"; }
    
    std::vector<ConfigType> supported_types() const override {
        return {supported_type_};
    }
    
    ConfigResult<std::vector<ConfigPtr>> load_configs(
        ConfigType type, 
        const std::string& file_path, 
        const std::string& task_name) override {
        
        // Return empty vector for successful load
        std::vector<ConfigPtr> configs;
        return configs;
    }
    
private:
    std::string name_;
    ConfigType supported_type_;
};

class ConfigFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing loaders
        // Note: ConfigFactory uses static members, so we need to be careful about test isolation
    }
    
    void TearDown() override {
        // Clean up any registered loaders
        auto loaders = ConfigFactory::get_registered_loaders();
        for (const auto& loader_name : loaders) {
            ConfigFactory::unregister_loader(loader_name);
        }
    }
};

TEST_F(ConfigFactoryTest, RegisterLoader) {
    auto loader = std::make_unique<MockConfigLoader>("TestLoader", ConfigType::Memory);
    
    bool result = ConfigFactory::register_loader(std::move(loader));
    EXPECT_TRUE(result);
    
    auto registered_loaders = ConfigFactory::get_registered_loaders();
    EXPECT_TRUE(std::find(registered_loaders.begin(), registered_loaders.end(), "TestLoader") != registered_loaders.end());
}

TEST_F(ConfigFactoryTest, UnregisterLoader) {
    auto loader = std::make_unique<MockConfigLoader>("UnregisterTest", ConfigType::Memory);
    
    EXPECT_TRUE(ConfigFactory::register_loader(std::move(loader)));
    EXPECT_TRUE(ConfigFactory::unregister_loader("UnregisterTest"));
    
    auto registered_loaders = ConfigFactory::get_registered_loaders();
    EXPECT_TRUE(std::find(registered_loaders.begin(), registered_loaders.end(), "UnregisterTest") == registered_loaders.end());
}

TEST_F(ConfigFactoryTest, UnregisterNonExistentLoader) {
    bool result = ConfigFactory::unregister_loader("NonExistent");
    EXPECT_FALSE(result);
}

TEST_F(ConfigFactoryTest, IsTypeSupportedWithRegisteredLoader) {
    auto loader = std::make_unique<MockConfigLoader>("SupportTest", ConfigType::Memory);
    
    EXPECT_TRUE(ConfigFactory::register_loader(std::move(loader)));
    EXPECT_TRUE(ConfigFactory::is_type_supported(ConfigType::Memory));
}

TEST_F(ConfigFactoryTest, IsTypeSupportedWithoutLoader) {
    EXPECT_FALSE(ConfigFactory::is_type_supported(ConfigType::Memory));
}

TEST_F(ConfigFactoryTest, GetRegisteredLoadersEmpty) {
    auto loaders = ConfigFactory::get_registered_loaders();
    EXPECT_TRUE(loaders.empty());
}

TEST_F(ConfigFactoryTest, LoadConfigsWithSupportedType) {
    auto loader = std::make_unique<MockConfigLoader>("LoadTest", ConfigType::Memory);
    
    EXPECT_TRUE(ConfigFactory::register_loader(std::move(loader)));
    
    auto result = ConfigFactory::load_configs(ConfigType::Memory, "test_config.toml", "test_task");
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty()); // Mock loader returns empty vector
}

TEST_F(ConfigFactoryTest, LoadConfigsWithUnsupportedType) {
    auto result = ConfigFactory::load_configs(ConfigType::Memory, "test_config.toml", "test_task");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ConfigFactoryTest, RegisterMultipleLoaders) {
    auto loader1 = std::make_unique<MockConfigLoader>("Loader1", ConfigType::Memory);
    auto loader2 = std::make_unique<MockConfigLoader>("Loader2", ConfigType::Patch);
    
    EXPECT_TRUE(ConfigFactory::register_loader(std::move(loader1)));
    EXPECT_TRUE(ConfigFactory::register_loader(std::move(loader2)));
    
    auto registered_loaders = ConfigFactory::get_registered_loaders();
    EXPECT_EQ(registered_loaders.size(), 2);
    EXPECT_TRUE(std::find(registered_loaders.begin(), registered_loaders.end(), "Loader1") != registered_loaders.end());
    EXPECT_TRUE(std::find(registered_loaders.begin(), registered_loaders.end(), "Loader2") != registered_loaders.end());
    
    EXPECT_TRUE(ConfigFactory::is_type_supported(ConfigType::Memory));
    EXPECT_TRUE(ConfigFactory::is_type_supported(ConfigType::Patch));
}

// Note: Additional tests would require actual config implementations
// This provides basic structural testing of the ConfigFactory interface

} // namespace app_hook::config 