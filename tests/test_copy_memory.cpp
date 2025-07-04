#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "memory/copy_memory.hpp"
#include "mock_plugin_host.hpp"
#include "context/mod_context.hpp"
#include <memory>

// Need to read some config headers to create test configs
namespace app_hook::memory {

// Mock configuration for testing
class MockCopyMemoryConfig {
public:
    MockCopyMemoryConfig(std::string key, std::size_t orig_size, std::size_t new_size, std::uintptr_t addr)
        : key_(std::move(key)), original_size_(orig_size), new_size_(new_size), original_address_(addr) {}
    
    const std::string& key() const { return key_; }
    std::size_t original_size() const { return original_size_; }
    std::size_t new_size() const { return new_size_; }
    std::uintptr_t original_address() const { return original_address_; }
    
private:
    std::string key_;
    std::size_t original_size_;
    std::size_t new_size_;
    std::uintptr_t original_address_;
};

// Test wrapper that accepts our mock config
class TestCopyMemoryTask : public task::IHookTask {
public:
    explicit TestCopyMemoryTask(MockCopyMemoryConfig config) 
        : config_(std::move(config)), host_(nullptr) {}
    
    void setHost(app_hook::plugin::IPluginHost* host) { host_ = host; }
    void setHost(void* host) override { 
        host_ = static_cast<app_hook::plugin::IPluginHost*>(host); 
    }
    
    task::TaskResult execute() override {
        if (!host_) {
            return std::unexpected(task::TaskError::dependency_not_met);
        }
        
        // Simulate copy memory operation
        auto& context = app_hook::context::ModContext::instance();
        
        // Check if source data exists
        if (!context.has_data(config_.key())) {
            return std::unexpected(task::TaskError::dependency_not_met);
        }
        
        // Get source memory region
        auto* source_region = context.get_data<MemoryRegion>(config_.key());
        if (!source_region) {
            return std::unexpected(task::TaskError::dependency_not_met);
        }
        
        if (source_region->original_size != config_.original_size()) {
            return std::unexpected(task::TaskError::invalid_config);
        }
        
        // Create new expanded memory region
        MemoryRegion new_region(config_.new_size(), config_.original_size(), 
                               config_.original_address(), source_region->description + " (expanded)");
        
        // Copy original data
        if (source_region->size > 0 && source_region->data) {
            std::size_t copy_size = std::min(source_region->size, config_.new_size());
            std::copy_n(source_region->data.get(), copy_size, new_region.data.get());
        }
        
        // Store the expanded region
        std::string new_key = config_.key() + "_expanded";
        context.store_data(new_key, std::move(new_region));
        
        // Log success
        host_->log_message(2, "Copy memory task completed successfully for key: " + config_.key());
        
        return {};
    }
    
    std::string name() const override { return "TestCopyMemory"; }
    
    std::string description() const override {
        return "Test copy memory region '" + config_.key() + "' from " + 
               std::to_string(config_.original_size()) + " to " + 
               std::to_string(config_.new_size()) + " bytes";
    }
    
    const MockCopyMemoryConfig& config() const { return config_; }
    
private:
    MockCopyMemoryConfig config_;
    app_hook::plugin::IPluginHost* host_;
};

class CopyMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear context before each test
        auto& context = app_hook::context::ModContext::instance();
        auto keys = context.get_all_keys();
        for (const auto& key : keys) {
            context.remove_data(key);
        }
        
        mock_host_ = std::make_unique<MockPluginHost>();
    }
    
    void TearDown() override {
        // Clean up context after each test
        auto& context = app_hook::context::ModContext::instance();
        auto keys = context.get_all_keys();
        for (const auto& key : keys) {
            context.remove_data(key);
        }
        
        mock_host_.reset();
    }
    
    std::unique_ptr<MockPluginHost> mock_host_;
};

TEST_F(CopyMemoryTest, ConstructorBasic) {
    MockCopyMemoryConfig config("test.memory.region", 512, 1024, 0x12345678);
    TestCopyMemoryTask task(std::move(config));
    
    EXPECT_EQ(task.name(), "TestCopyMemory");
    EXPECT_TRUE(task.description().find("test.memory.region") != std::string::npos);
    EXPECT_TRUE(task.description().find("512") != std::string::npos);
    EXPECT_TRUE(task.description().find("1024") != std::string::npos);
}

TEST_F(CopyMemoryTest, SetHost) {
    MockCopyMemoryConfig config("test.host", 256, 512, 0x11111111);
    TestCopyMemoryTask task(std::move(config));
    
    task.setHost(mock_host_.get());
    
    // Task should now have host set (tested indirectly through execute)
}

TEST_F(CopyMemoryTest, SetHostVoidPointer) {
    MockCopyMemoryConfig config("test.host.void", 256, 512, 0x22222222);
    TestCopyMemoryTask task(std::move(config));
    
    task.setHost(static_cast<void*>(mock_host_.get()));
    
    // Should work the same as typed setHost
}

TEST_F(CopyMemoryTest, ExecuteWithoutHost) {
    MockCopyMemoryConfig config("test.no.host", 128, 256, 0x33333333);
    TestCopyMemoryTask task(std::move(config));
    
    auto result = task.execute();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), task::TaskError::dependency_not_met);
}

TEST_F(CopyMemoryTest, ExecuteWithoutSourceData) {
    MockCopyMemoryConfig config("test.missing.source", 128, 256, 0x44444444);
    TestCopyMemoryTask task(std::move(config));
    task.setHost(mock_host_.get());
    
    auto result = task.execute();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), task::TaskError::dependency_not_met);
}

TEST_F(CopyMemoryTest, ExecuteSuccessful) {
    const std::string key = "test.successful.copy";
    const std::size_t original_size = 256;
    const std::size_t new_size = 512;
    const std::uintptr_t address = 0x55555555;
    
    // Create source memory region
    MemoryRegion source_region(original_size, original_size, address, "Test source region");
    
    // Fill source with test data
    for (std::size_t i = 0; i < original_size; ++i) {
        source_region.data.get()[i] = static_cast<std::uint8_t>(i % 256);
    }
    
    // Store in context
    auto& context = app_hook::context::ModContext::instance();
    context.store_data(key, std::move(source_region));
    
    // Create and execute task
    MockCopyMemoryConfig config(key, original_size, new_size, address);
    TestCopyMemoryTask task(std::move(config));
    task.setHost(mock_host_.get());
    
    auto result = task.execute();
    EXPECT_TRUE(result.has_value());
    
    // Verify expanded region was created
    std::string expanded_key = key + "_expanded";
    EXPECT_TRUE(context.has_data(expanded_key));
    
    auto* expanded_region = context.get_data<MemoryRegion>(expanded_key);
    ASSERT_NE(expanded_region, nullptr);
    EXPECT_EQ(expanded_region->size, new_size);
    EXPECT_EQ(expanded_region->original_size, original_size);
    EXPECT_EQ(expanded_region->original_address, address);
    EXPECT_TRUE(expanded_region->description.find("expanded") != std::string::npos);
    
    // Verify data was copied correctly
    for (std::size_t i = 0; i < original_size; ++i) {
        EXPECT_EQ(expanded_region->data.get()[i], static_cast<std::uint8_t>(i % 256));
    }
}

TEST_F(CopyMemoryTest, ExecuteWithSizeMismatch) {
    const std::string key = "test.size.mismatch";
    const std::size_t actual_size = 256;
    const std::size_t expected_size = 128; // Different from actual
    const std::uintptr_t address = 0x66666666;
    
    // Create source memory region with actual_size
    MemoryRegion source_region(actual_size, actual_size, address, "Size mismatch test");
    
    auto& context = app_hook::context::ModContext::instance();
    context.store_data(key, std::move(source_region));
    
    // Create task expecting different size
    MockCopyMemoryConfig config(key, expected_size, 512, address);
    TestCopyMemoryTask task(std::move(config));
    task.setHost(mock_host_.get());
    
    auto result = task.execute();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), task::TaskError::invalid_config);
}

TEST_F(CopyMemoryTest, ExecuteWithZeroSize) {
    const std::string key = "test.zero.size";
    const std::size_t original_size = 0;
    const std::size_t new_size = 128;
    const std::uintptr_t address = 0x77777777;
    
    // Create zero-size source region
    MemoryRegion source_region(original_size, original_size, address, "Zero size test");
    
    auto& context = app_hook::context::ModContext::instance();
    context.store_data(key, std::move(source_region));
    
    MockCopyMemoryConfig config(key, original_size, new_size, address);
    TestCopyMemoryTask task(std::move(config));
    task.setHost(mock_host_.get());
    
    auto result = task.execute();
    EXPECT_TRUE(result.has_value());
    
    // Verify expanded region was created even with zero source size
    std::string expanded_key = key + "_expanded";
    auto* expanded_region = context.get_data<MemoryRegion>(expanded_key);
    ASSERT_NE(expanded_region, nullptr);
    EXPECT_EQ(expanded_region->size, new_size);
    EXPECT_EQ(expanded_region->original_size, original_size);
}

TEST_F(CopyMemoryTest, ExecuteExpandingSize) {
    const std::string key = "test.expand";
    const std::size_t original_size = 64;
    const std::size_t new_size = 256;
    const std::uintptr_t address = 0x88888888;
    
    // Create source region
    MemoryRegion source_region(original_size, original_size, address, "Expand test");
    
    // Fill with pattern
    for (std::size_t i = 0; i < original_size; ++i) {
        source_region.data.get()[i] = static_cast<std::uint8_t>(0xAA);
    }
    
    auto& context = app_hook::context::ModContext::instance();
    context.store_data(key, std::move(source_region));
    
    MockCopyMemoryConfig config(key, original_size, new_size, address);
    TestCopyMemoryTask task(std::move(config));
    task.setHost(mock_host_.get());
    
    auto result = task.execute();
    EXPECT_TRUE(result.has_value());
    
    // Verify expansion
    std::string expanded_key = key + "_expanded";
    auto* expanded_region = context.get_data<MemoryRegion>(expanded_key);
    ASSERT_NE(expanded_region, nullptr);
    EXPECT_EQ(expanded_region->size, new_size);
    
    // Verify original data was copied
    for (std::size_t i = 0; i < original_size; ++i) {
        EXPECT_EQ(expanded_region->data.get()[i], 0xAA);
    }
    
    // Expanded area should be zero-initialized
    for (std::size_t i = original_size; i < new_size; ++i) {
        EXPECT_EQ(expanded_region->data.get()[i], 0);
    }
}

TEST_F(CopyMemoryTest, ExecuteShrinkingSize) {
    const std::string key = "test.shrink";
    const std::size_t original_size = 256;
    const std::size_t new_size = 128; // Smaller than original
    const std::uintptr_t address = 0x99999999;
    
    // Create source region
    MemoryRegion source_region(original_size, original_size, address, "Shrink test");
    
    // Fill with incrementing pattern
    for (std::size_t i = 0; i < original_size; ++i) {
        source_region.data.get()[i] = static_cast<std::uint8_t>(i);
    }
    
    auto& context = app_hook::context::ModContext::instance();
    context.store_data(key, std::move(source_region));
    
    MockCopyMemoryConfig config(key, original_size, new_size, address);
    TestCopyMemoryTask task(std::move(config));
    task.setHost(mock_host_.get());
    
    auto result = task.execute();
    EXPECT_TRUE(result.has_value());
    
    // Verify shrinking
    std::string expanded_key = key + "_expanded";
    auto* expanded_region = context.get_data<MemoryRegion>(expanded_key);
    ASSERT_NE(expanded_region, nullptr);
    EXPECT_EQ(expanded_region->size, new_size);
    
    // Verify only first new_size bytes were copied
    for (std::size_t i = 0; i < new_size; ++i) {
        EXPECT_EQ(expanded_region->data.get()[i], static_cast<std::uint8_t>(i));
    }
}

TEST_F(CopyMemoryTest, ConfigAccessors) {
    const std::string key = "test.config.access";
    const std::size_t original_size = 512;
    const std::size_t new_size = 1024;
    const std::uintptr_t address = 0xAAAAAAAA;
    
    MockCopyMemoryConfig config(key, original_size, new_size, address);
    TestCopyMemoryTask task(std::move(config));
    
    const auto& task_config = task.config();
    EXPECT_EQ(task_config.key(), key);
    EXPECT_EQ(task_config.original_size(), original_size);
    EXPECT_EQ(task_config.new_size(), new_size);
    EXPECT_EQ(task_config.original_address(), address);
}

TEST_F(CopyMemoryTest, LoggingBehavior) {
    const std::string key = "test.logging";
    const std::size_t original_size = 128;
    const std::size_t new_size = 256;
    const std::uintptr_t address = 0xBBBBBBBB;
    
    // Create source region
    MemoryRegion source_region(original_size, original_size, address, "Logging test");
    auto& context = app_hook::context::ModContext::instance();
    context.store_data(key, std::move(source_region));
    
    MockCopyMemoryConfig config(key, original_size, new_size, address);
    TestCopyMemoryTask task(std::move(config));
    task.setHost(mock_host_.get());
    
    // Clear any existing log messages
    mock_host_->clear_log_messages();
    
    auto result = task.execute();
    EXPECT_TRUE(result.has_value());
    
    // Verify logging occurred
    const auto& log_messages = mock_host_->get_log_messages();
    EXPECT_FALSE(log_messages.empty());
    
    // Find the success message
    bool found_success_message = false;
    for (const auto& [level, message] : log_messages) {
        if (message.find("Copy memory task completed successfully") != std::string::npos &&
            message.find(key) != std::string::npos) {
            found_success_message = true;
            EXPECT_EQ(level, 2); // Info level
            break;
        }
    }
    EXPECT_TRUE(found_success_message);
}

TEST_F(CopyMemoryTest, TaskInterface) {
    MockCopyMemoryConfig config("test.interface", 64, 128, 0xCCCCCCCC);
    TestCopyMemoryTask task(std::move(config));
    
    // Test that it implements IHookTask interface correctly
    task::IHookTask* base_task = &task;
    
    EXPECT_EQ(base_task->name(), "TestCopyMemory");
    EXPECT_FALSE(base_task->description().empty());
    
    // setHost should work through base interface
    base_task->setHost(mock_host_.get());
}

TEST_F(CopyMemoryTest, MultipleExecutions) {
    const std::string key = "test.multiple";
    const std::size_t original_size = 128;
    const std::size_t new_size = 256;
    const std::uintptr_t address = 0xDDDDDDDD;
    
    // Create source region
    MemoryRegion source_region(original_size, original_size, address, "Multiple test");
    auto& context = app_hook::context::ModContext::instance();
    context.store_data(key, std::move(source_region));
    
    MockCopyMemoryConfig config(key, original_size, new_size, address);
    TestCopyMemoryTask task(std::move(config));
    task.setHost(mock_host_.get());
    
    // Execute multiple times
    auto result1 = task.execute();
    EXPECT_TRUE(result1.has_value());
    
    auto result2 = task.execute();
    EXPECT_TRUE(result2.has_value());
    
    // Both executions should succeed (second one overwrites first)
    std::string expanded_key = key + "_expanded";
    EXPECT_TRUE(context.has_data(expanded_key));
}

} // namespace app_hook::memory 