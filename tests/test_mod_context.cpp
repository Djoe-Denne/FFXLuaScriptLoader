#include <gtest/gtest.h>
#include "context/mod_context.hpp"
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <future>
#include <chrono>

namespace app_hook::context {

class ModContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing data before each test
        auto& context = ModContext::instance();
        auto keys = context.get_all_keys();
        for (const auto& key : keys) {
            context.remove_data(key);
        }
    }
    
    void TearDown() override {
        // Clean up after each test
        auto& context = ModContext::instance();
        auto keys = context.get_all_keys();
        for (const auto& key : keys) {
            context.remove_data(key);
        }
    }
};

// Test data structures
struct TestData {
    int value;
    std::string name;
    
    TestData(int v, std::string n) : value(v), name(std::move(n)) {}
    
    bool operator==(const TestData& other) const {
        return value == other.value && name == other.name;
    }
};

class MoveOnlyData {
public:
    explicit MoveOnlyData(int val) : value_(val) {}
    
    // Non-copyable
    MoveOnlyData(const MoveOnlyData&) = delete;
    MoveOnlyData& operator=(const MoveOnlyData&) = delete;
    
    // Movable
    MoveOnlyData(MoveOnlyData&&) = default;
    MoveOnlyData& operator=(MoveOnlyData&&) = default;
    
    int get_value() const { return value_; }
    
private:
    int value_;
};

TEST_F(ModContextTest, SingletonInstance) {
    auto& context1 = ModContext::instance();
    auto& context2 = ModContext::instance();
    
    // Both references should point to the same instance
    EXPECT_EQ(&context1, &context2);
}

TEST_F(ModContextTest, StoreAndGetCopyableData) {
    auto& context = ModContext::instance();
    
    const std::string key = "test.copyable";
    const int value = 42;
    
    context.store_data(key, value);
    
    auto* retrieved = context.get_data<int>(key);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(*retrieved, value);
}

TEST_F(ModContextTest, StoreAndGetString) {
    auto& context = ModContext::instance();
    
    const std::string key = "test.string";
    const std::string value = "Hello, World!";
    
    context.store_data(key, value);
    
    auto* retrieved = context.get_data<std::string>(key);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(*retrieved, value);
}

TEST_F(ModContextTest, StoreAndGetStringMove) {
    auto& context = ModContext::instance();
    
    const std::string key = "test.string.move";
    std::string value = "Movable string";
    std::string value_copy = value;
    
    context.store_data(key, std::move(value));
    
    auto* retrieved = context.get_data<std::string>(key);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(*retrieved, value_copy);
    EXPECT_TRUE(value.empty()); // Original should be moved from
}

TEST_F(ModContextTest, StoreAndGetCustomStruct) {
    auto& context = ModContext::instance();
    
    const std::string key = "test.struct";
    TestData data(123, "test_name");
    
    context.store_data(key, data);
    
    auto* retrieved = context.get_data<TestData>(key);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(*retrieved, data);
}

TEST_F(ModContextTest, StoreAndGetMoveOnlyData) {
    auto& context = ModContext::instance();
    
    const std::string key = "test.move_only";
    const int expected_value = 999;
    
    MoveOnlyData data(expected_value);
    context.store_data(key, std::move(data));
    
    auto* retrieved = context.get_data<MoveOnlyData>(key);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->get_value(), expected_value);
}

TEST_F(ModContextTest, StoreAndGetConstData) {
    auto& context = ModContext::instance();
    
    const std::string key = "test.const";
    const double value = 3.14159;
    
    context.store_data(key, value);
    
    const auto& const_context = context;
    auto* retrieved = const_context.get_data<double>(key);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_DOUBLE_EQ(*retrieved, value);
}

TEST_F(ModContextTest, HasData) {
    auto& context = ModContext::instance();
    
    const std::string key1 = "test.exists";
    const std::string key2 = "test.missing";
    
    EXPECT_FALSE(context.has_data(key1));
    EXPECT_FALSE(context.has_data(key2));
    
    context.store_data(key1, 42);
    
    EXPECT_TRUE(context.has_data(key1));
    EXPECT_FALSE(context.has_data(key2));
}

TEST_F(ModContextTest, GetDataTypeInfo) {
    auto& context = ModContext::instance();
    
    const std::string key = "test.type_info";
    const int value = 100;
    
    context.store_data(key, value);
    
    auto* type_info = context.get_data_type(key);
    ASSERT_NE(type_info, nullptr);
    EXPECT_EQ(*type_info, typeid(int));
}

TEST_F(ModContextTest, GetDataTypeInfoMissing) {
    auto& context = ModContext::instance();
    
    const std::string key = "test.missing";
    
    auto* type_info = context.get_data_type(key);
    EXPECT_EQ(type_info, nullptr);
}

TEST_F(ModContextTest, RemoveData) {
    auto& context = ModContext::instance();
    
    const std::string key = "test.removable";
    const std::string value = "to be removed";
    
    context.store_data(key, value);
    EXPECT_TRUE(context.has_data(key));
    
    bool removed = context.remove_data(key);
    EXPECT_TRUE(removed);
    EXPECT_FALSE(context.has_data(key));
    
    auto* retrieved = context.get_data<std::string>(key);
    EXPECT_EQ(retrieved, nullptr);
}

TEST_F(ModContextTest, RemoveNonExistentData) {
    auto& context = ModContext::instance();
    
    const std::string key = "test.non_existent";
    
    bool removed = context.remove_data(key);
    EXPECT_FALSE(removed);
}

TEST_F(ModContextTest, GetAllKeys) {
    auto& context = ModContext::instance();
    
    const std::vector<std::string> keys = {
        "test.key1", "test.key2", "test.key3"
    };
    
    // Store data with different keys
    for (size_t i = 0; i < keys.size(); ++i) {
        context.store_data(keys[i], static_cast<int>(i));
    }
    
    auto all_keys = context.get_all_keys();
    EXPECT_EQ(all_keys.size(), keys.size());
    
    // Check that all keys are present
    for (const auto& key : keys) {
        EXPECT_TRUE(std::find(all_keys.begin(), all_keys.end(), key) != all_keys.end());
    }
}

TEST_F(ModContextTest, GetAllKeysEmpty) {
    auto& context = ModContext::instance();
    
    auto all_keys = context.get_all_keys();
    EXPECT_TRUE(all_keys.empty());
}

TEST_F(ModContextTest, TypeMismatchReturnsNull) {
    auto& context = ModContext::instance();
    
    const std::string key = "test.type_mismatch";
    const int stored_value = 42;
    
    context.store_data(key, stored_value);
    
    // Try to retrieve as wrong type
    auto* wrong_type = context.get_data<std::string>(key);
    EXPECT_EQ(wrong_type, nullptr);
    
    // Correct type should still work
    auto* correct_type = context.get_data<int>(key);
    ASSERT_NE(correct_type, nullptr);
    EXPECT_EQ(*correct_type, stored_value);
}

TEST_F(ModContextTest, OverwriteExistingData) {
    auto& context = ModContext::instance();
    
    const std::string key = "test.overwrite";
    const int initial_value = 100;
    const int new_value = 200;
    
    context.store_data(key, initial_value);
    
    auto* retrieved1 = context.get_data<int>(key);
    ASSERT_NE(retrieved1, nullptr);
    EXPECT_EQ(*retrieved1, initial_value);
    
    // Overwrite with new value
    context.store_data(key, new_value);
    
    auto* retrieved2 = context.get_data<int>(key);
    ASSERT_NE(retrieved2, nullptr);
    EXPECT_EQ(*retrieved2, new_value);
}

TEST_F(ModContextTest, ThreadSafety) {
    auto& context = ModContext::instance();
    const int num_threads = 10;
    const int operations_per_thread = 100;
    
    std::vector<std::future<void>> futures;
    
    // Launch multiple threads that store and retrieve data
    for (int thread_id = 0; thread_id < num_threads; ++thread_id) {
        futures.emplace_back(std::async(std::launch::async, [&context, thread_id, operations_per_thread]() {
            for (int i = 0; i < operations_per_thread; ++i) {
                std::string key = "thread." + std::to_string(thread_id) + ".item." + std::to_string(i);
                int value = thread_id * 1000 + i;
                
                // Store data
                context.store_data(key, value);
                
                // Retrieve and verify
                auto* retrieved = context.get_data<int>(key);
                EXPECT_NE(retrieved, nullptr);
                if (retrieved) {
                    EXPECT_EQ(*retrieved, value);
                }
                
                // Check existence
                EXPECT_TRUE(context.has_data(key));
            }
        }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    // Verify all data was stored correctly
    auto all_keys = context.get_all_keys();
    EXPECT_EQ(all_keys.size(), num_threads * operations_per_thread);
}

TEST_F(ModContextTest, ThreadSafetyRemoval) {
    auto& context = ModContext::instance();
    const int num_items = 1000;
    
    // Store initial data
    for (int i = 0; i < num_items; ++i) {
        std::string key = "remove.test." + std::to_string(i);
        context.store_data(key, i);
    }
    
    std::vector<std::future<int>> futures;
    
    // Launch multiple threads that try to remove data
    for (int i = 0; i < num_items; ++i) {
        futures.emplace_back(std::async(std::launch::async, [&context, i]() {
            std::string key = "remove.test." + std::to_string(i);
            return context.remove_data(key) ? 1 : 0;
        }));
    }
    
    // Count successful removals
    int successful_removals = 0;
    for (auto& future : futures) {
        successful_removals += future.get();
    }
    
    // All items should have been removed exactly once
    EXPECT_EQ(successful_removals, num_items);
    
    // Context should be empty
    auto all_keys = context.get_all_keys();
    EXPECT_TRUE(all_keys.empty());
}

TEST_F(ModContextTest, NonCopyableNonMovable) {
    // Verify that ModContext cannot be copied or moved
    EXPECT_FALSE(std::is_copy_constructible_v<ModContext>);
    EXPECT_FALSE(std::is_copy_assignable_v<ModContext>);
    EXPECT_FALSE(std::is_move_constructible_v<ModContext>);
    EXPECT_FALSE(std::is_move_assignable_v<ModContext>);
}

TEST_F(ModContextTest, LargeDataStorage) {
    auto& context = ModContext::instance();
    
    const std::string key = "test.large_vector";
    const size_t vector_size = 10000;
    
    std::vector<int> large_vector;
    large_vector.reserve(vector_size);
    for (size_t i = 0; i < vector_size; ++i) {
        large_vector.push_back(static_cast<int>(i));
    }
    
    context.store_data(key, std::move(large_vector));
    
    auto* retrieved = context.get_data<std::vector<int>>(key);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->size(), vector_size);
    
    // Verify some values
    EXPECT_EQ((*retrieved)[0], 0);
    EXPECT_EQ((*retrieved)[vector_size - 1], static_cast<int>(vector_size - 1));
}

TEST_F(ModContextTest, KeyWithSpecialCharacters) {
    auto& context = ModContext::instance();
    
    const std::string key = "test.key.with.dots-and_underscores@symbols#123";
    const std::string value = "Special key test";
    
    context.store_data(key, value);
    
    EXPECT_TRUE(context.has_data(key));
    
    auto* retrieved = context.get_data<std::string>(key);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(*retrieved, value);
}

} // namespace app_hook::context 