/**
 * @file test_hook_task.cpp
 * @brief Unit tests for HookTask interface and related functionality
 * 
 * Tests the task system including:
 * - Task interface implementation
 * - Error handling with std::expected
 * - Task factory functions
 * - Concept validation
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "task/hook_task.hpp"
#include <memory>
#include <string>

using namespace app_hook::task;
using namespace ::testing;

namespace {

/// @brief Mock implementation of IHookTask for testing
class MockHookTask : public IHookTask {
public:
    MOCK_METHOD(TaskResult, execute, (), (override));
    MOCK_METHOD(std::string, name, (), (const, override));
    MOCK_METHOD(std::string, description, (), (const, override));
};

/// @brief Test implementation of IHookTask for testing
class TestHookTask : public IHookTask {
public:
    explicit TestHookTask(std::string task_name, std::string task_description, TaskError result_error = TaskError::success)
        : task_name_(std::move(task_name))
        , task_description_(std::move(task_description))
        , result_error_(result_error)
        , execute_count_(0) {}
    
    [[nodiscard]] TaskResult execute() override {
        ++execute_count_;
        if (result_error_ == TaskError::success) {
            return {};  // Success
        }
        return std::unexpected(result_error_);
    }
    
    [[nodiscard]] std::string name() const override {
        return task_name_;
    }
    
    [[nodiscard]] std::string description() const override {
        return task_description_;
    }
    
    [[nodiscard]] int execute_count() const noexcept { return execute_count_; }
    
    void set_result_error(TaskError error) noexcept { result_error_ = error; }

private:
    std::string task_name_;
    std::string task_description_;
    TaskError result_error_;
    int execute_count_;
};

/// @brief Test class that satisfies HookTask concept
class ConceptTestTask : public IHookTask {
public:
    [[nodiscard]] TaskResult execute() override { return {}; }
    [[nodiscard]] std::string name() const override { return "ConceptTestTask"; }
    [[nodiscard]] std::string description() const override { return "Test task for concept validation"; }
};

/// @brief Test class that does NOT satisfy HookTask concept (missing methods)
class InvalidConceptTask {
public:
    void execute() {}  // Wrong return type
    // Missing name() and description() methods
};

} // anonymous namespace

/// @brief Test fixture for HookTask tests
class HookTaskTest : public Test {
protected:
    void SetUp() override {
        success_task_ = std::make_unique<TestHookTask>("SuccessTask", "A task that succeeds");
        failure_task_ = std::make_unique<TestHookTask>("FailureTask", "A task that fails", TaskError::invalid_config);
    }

    std::unique_ptr<TestHookTask> success_task_;
    std::unique_ptr<TestHookTask> failure_task_;
};

// =============================================================================
// TaskError Enum Tests
// =============================================================================

TEST(TaskErrorTest, ErrorValues) {
    // Test that error enum values are as expected
    EXPECT_EQ(static_cast<int>(TaskError::success), 0);
    EXPECT_NE(static_cast<int>(TaskError::invalid_config), 0);
    EXPECT_NE(static_cast<int>(TaskError::memory_allocation_failed), 0);
    EXPECT_NE(static_cast<int>(TaskError::invalid_address), 0);
    EXPECT_NE(static_cast<int>(TaskError::copy_failed), 0);
    EXPECT_NE(static_cast<int>(TaskError::dependency_not_met), 0);
    EXPECT_NE(static_cast<int>(TaskError::patch_failed), 0);
    EXPECT_NE(static_cast<int>(TaskError::unknown_error), 0);
}

// =============================================================================
// TaskResult (std::expected) Tests
// =============================================================================

TEST(TaskResultTest, SuccessResult) {
    TaskResult success_result{};
    
    EXPECT_TRUE(success_result.has_value());
    EXPECT_FALSE(!success_result);  // Test operator bool
}

TEST(TaskResultTest, ErrorResult) {
    TaskResult error_result = std::unexpected(TaskError::invalid_config);
    
    EXPECT_FALSE(error_result.has_value());
    EXPECT_TRUE(!error_result);  // Test operator bool
    EXPECT_EQ(error_result.error(), TaskError::invalid_config);
}

TEST(TaskResultTest, ErrorComparisons) {
    const auto error1 = std::unexpected(TaskError::invalid_config);
    const auto error2 = std::unexpected(TaskError::memory_allocation_failed);
    const auto error3 = std::unexpected(TaskError::invalid_config);
    
    EXPECT_NE(error1.error(), error2.error());
    EXPECT_EQ(error1.error(), error3.error());
}

// =============================================================================
// IHookTask Interface Tests
// =============================================================================

TEST_F(HookTaskTest, BasicTaskExecution) {
    // Test successful execution
    const auto result = success_task_->execute();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(success_task_->execute_count(), 1);
    
    // Test failed execution
    const auto failure_result = failure_task_->execute();
    EXPECT_FALSE(failure_result.has_value());
    EXPECT_EQ(failure_result.error(), TaskError::invalid_config);
    EXPECT_EQ(failure_task_->execute_count(), 1);
}

TEST_F(HookTaskTest, TaskProperties) {
    EXPECT_EQ(success_task_->name(), "SuccessTask");
    EXPECT_EQ(success_task_->description(), "A task that succeeds");
    
    EXPECT_EQ(failure_task_->name(), "FailureTask");
    EXPECT_EQ(failure_task_->description(), "A task that fails");
}

TEST_F(HookTaskTest, MultipleExecutions) {
    // Execute multiple times
    for (int i = 0; i < 5; ++i) {
        const auto result = success_task_->execute();
        EXPECT_TRUE(result.has_value());
    }
    
    EXPECT_EQ(success_task_->execute_count(), 5);
}

TEST_F(HookTaskTest, DynamicErrorChanges) {
    // Start with success
    auto result = success_task_->execute();
    EXPECT_TRUE(result.has_value());
    
    // Change to error
    success_task_->set_result_error(TaskError::memory_allocation_failed);
    result = success_task_->execute();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TaskError::memory_allocation_failed);
    
    // Change back to success
    success_task_->set_result_error(TaskError::success);
    result = success_task_->execute();
    EXPECT_TRUE(result.has_value());
}

// =============================================================================
// Mock Tests
// =============================================================================

TEST(MockHookTaskTest, MockInteractions) {
    auto mock_task = std::make_unique<MockHookTask>();
    
    // Set up expectations
    EXPECT_CALL(*mock_task, name())
        .WillRepeatedly(Return("MockTask"));
        
    EXPECT_CALL(*mock_task, description())
        .WillRepeatedly(Return("Mock task description"));
        
    EXPECT_CALL(*mock_task, execute())
        .WillOnce(Return(TaskResult{}))  // Success
        .WillOnce(Return(std::unexpected(TaskError::patch_failed)));  // Failure
    
    // Test the mock
    EXPECT_EQ(mock_task->name(), "MockTask");
    EXPECT_EQ(mock_task->description(), "Mock task description");
    
    auto result1 = mock_task->execute();
    EXPECT_TRUE(result1.has_value());
    
    auto result2 = mock_task->execute();
    EXPECT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error(), TaskError::patch_failed);
}

// =============================================================================
// Factory Function Tests
// =============================================================================

TEST(TaskFactoryTest, MakeTaskFunction) {
    auto task = make_task<TestHookTask>("FactoryTask", "Created by factory", TaskError::dependency_not_met);
    
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->name(), "FactoryTask");
    EXPECT_EQ(task->description(), "Created by factory");
    
    const auto result = task->execute();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TaskError::dependency_not_met);
}

TEST(TaskFactoryTest, MakeTaskPolymorphism) {
    HookTaskPtr task = make_task<TestHookTask>("PolymorphicTask", "Test polymorphism");
    
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->name(), "PolymorphicTask");
    
    const auto result = task->execute();
    EXPECT_TRUE(result.has_value());
}

// =============================================================================
// Concept Tests
// =============================================================================

TEST(ConceptTest, ValidHookTaskConcept) {
    // This should compile if the concept is correctly implemented
    static_assert(HookTask<ConceptTestTask>, "ConceptTestTask should satisfy HookTask concept");
    static_assert(HookTask<TestHookTask>, "TestHookTask should satisfy HookTask concept");
    static_assert(HookTask<MockHookTask>, "MockHookTask should satisfy HookTask concept");
}

TEST(ConceptTest, InvalidHookTaskConcept) {
    // This should NOT satisfy the concept
    static_assert(!HookTask<InvalidConceptTask>, "InvalidConceptTask should NOT satisfy HookTask concept");
    static_assert(!HookTask<int>, "int should NOT satisfy HookTask concept");
    static_assert(!HookTask<std::string>, "std::string should NOT satisfy HookTask concept");
}

TEST(ConceptTest, ConceptUsageInFactory) {
    // Test that the factory function works with concept-satisfying types
    auto concept_task = make_task<ConceptTestTask>();
    ASSERT_NE(concept_task, nullptr);
    EXPECT_EQ(concept_task->name(), "ConceptTestTask");
}

// =============================================================================
// Polymorphic Behavior Tests
// =============================================================================

TEST(PolymorphismTest, VirtualFunctionCalls) {
    std::vector<std::unique_ptr<IHookTask>> tasks;
    
    tasks.push_back(std::make_unique<TestHookTask>("Task1", "First test task"));
    tasks.push_back(std::make_unique<TestHookTask>("Task2", "Second test task", TaskError::copy_failed));
    
    // Test polymorphic calls
    EXPECT_EQ(tasks[0]->name(), "Task1");
    EXPECT_EQ(tasks[1]->name(), "Task2");
    
    const auto result1 = tasks[0]->execute();
    EXPECT_TRUE(result1.has_value());
    
    const auto result2 = tasks[1]->execute();
    EXPECT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error(), TaskError::copy_failed);
}

// =============================================================================
// Error Handling Edge Cases
// =============================================================================

TEST(ErrorHandlingTest, AllErrorTypes) {
    const std::vector<TaskError> all_errors = {
        TaskError::success,
        TaskError::invalid_config,
        TaskError::memory_allocation_failed,
        TaskError::invalid_address,
        TaskError::copy_failed,
        TaskError::dependency_not_met,
        TaskError::patch_failed,
        TaskError::unknown_error
    };
    
    for (const auto error : all_errors) {
        TestHookTask task("ErrorTest", "Testing error", error);
        const auto result = task.execute();
        
        if (error == TaskError::success) {
            EXPECT_TRUE(result.has_value());
        } else {
            EXPECT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), error);
        }
    }
}

// =============================================================================
// Memory Management Tests
// =============================================================================

TEST(MemoryManagementTest, TaskLifetime) {
    std::unique_ptr<IHookTask> task;
    
    {
        // Create task in nested scope
        auto test_task = std::make_unique<TestHookTask>("ScopeTask", "Testing scope");
        task = std::move(test_task);
    }
    
    // Task should still be valid after scope exit
    EXPECT_EQ(task->name(), "ScopeTask");
    const auto result = task->execute();
    EXPECT_TRUE(result.has_value());
}

TEST(MemoryManagementTest, MoveSemantics) {
    auto original = std::make_unique<TestHookTask>("OriginalTask", "Original description");
    const auto* original_ptr = original.get();
    
    auto moved = std::move(original);
    
    EXPECT_EQ(original.get(), nullptr);
    EXPECT_EQ(moved.get(), original_ptr);
    EXPECT_EQ(moved->name(), "OriginalTask");
} 
