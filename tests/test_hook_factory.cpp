#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "hook/hook_factory.hpp"
#include "hook/hook_manager.hpp"
#include "config/config_factory.hpp"
#include "config/config_loader_base.hpp"
#include "config/config_base.hpp"
#include "task/task_factory.hpp"
#include "util/logger.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace app_hook::hook {

// Mock config for testing
class MockConfig : public config::ConfigBase, public config::AddressTrigger {
public:
    MockConfig(std::string key, std::string name, std::uintptr_t address = 0x401000)
        : ConfigBase(config::ConfigType::Memory, std::move(key), std::move(name)), address_(address) {}
    
    bool is_valid() const noexcept override { return true; }
    std::uintptr_t get_hook_address() const noexcept override { return address_; }
    
private:
    std::uintptr_t address_;
};

// Mock config loader for testing
class MockConfigLoader : public config::ConfigLoaderBase {
public:
    MockConfigLoader(std::string name, config::ConfigType supported_type, bool should_fail = false)
        : name_(std::move(name)), supported_type_(supported_type), should_fail_(should_fail) {}
    
    std::string get_name() const override { return name_; }
    std::string get_version() const override { return "1.0.0"; }
    
    std::vector<config::ConfigType> supported_types() const override {
        return {supported_type_};
    }
    
    config::ConfigResult<std::vector<config::ConfigPtr>> load_configs(
        config::ConfigType type, 
        const std::string& file_path, 
        const std::string& task_name) override {
        
        (void)type; // Suppress unused parameter warning
        (void)file_path; // Suppress unused parameter warning
        
        if (should_fail_) {
            return std::unexpected(config::ConfigError::file_not_found);
        }
        
        // Return a mock config
        std::vector<config::ConfigPtr> configs;
        configs.push_back(std::make_shared<MockConfig>(task_name, task_name, 0x401000));
        return configs;
    }
    
private:
    std::string name_;
    config::ConfigType supported_type_;
    bool should_fail_;
};

// Mock task for testing
class MockTask : public task::IHookTask {
public:
    MockTask(std::string name) : name_(std::move(name)) {}
    
    std::string name() const override { return name_; }
    
    std::string description() const override { return "Mock task: " + name_; }
    
    task::TaskResult execute() override {
        executed_ = true;
        return {};
    }
    
    bool was_executed() const { return executed_; }
    
private:
    std::string name_;
    bool executed_ = false;
};

// Mock task creator for testing
class MockTaskCreator {
public:
    static std::unique_ptr<task::IHookTask> create_task(const config::ConfigBase& config) {
        return std::make_unique<MockTask>(config.key());
    }
};

class HookFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for test files
        test_dir_ = std::filesystem::temp_directory_path() / "hook_factory_tests";
        std::filesystem::create_directories(test_dir_);
        
        // Create tasks subdirectory
        tasks_dir_ = test_dir_ / "tasks";
        std::filesystem::create_directories(tasks_dir_);
        
        // Register mock config loader
        auto loader = std::make_unique<MockConfigLoader>("TestLoader", config::ConfigType::Memory);
        bool loader_registered = config::ConfigFactory::register_loader(std::move(loader));
        (void)loader_registered; // Suppress unused variable warning
        
        // Register mock task creator
        task::TaskFactory::instance().register_task_creator(
            "app_hook::hook::MockConfig",
            [](const config::ConfigBase& config) -> std::unique_ptr<task::IHookTask> {
                if (const auto* mock_config = dynamic_cast<const MockConfig*>(&config)) {
                    return std::make_unique<MockTask>(mock_config->key());
                }
                return nullptr;
            }
        );
    }
    
    void TearDown() override {
        // Clean up registered loaders
        auto loaders = config::ConfigFactory::get_registered_loaders();
        for (const auto& loader_name : loaders) {
            bool unregistered = config::ConfigFactory::unregister_loader(loader_name);
            (void)unregistered; // Suppress unused variable warning
        }
        
        // Clean up test directory
        std::filesystem::remove_all(test_dir_);
    }
    
    void CreateTasksTomlFile(const std::string& content) {
        std::ofstream file(test_dir_ / "tasks.toml");
        file << content;
        file.close();
    }
    
    void CreateTaskConfigFile(const std::string& filename, const std::string& content) {
        std::ofstream file(tasks_dir_ / filename);
        file << content;
        file.close();
    }
    
    std::filesystem::path test_dir_;
    std::filesystem::path tasks_dir_;
};

TEST_F(HookFactoryTest, CreateHooksFromTasks_Success) {
    // Create tasks.toml file
    CreateTasksTomlFile(R"(
[metadata]
version = "1.0.0"
description = "Test configuration"

[tasks.test_task]
name = "Test Task"
description = "A test task"
type = "memory"
config_file = "tasks/test_config.toml"
enabled = true
)");
    
    // Create task config file
    CreateTaskConfigFile("test_config.toml", R"(
[metadata]
description = "Test config"

[memory.test_memory]
source_address = 0x401000
destination_address = 0x402000
size = 100
)");
    
    HookManager manager;
    std::string tasks_path = (test_dir_ / "tasks.toml").string();
    
    auto result = HookFactory::create_hooks_from_tasks(tasks_path, manager);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(manager.hook_count(), 0);
}

TEST_F(HookFactoryTest, CreateHooksFromTasks_TasksFileNotFound) {
    HookManager manager;
    std::string tasks_path = (test_dir_ / "nonexistent_tasks.toml").string();
    
    auto result = HookFactory::create_hooks_from_tasks(tasks_path, manager);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FactoryError::config_load_failed);
}

TEST_F(HookFactoryTest, CreateHooksFromTasks_ConfigFileNotFound) {
    // Create tasks.toml file with reference to non-existent config file
    CreateTasksTomlFile(R"(
[metadata]
version = "1.0.0"

[tasks.test_task]
name = "Test Task"
type = "memory"
config_file = "tasks/nonexistent_config.toml"
enabled = true
)");
    
    HookManager manager;
    std::string tasks_path = (test_dir_ / "tasks.toml").string();
    
    auto result = HookFactory::create_hooks_from_tasks(tasks_path, manager);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FactoryError::config_load_failed);
}

TEST_F(HookFactoryTest, CreateHooksFromTasks_PathResolution) {
    // Create tasks.toml file
    CreateTasksTomlFile(R"(
[metadata]
version = "1.0.0"

[tasks.path_test]
name = "Path Test Task"
type = "memory"
config_file = "tasks/path_test_config.toml"
enabled = true
)");
    
    // Create config file in tasks subdirectory
    CreateTaskConfigFile("path_test_config.toml", R"(
[metadata]
description = "Path test config"

[memory.path_test]
source_address = 0x401000
destination_address = 0x402000
size = 100
)");
    
    HookManager manager;
    std::string tasks_path = (test_dir_ / "tasks.toml").string();
    
    auto result = HookFactory::create_hooks_from_tasks(tasks_path, manager);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(manager.hook_count(), 0);
}

TEST_F(HookFactoryTest, CreateHooksFromTasks_DisabledTask) {
    // Create tasks.toml file with disabled task
    CreateTasksTomlFile(R"(
[metadata]
version = "1.0.0"

[tasks.disabled_task]
name = "Disabled Task"
type = "memory"
config_file = "tasks/disabled_config.toml"
enabled = false
)");
    
    // Create config file (shouldn't be loaded)
    CreateTaskConfigFile("disabled_config.toml", R"(
[metadata]
description = "Disabled config"

[memory.disabled]
source_address = 0x401000
destination_address = 0x402000
size = 100
)");
    
    HookManager manager;
    std::string tasks_path = (test_dir_ / "tasks.toml").string();
    
    auto result = HookFactory::create_hooks_from_tasks(tasks_path, manager);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(manager.hook_count(), 0); // No hooks should be created for disabled tasks
}

TEST_F(HookFactoryTest, CreateHooksFromTasks_DependencyOrder) {
    // Create tasks.toml file with dependencies
    CreateTasksTomlFile(R"(
[metadata]
version = "1.0.0"

[tasks.parent_task]
name = "Parent Task"
type = "memory"
config_file = "tasks/parent_config.toml"
followBy = ["child_task"]
enabled = true

[tasks.child_task]
name = "Child Task"
type = "memory"
config_file = "tasks/child_config.toml"
enabled = true
)");
    
    // Create config files
    CreateTaskConfigFile("parent_config.toml", R"(
[metadata]
description = "Parent config"

[memory.parent]
source_address = 0x401000
destination_address = 0x402000
size = 100
)");
    
    CreateTaskConfigFile("child_config.toml", R"(
[metadata]
description = "Child config"

[memory.child]
source_address = 0x403000
destination_address = 0x404000
size = 100
)");
    
    HookManager manager;
    std::string tasks_path = (test_dir_ / "tasks.toml").string();
    
    auto result = HookFactory::create_hooks_from_tasks(tasks_path, manager);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(manager.hook_count(), 0);
}

TEST_F(HookFactoryTest, CreateHooksFromTasks_MultipleTasks) {
    // Create tasks.toml file with multiple tasks
    CreateTasksTomlFile(R"(
[metadata]
version = "1.0.0"

[tasks.task1]
name = "Task 1"
type = "memory"
config_file = "tasks/task1_config.toml"
enabled = true

[tasks.task2]
name = "Task 2"
type = "memory"
config_file = "tasks/task2_config.toml"
enabled = true
)");
    
    // Create config files
    CreateTaskConfigFile("task1_config.toml", R"(
[metadata]
description = "Task 1 config"

[memory.task1]
source_address = 0x401000
destination_address = 0x402000
size = 100
)");
    
    CreateTaskConfigFile("task2_config.toml", R"(
[metadata]
description = "Task 2 config"

[memory.task2]
source_address = 0x403000
destination_address = 0x404000
size = 100
)");
    
    HookManager manager;
    std::string tasks_path = (test_dir_ / "tasks.toml").string();
    
    auto result = HookFactory::create_hooks_from_tasks(tasks_path, manager);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(manager.hook_count(), 0);
}

TEST_F(HookFactoryTest, CreateHooksFromTasks_InvalidTasksToml) {
    // Create invalid tasks.toml file
    CreateTasksTomlFile(R"(
[metadata]
version = "1.0.0"

[tasks.invalid_task
name = "Invalid Task"
type = "memory"
config_file = "tasks/invalid_config.toml"
enabled = true
)"); // Missing closing bracket
    
    HookManager manager;
    std::string tasks_path = (test_dir_ / "tasks.toml").string();
    
    auto result = HookFactory::create_hooks_from_tasks(tasks_path, manager);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FactoryError::config_load_failed);
}

TEST_F(HookFactoryTest, CreateHooksFromTasks_ConfigLoaderFailure) {
    // Register a failing config loader
    auto failing_loader = std::make_unique<MockConfigLoader>("FailingLoader", config::ConfigType::Patch, true);
    bool failing_registered = config::ConfigFactory::register_loader(std::move(failing_loader));
    (void)failing_registered; // Suppress unused variable warning
    
    // Create tasks.toml file
    CreateTasksTomlFile(R"(
[metadata]
version = "1.0.0"

[tasks.failing_task]
name = "Failing Task"
type = "patch"
config_file = "tasks/failing_config.toml"
enabled = true
)");
    
    // Create config file (will still fail due to mock loader)
    CreateTaskConfigFile("failing_config.toml", R"(
[metadata]
description = "Failing config"

[patch.failing]
address = 0x401000
data = "90"
)");
    
    HookManager manager;
    std::string tasks_path = (test_dir_ / "tasks.toml").string();
    
    auto result = HookFactory::create_hooks_from_tasks(tasks_path, manager);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FactoryError::config_load_failed);
}

TEST_F(HookFactoryTest, CreateHooksFromTasks_EmptyTasksFile) {
    // Create empty tasks.toml file
    CreateTasksTomlFile(R"(
[metadata]
version = "1.0.0"
description = "Empty tasks file"
)");
    
    HookManager manager;
    std::string tasks_path = (test_dir_ / "tasks.toml").string();
    
    auto result = HookFactory::create_hooks_from_tasks(tasks_path, manager);
    
    EXPECT_TRUE(result.has_value()); // Should succeed but create no hooks
    EXPECT_EQ(manager.hook_count(), 0);
}

TEST_F(HookFactoryTest, CreateHooksFromConfigs_Success) {
    std::vector<config::ConfigPtr> configs;
    configs.push_back(std::make_shared<MockConfig>("test_config", "Test Config", 0x401000));
    
    HookManager manager;
    
    auto result = HookFactory::create_hooks_from_configs(configs, manager);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(manager.hook_count(), 0);
}

TEST_F(HookFactoryTest, CreateHooksFromConfigs_EmptyConfigs) {
    std::vector<config::ConfigPtr> configs; // Empty
    
    HookManager manager;
    
    auto result = HookFactory::create_hooks_from_configs(configs, manager);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(manager.hook_count(), 0);
}

TEST_F(HookFactoryTest, PathResolution_AbsoluteConfigPath) {
    // Test that absolute paths in config_file are handled correctly
    std::string absolute_config_path = (tasks_dir_ / "absolute_config.toml").string();
    
    // Replace backslashes with forward slashes for TOML compatibility
    std::replace(absolute_config_path.begin(), absolute_config_path.end(), '\\', '/');
    
    CreateTasksTomlFile(R"(
[metadata]
version = "1.0.0"

[tasks.absolute_path_task]
name = "Absolute Path Task"
type = "memory"
config_file = ")" + absolute_config_path + R"("
enabled = true
)");
    
    CreateTaskConfigFile("absolute_config.toml", R"(
[metadata]
description = "Absolute path config"

[memory.absolute]
source_address = 0x401000
destination_address = 0x402000
size = 100
)");
    
    HookManager manager;
    std::string tasks_path = (test_dir_ / "tasks.toml").string();
    
    auto result = HookFactory::create_hooks_from_tasks(tasks_path, manager);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(manager.hook_count(), 0);
}

TEST_F(HookFactoryTest, PathResolution_NestedSubdirectories) {
    // Test path resolution with nested subdirectories
    std::filesystem::path nested_dir = tasks_dir_ / "nested" / "subdir";
    std::filesystem::create_directories(nested_dir);
    
    CreateTasksTomlFile(R"(
[metadata]
version = "1.0.0"

[tasks.nested_task]
name = "Nested Task"
type = "memory"
config_file = "tasks/nested/subdir/nested_config.toml"
enabled = true
)");
    
    // Create config file in nested subdirectory
    std::ofstream nested_file(nested_dir / "nested_config.toml");
    nested_file << R"(
[metadata]
description = "Nested config"

[memory.nested]
source_address = 0x401000
destination_address = 0x402000
size = 100
)";
    nested_file.close();
    
    HookManager manager;
    std::string tasks_path = (test_dir_ / "tasks.toml").string();
    
    auto result = HookFactory::create_hooks_from_tasks(tasks_path, manager);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(manager.hook_count(), 0);
}

TEST_F(HookFactoryTest, PathResolution_RelativePathWithDotDot) {
    // Test path resolution with "../" in relative paths
    // Create a sibling directory at the same level as test_dir_
    std::filesystem::path parent_dir = test_dir_.parent_path();
    std::filesystem::path sibling_dir = parent_dir / "hook_factory_sibling";
    std::filesystem::create_directories(sibling_dir);
    
    CreateTasksTomlFile(R"(
[metadata]
version = "1.0.0"

[tasks.relative_task]
name = "Relative Task"
type = "memory"
config_file = "../hook_factory_sibling/relative_config.toml"
enabled = true
)");
    
    // Create config file in sibling directory
    std::ofstream sibling_file(sibling_dir / "relative_config.toml");
    sibling_file << R"(
[metadata]
description = "Relative config"

[memory.relative]
source_address = 0x401000
destination_address = 0x402000
size = 100
)";
    sibling_file.close();
    
    HookManager manager;
    std::string tasks_path = (test_dir_ / "tasks.toml").string();
    
    auto result = HookFactory::create_hooks_from_tasks(tasks_path, manager);
    
    // Clean up sibling directory
    std::filesystem::remove_all(sibling_dir);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(manager.hook_count(), 0);
}

TEST_F(HookFactoryTest, PathResolution_WorkingDirectoryIndependence) {
    // Test that path resolution works regardless of current working directory
    std::filesystem::path original_cwd = std::filesystem::current_path();
    
    // Change to a different directory
    std::filesystem::path temp_cwd = std::filesystem::temp_directory_path();
    std::error_code ec;
    std::filesystem::current_path(temp_cwd, ec);
    (void)ec; // Suppress unused variable warning
    
    CreateTasksTomlFile(R"(
[metadata]
version = "1.0.0"

[tasks.cwd_test]
name = "CWD Test Task"
type = "memory"
config_file = "tasks/cwd_config.toml"
enabled = true
)");
    
    CreateTaskConfigFile("cwd_config.toml", R"(
[metadata]
description = "CWD test config"

[memory.cwd_test]
source_address = 0x401000
destination_address = 0x402000
size = 100
)");
    
    HookManager manager;
    std::string tasks_path = (test_dir_ / "tasks.toml").string();
    
    auto result = HookFactory::create_hooks_from_tasks(tasks_path, manager);
    
    // Restore original working directory
    std::error_code restore_ec;
    std::filesystem::current_path(original_cwd, restore_ec);
    (void)restore_ec; // Suppress unused variable warning
    
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(manager.hook_count(), 0);
}

TEST_F(HookFactoryTest, PathResolution_ConfigInTasksDirectoryRoot) {
    // Test config files placed directly in the config directory (not in tasks/ subdirectory)
    CreateTasksTomlFile(R"(
[metadata]
version = "1.0.0"

[tasks.root_config_task]
name = "Root Config Task"
type = "memory"
config_file = "root_config.toml"
enabled = true
)");
    
    // Create config file in the test directory root (same level as tasks.toml)
    std::ofstream root_file(test_dir_ / "root_config.toml");
    root_file << R"(
[metadata]
description = "Root config"

[memory.root_config]
source_address = 0x401000
destination_address = 0x402000
size = 100
)";
    root_file.close();
    
    HookManager manager;
    std::string tasks_path = (test_dir_ / "tasks.toml").string();
    
    auto result = HookFactory::create_hooks_from_tasks(tasks_path, manager);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(manager.hook_count(), 0);
}

} // namespace app_hook::hook 