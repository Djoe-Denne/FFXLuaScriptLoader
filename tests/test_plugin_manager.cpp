#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "plugin/plugin_manager.hpp"

namespace app_hook::plugin {

class PluginManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup for plugin manager tests
    }
    
    void TearDown() override {
        // Cleanup for plugin manager tests
    }
};

TEST_F(PluginManagerTest, DefaultConstructor) {
    PluginManager manager;
    
    // Basic smoke test - verify it constructs without error
    EXPECT_EQ(manager.plugin_count(), 0);
    EXPECT_TRUE(manager.get_loaded_plugin_names().empty());
}

TEST_F(PluginManagerTest, InitializeWithBasicParams) {
    PluginManager manager;
    
    auto config_registry = [](std::unique_ptr<::app_hook::config::ConfigBase> config) {
        // Mock config registry
    };
    
    auto result = manager.initialize("test_data", config_registry);
    
    // Note: This will likely fail without proper DLL infrastructure
    // But verifies the interface works
    EXPECT_EQ(manager.plugin_count(), 0);
}

TEST_F(PluginManagerTest, LoadNonExistentPlugin) {
    PluginManager manager;
    
    auto result = manager.load_plugin("non_existent_plugin.dll");
    
    // Should fail gracefully
    EXPECT_EQ(result, PluginResult::Failed);
    EXPECT_EQ(manager.plugin_count(), 0);
}

TEST_F(PluginManagerTest, UnloadNonExistentPlugin) {
    PluginManager manager;
    
    auto result = manager.unload_plugin("non_existent");
    
    // Should fail gracefully
    EXPECT_EQ(result, PluginResult::Failed);
}

TEST_F(PluginManagerTest, IsPluginLoadedNonExistent) {
    PluginManager manager;
    
    bool loaded = manager.is_plugin_loaded("non_existent");
    EXPECT_FALSE(loaded);
}

TEST_F(PluginManagerTest, GetPluginInfoNonExistent) {
    PluginManager manager;
    
    auto info = manager.get_plugin_info("non_existent");
    EXPECT_FALSE(info.has_value());
}

TEST_F(PluginManagerTest, UnloadAllPluginsEmpty) {
    PluginManager manager;
    
    // Should handle empty case gracefully
    manager.unload_all_plugins();
    EXPECT_EQ(manager.plugin_count(), 0);
}

TEST_F(PluginManagerTest, LoadPluginsFromNonExistentDirectory) {
    PluginManager manager;
    
    std::size_t count = manager.load_plugins_from_directory("non_existent_directory");
    EXPECT_EQ(count, 0);
}

// Note: More comprehensive tests would require actual plugin DLLs
// These tests verify the basic interface works and handles error cases

} // namespace app_hook::plugin 