#include <gtest/gtest.h>
#include "task/hook_task.hpp"

namespace app_hook::task {

// Concrete implementation for testing the interface
class TestHookTask : public IHookTask {
public:
    TestHookTask(std::string name, std::string desc, TaskError error = TaskError::success)
        : name_(std::move(name)), description_(std::move(desc)), error_to_return_(error), execute_count_(0) {}
    
    TaskResult execute() override {
        ++execute_count_;
        if (error_to_return_ == TaskError::success) {
            return {};
        } else {
            return std::unexpected(error_to_return_);
        }
    }
    
    std::string name() const override { return name_; }
    std::string description() const override { return description_; }
    
    int get_execute_count() const { return execute_count_; }
    void set_error_to_return(TaskError error) { error_to_return_ = error; }
    
private:
    std::string name_;
    std::string description_;
    TaskError error_to_return_;
    int execute_count_;
};

class HookTaskTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup
    }
    
    void TearDown() override {
        // Test cleanup
    }
};

TEST_F(HookTaskTest, TaskErrorEnum) {
    // Test that task error enum values are defined
    EXPECT_EQ(static_cast<int>(TaskError::success), 0);
    EXPECT_NE(static_cast<int>(TaskError::invalid_config), 0);
    EXPECT_NE(static_cast<int>(TaskError::memory_allocation_failed), 0);
}

TEST_F(HookTaskTest, BasicTaskInterface) {
    TestHookTask task("TestTask", "Test description");
    
    EXPECT_EQ(task.name(), "TestTask");
    EXPECT_EQ(task.description(), "Test description");
    EXPECT_EQ(task.get_execute_count(), 0);
}

TEST_F(HookTaskTest, SuccessfulExecution) {
    TestHookTask task("SuccessTask", "Success test");
    
    auto result = task.execute();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(task.get_execute_count(), 1);
}

TEST_F(HookTaskTest, FailedExecution) {
    TestHookTask task("FailTask", "Fail test", TaskError::invalid_config);
    
    auto result = task.execute();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TaskError::invalid_config);
    EXPECT_EQ(task.get_execute_count(), 1);
}

} // namespace app_hook::task 