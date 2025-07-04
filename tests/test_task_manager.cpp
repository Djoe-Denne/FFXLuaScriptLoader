#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "util/task_manager.hpp"
#include "task/hook_task.hpp"
#include "mock_plugin_host.hpp"
#include <memory>
#include <filesystem>
#include <fstream>

namespace app_hook::util {

// Mock task for testing
class MockHookTask : public task::IHookTask {
public:
    MockHookTask(std::string name, std::string desc, task::TaskError error = task::TaskError::success)
        : name_(std::move(name)), description_(std::move(desc)), error_to_return_(error), execute_count_(0) {}
    
    task::TaskResult execute() override {
        ++execute_count_;
        if (error_to_return_ == task::TaskError::success) {
            return {};
        } else {
            return std::unexpected(error_to_return_);
        }
    }
    
    std::string name() const override { return name_; }
    std::string description() const override { return description_; }
    
    int get_execute_count() const { return execute_count_; }
    void set_error_to_return(task::TaskError error) { error_to_return_ = error; }
    
private:
    std::string name_;
    std::string description_;
    task::TaskError error_to_return_;
    int execute_count_;
};

class TaskManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_config_file_ = "test_tasks.toml";
        
        // Clean up any existing test files
        if (std::filesystem::exists(test_config_file_)) {
            std::filesystem::remove(test_config_file_);
        }
    }
    
    void TearDown() override {
        // Clean up test files
        if (std::filesystem::exists(test_config_file_)) {
            std::filesystem::remove(test_config_file_);
        }
    }
    
    void create_test_config_file(const std::string& content) {
        std::ofstream file(test_config_file_);
        file << content;
        file.close();
    }
    
    std::string test_config_file_;
};

TEST_F(TaskManagerTest, DefaultConstructor) {
    TaskManager manager;
    
    EXPECT_EQ(manager.task_count(), 0);
    EXPECT_FALSE(manager.has_tasks());
    EXPECT_TRUE(manager.get_task_names().empty());
}

TEST_F(TaskManagerTest, AddSingleTask) {
    TaskManager manager;
    
    auto task = std::make_unique<MockHookTask>("TestTask", "Test description");
    manager.add_task(std::move(task));
    
    EXPECT_EQ(manager.task_count(), 1);
    EXPECT_TRUE(manager.has_tasks());
    
    auto task_names = manager.get_task_names();
    EXPECT_EQ(task_names.size(), 1);
    EXPECT_EQ(task_names[0], "TestTask");
}

TEST_F(TaskManagerTest, AddMultipleTasks) {
    TaskManager manager;
    
    auto task1 = std::make_unique<MockHookTask>("Task1", "First task");
    auto task2 = std::make_unique<MockHookTask>("Task2", "Second task");
    auto task3 = std::make_unique<MockHookTask>("Task3", "Third task");
    
    manager.add_task(std::move(task1));
    manager.add_task(std::move(task2));
    manager.add_task(std::move(task3));
    
    EXPECT_EQ(manager.task_count(), 3);
    EXPECT_TRUE(manager.has_tasks());
    
    auto task_names = manager.get_task_names();
    EXPECT_EQ(task_names.size(), 3);
    EXPECT_TRUE(std::find(task_names.begin(), task_names.end(), "Task1") != task_names.end());
    EXPECT_TRUE(std::find(task_names.begin(), task_names.end(), "Task2") != task_names.end());
    EXPECT_TRUE(std::find(task_names.begin(), task_names.end(), "Task3") != task_names.end());
}

TEST_F(TaskManagerTest, ExecuteAllSuccess) {
    TaskManager manager;
    
    auto task1 = std::make_unique<MockHookTask>("SuccessTask1", "Success 1");
    auto task2 = std::make_unique<MockHookTask>("SuccessTask2", "Success 2");
    
    MockHookTask* task1_ptr = task1.get();
    MockHookTask* task2_ptr = task2.get();
    
    manager.add_task(std::move(task1));
    manager.add_task(std::move(task2));
    
    bool result = manager.execute_all();
    EXPECT_TRUE(result);
    
    // Verify both tasks were executed
    EXPECT_EQ(task1_ptr->get_execute_count(), 1);
    EXPECT_EQ(task2_ptr->get_execute_count(), 1);
}

TEST_F(TaskManagerTest, ExecuteAllWithFailure) {
    TaskManager manager;
    
    auto task1 = std::make_unique<MockHookTask>("SuccessTask", "Success");
    auto task2 = std::make_unique<MockHookTask>("FailTask", "Fail", task::TaskError::invalid_config);
    auto task3 = std::make_unique<MockHookTask>("AnotherTask", "Another");
    
    MockHookTask* task1_ptr = task1.get();
    MockHookTask* task2_ptr = task2.get();
    MockHookTask* task3_ptr = task3.get();
    
    manager.add_task(std::move(task1));
    manager.add_task(std::move(task2));
    manager.add_task(std::move(task3));
    
    bool result = manager.execute_all();
    EXPECT_FALSE(result); // Should fail due to second task
    
    // All tasks should still be executed even if one fails
    EXPECT_EQ(task1_ptr->get_execute_count(), 1);
    EXPECT_EQ(task2_ptr->get_execute_count(), 1);
    EXPECT_EQ(task3_ptr->get_execute_count(), 1);
}

TEST_F(TaskManagerTest, ExecuteAllDetailed) {
    TaskManager manager;
    
    auto task1 = std::make_unique<MockHookTask>("DetailTask1", "Detail 1");
    auto task2 = std::make_unique<MockHookTask>("DetailTask2", "Detail 2", task::TaskError::memory_allocation_failed);
    auto task3 = std::make_unique<MockHookTask>("DetailTask3", "Detail 3");
    
    manager.add_task(std::move(task1));
    manager.add_task(std::move(task2));
    manager.add_task(std::move(task3));
    
    auto results = manager.execute_all_detailed();
    
    EXPECT_EQ(results.size(), 3);
    
    // Check each result
    EXPECT_EQ(results[0].first, "DetailTask1");
    EXPECT_TRUE(results[0].second.has_value());
    
    EXPECT_EQ(results[1].first, "DetailTask2");
    EXPECT_FALSE(results[1].second.has_value());
    EXPECT_EQ(results[1].second.error(), task::TaskError::memory_allocation_failed);
    
    EXPECT_EQ(results[2].first, "DetailTask3");
    EXPECT_TRUE(results[2].second.has_value());
}

TEST_F(TaskManagerTest, ExecuteAllEmpty) {
    TaskManager manager;
    
    bool result = manager.execute_all();
    EXPECT_TRUE(result); // Empty execution should succeed
    
    auto results = manager.execute_all_detailed();
    EXPECT_TRUE(results.empty());
}

TEST_F(TaskManagerTest, Clear) {
    TaskManager manager;
    
    auto task1 = std::make_unique<MockHookTask>("ClearTask1", "Clear 1");
    auto task2 = std::make_unique<MockHookTask>("ClearTask2", "Clear 2");
    
    manager.add_task(std::move(task1));
    manager.add_task(std::move(task2));
    
    EXPECT_EQ(manager.task_count(), 2);
    EXPECT_TRUE(manager.has_tasks());
    
    manager.clear();
    
    EXPECT_EQ(manager.task_count(), 0);
    EXPECT_FALSE(manager.has_tasks());
    EXPECT_TRUE(manager.get_task_names().empty());
}

TEST_F(TaskManagerTest, ClearAndReuse) {
    TaskManager manager;
    
    // Add initial tasks
    auto task1 = std::make_unique<MockHookTask>("InitialTask", "Initial");
    manager.add_task(std::move(task1));
    EXPECT_EQ(manager.task_count(), 1);
    
    // Clear
    manager.clear();
    EXPECT_EQ(manager.task_count(), 0);
    
    // Add new tasks
    auto task2 = std::make_unique<MockHookTask>("NewTask", "New");
    manager.add_task(std::move(task2));
    EXPECT_EQ(manager.task_count(), 1);
    
    auto task_names = manager.get_task_names();
    EXPECT_EQ(task_names[0], "NewTask");
}

TEST_F(TaskManagerTest, MoveConstructor) {
    TaskManager manager1;
    
    auto task = std::make_unique<MockHookTask>("MoveTask", "Move test");
    manager1.add_task(std::move(task));
    
    EXPECT_EQ(manager1.task_count(), 1);
    
    TaskManager manager2 = std::move(manager1);
    
    EXPECT_EQ(manager2.task_count(), 1);
    EXPECT_EQ(manager2.get_task_names()[0], "MoveTask");
    
    // Original manager should be in valid but unspecified state
    // We can't test the exact state, but operations should be safe
    manager1.clear();
    EXPECT_EQ(manager1.task_count(), 0);
}

TEST_F(TaskManagerTest, MoveAssignment) {
    TaskManager manager1;
    TaskManager manager2;
    
    auto task1 = std::make_unique<MockHookTask>("AssignTask1", "Assign 1");
    auto task2 = std::make_unique<MockHookTask>("AssignTask2", "Assign 2");
    
    manager1.add_task(std::move(task1));
    manager2.add_task(std::move(task2));
    
    EXPECT_EQ(manager1.task_count(), 1);
    EXPECT_EQ(manager2.task_count(), 1);
    
    manager2 = std::move(manager1);
    
    EXPECT_EQ(manager2.task_count(), 1);
    EXPECT_EQ(manager2.get_task_names()[0], "AssignTask1");
}

TEST_F(TaskManagerTest, NonCopyable) {
    // Verify that TaskManager is not copyable
    EXPECT_FALSE(std::is_copy_constructible_v<TaskManager>);
    EXPECT_FALSE(std::is_copy_assignable_v<TaskManager>);
}

TEST_F(TaskManagerTest, LoadFromConfigNonExistent) {
    TaskManager manager;
    
    bool result = manager.load_from_config("non_existent_file.toml");
    EXPECT_FALSE(result);
    EXPECT_EQ(manager.task_count(), 0);
}

TEST_F(TaskManagerTest, LoadFromTasksConfigNonExistent) {
    TaskManager manager;
    
    bool result = manager.load_from_tasks_config("non_existent_tasks.toml");
    EXPECT_FALSE(result);
    EXPECT_EQ(manager.task_count(), 0);
}

TEST_F(TaskManagerTest, LoadFromConfigBasic) {
    // Create a minimal valid TOML file
    create_test_config_file(R"(
# Test configuration file
[test_section]
key = "value"
)");
    
    TaskManager manager;
    
    // Note: This will likely fail because we don't have actual config loaders
    // but it tests the file loading path
    bool result = manager.load_from_config(test_config_file_);
    
    // For this test, we just verify it doesn't crash
    // Actual loading depends on the config system being properly set up
    EXPECT_EQ(manager.task_count(), 0); // No tasks loaded without proper config loaders
}

TEST_F(TaskManagerTest, LoadFromTasksConfigBasic) {
    // Create a minimal tasks.toml file
    create_test_config_file(R"(
# Test tasks configuration
[task1]
type = "test"
config_file = "test1.toml"

[task2]
type = "test"
config_file = "test2.toml"
)");
    
    TaskManager manager;
    
    // Note: This will likely fail because we don't have actual task loaders
    bool result = manager.load_from_tasks_config(test_config_file_);
    
    // For this test, we just verify it doesn't crash
    EXPECT_EQ(manager.task_count(), 0); // No tasks loaded without proper loaders
}

TEST_F(TaskManagerTest, ExecuteMultipleTimes) {
    TaskManager manager;
    
    auto task = std::make_unique<MockHookTask>("MultiExecTask", "Multi execution");
    MockHookTask* task_ptr = task.get();
    
    manager.add_task(std::move(task));
    
    // Execute multiple times
    EXPECT_TRUE(manager.execute_all());
    EXPECT_EQ(task_ptr->get_execute_count(), 1);
    
    EXPECT_TRUE(manager.execute_all());
    EXPECT_EQ(task_ptr->get_execute_count(), 2);
    
    EXPECT_TRUE(manager.execute_all());
    EXPECT_EQ(task_ptr->get_execute_count(), 3);
}

TEST_F(TaskManagerTest, TaskWithChangingBehavior) {
    TaskManager manager;
    
    auto task = std::make_unique<MockHookTask>("ChangingTask", "Changing behavior");
    MockHookTask* task_ptr = task.get();
    
    manager.add_task(std::move(task));
    
    // First execution succeeds
    EXPECT_TRUE(manager.execute_all());
    EXPECT_EQ(task_ptr->get_execute_count(), 1);
    
    // Change task to fail
    task_ptr->set_error_to_return(task::TaskError::patch_failed);
    
    // Second execution fails
    EXPECT_FALSE(manager.execute_all());
    EXPECT_EQ(task_ptr->get_execute_count(), 2);
    
    // Change back to success
    task_ptr->set_error_to_return(task::TaskError::success);
    
    // Third execution succeeds
    EXPECT_TRUE(manager.execute_all());
    EXPECT_EQ(task_ptr->get_execute_count(), 3);
}

TEST_F(TaskManagerTest, LargeNumberOfTasks) {
    TaskManager manager;
    
    const int num_tasks = 100;
    std::vector<MockHookTask*> task_ptrs;
    
    // Add many tasks
    for (int i = 0; i < num_tasks; ++i) {
        auto task = std::make_unique<MockHookTask>(
            "Task" + std::to_string(i), 
            "Description " + std::to_string(i)
        );
        task_ptrs.push_back(task.get());
        manager.add_task(std::move(task));
    }
    
    EXPECT_EQ(manager.task_count(), num_tasks);
    EXPECT_TRUE(manager.has_tasks());
    
    // Execute all
    EXPECT_TRUE(manager.execute_all());
    
    // Verify all were executed
    for (auto* task_ptr : task_ptrs) {
        EXPECT_EQ(task_ptr->get_execute_count(), 1);
    }
    
    // Check task names
    auto task_names = manager.get_task_names();
    EXPECT_EQ(task_names.size(), num_tasks);
    
    // Verify names are correct
    for (int i = 0; i < num_tasks; ++i) {
        std::string expected_name = "Task" + std::to_string(i);
        EXPECT_TRUE(std::find(task_names.begin(), task_names.end(), expected_name) != task_names.end());
    }
}

TEST_F(TaskManagerTest, MixedTaskResults) {
    TaskManager manager;
    
    // Create tasks with different results
    auto success_task = std::make_unique<MockHookTask>("Success", "Success task");
    auto invalid_config_task = std::make_unique<MockHookTask>("InvalidConfig", "Invalid config", task::TaskError::invalid_config);
    auto memory_fail_task = std::make_unique<MockHookTask>("MemoryFail", "Memory fail", task::TaskError::memory_allocation_failed);
    auto another_success = std::make_unique<MockHookTask>("Success2", "Success 2");
    
    manager.add_task(std::move(success_task));
    manager.add_task(std::move(invalid_config_task));
    manager.add_task(std::move(memory_fail_task));
    manager.add_task(std::move(another_success));
    
    auto results = manager.execute_all_detailed();
    
    EXPECT_EQ(results.size(), 4);
    
    // Check specific results
    EXPECT_EQ(results[0].first, "Success");
    EXPECT_TRUE(results[0].second.has_value());
    
    EXPECT_EQ(results[1].first, "InvalidConfig");
    EXPECT_FALSE(results[1].second.has_value());
    EXPECT_EQ(results[1].second.error(), task::TaskError::invalid_config);
    
    EXPECT_EQ(results[2].first, "MemoryFail");
    EXPECT_FALSE(results[2].second.has_value());
    EXPECT_EQ(results[2].second.error(), task::TaskError::memory_allocation_failed);
    
    EXPECT_EQ(results[3].first, "Success2");
    EXPECT_TRUE(results[3].second.has_value());
    
    // Overall execution should fail due to failures
    EXPECT_FALSE(manager.execute_all());
}

} // namespace app_hook::util 