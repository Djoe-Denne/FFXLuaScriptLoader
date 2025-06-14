/**
 * @file test_mod_context.cpp
 * @brief Unit tests for ModContext and MemoryRegion classes
 * 
 * Tests the context management system including:
 * - Memory region creation and management
 * - Thread-safe operations
 * - Singleton behavior
 * - Memory region utilities
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "context/mod_context.hpp"
#include <thread>
#include <vector>
#include <chrono>
#include <memory>
#include <string>
#include <atomic>
#include <cstdio>

using namespace app_hook::context;
using namespace ::testing;

namespace {

/// @brief Helper function to create test memory region
[[nodiscard]] MemoryRegion create_test_region(std::size_t size, std::uintptr_t addr, const std::string& desc) {
    MemoryRegion region(size, addr, desc);
    
    // Fill with test pattern
    for (std::size_t i = 0; i < size; ++i) {
        region.data.get()[i] = static_cast<std::uint8_t>(i % 256);
    }
    
    return region;
}

} // anonymous namespace

/// @brief Test fixture for MemoryRegion tests
class MemoryRegionTest : public Test {
protected:
    void SetUp() override {
        region_ = std::make_unique<MemoryRegion>(1024, 0x12345678, "Test Memory Region");
        
        // Fill with test data
        for (std::size_t i = 0; i < 1024; ++i) {
            region_->data.get()[i] = static_cast<std::uint8_t>((i * 7) % 256);
        }
    }

    std::unique_ptr<MemoryRegion> region_;
};

/// @brief Test fixture for ModContext tests
class ModContextTest : public Test {
protected:
    void SetUp() override {
        // Get fresh instance for each test
        context_ = &ModContext::instance();
        
        // Clear any existing regions (note: this is not ideal in production,
        // but necessary for isolated testing)
    }
    
    void TearDown() override {
        // Clean up any test regions
        // Note: In a real scenario, we'd need a way to clear the context
        // For now, we'll use unique keys to avoid conflicts
    }

    ModContext* context_;
};

// =============================================================================
// MemoryRegion Basic Tests
// =============================================================================

TEST_F(MemoryRegionTest, BasicConstruction) {
    EXPECT_EQ(region_->size, 1024);
    EXPECT_EQ(region_->original_address, 0x12345678);
    EXPECT_EQ(region_->description, "Test Memory Region");
    EXPECT_NE(region_->data.get(), nullptr);
}

TEST_F(MemoryRegionTest, DefaultConstruction) {
    MemoryRegion default_region;
    
    EXPECT_EQ(default_region.size, 0);
    EXPECT_EQ(default_region.original_address, 0);
    EXPECT_TRUE(default_region.description.empty());
    EXPECT_EQ(default_region.data.get(), nullptr);
}

TEST_F(MemoryRegionTest, MoveSemantics) {
    const auto original_size = region_->size;
    const auto original_address = region_->original_address;
    const auto original_description = region_->description;
    const auto* original_data_ptr = region_->data.get();
    
    // Move construct
    MemoryRegion moved_region = std::move(*region_);
    
    EXPECT_EQ(moved_region.size, original_size);
    EXPECT_EQ(moved_region.original_address, original_address);
    EXPECT_EQ(moved_region.description, original_description);
    EXPECT_EQ(moved_region.data.get(), original_data_ptr);
    
    // Original should be in moved-from state
    EXPECT_EQ(region_->data.get(), nullptr);
}

TEST_F(MemoryRegionTest, MoveAssignment) {
    MemoryRegion target_region(512, 0x87654321, "Target Region");
    
    const auto source_size = region_->size;
    const auto source_address = region_->original_address;
    const auto* source_data_ptr = region_->data.get();
    
    target_region = std::move(*region_);
    
    EXPECT_EQ(target_region.size, source_size);
    EXPECT_EQ(target_region.original_address, source_address);
    EXPECT_EQ(target_region.data.get(), source_data_ptr);
    
    EXPECT_EQ(region_->data.get(), nullptr);
}

// =============================================================================
// MemoryRegion Span Tests
// =============================================================================

TEST_F(MemoryRegionTest, SpanAccess) {
    auto span = region_->span();
    
    EXPECT_EQ(span.size(), 1024);
    EXPECT_EQ(span.data(), region_->data.get());
    
    // Test mutable access
    span[0] = 0xFF;
    EXPECT_EQ(region_->data.get()[0], 0xFF);
}

TEST_F(MemoryRegionTest, ConstSpanAccess) {
    const auto& const_region = *region_;
    auto const_span = const_region.span();
    
    EXPECT_EQ(const_span.size(), 1024);
    EXPECT_EQ(const_span.data(), region_->data.get());
    
    // Verify data integrity
    for (std::size_t i = 0; i < 10; ++i) {
        EXPECT_EQ(const_span[i], static_cast<std::uint8_t>((i * 7) % 256));
    }
}

// =============================================================================
// MemoryRegion String Representation Tests
// =============================================================================

TEST_F(MemoryRegionTest, ToStringBasic) {
    const std::string str = region_->to_string();
    
    EXPECT_THAT(str, HasSubstr("Test Memory Region"));
    EXPECT_THAT(str, HasSubstr("0x12345678"));
    EXPECT_THAT(str, HasSubstr("MemoryRegion"));
}

TEST_F(MemoryRegionTest, ToStringWithOffsetAndCount) {
    const std::string content_str = region_->to_string(0, 10);
    
    // Should contain hex representation of first 10 bytes
    EXPECT_THAT(content_str, HasSubstr("0x"));
    
    // Check that it contains expected values using more compatible string formatting
    for (std::size_t i = 0; i < 10; ++i) {
        const auto expected_value = static_cast<std::uint8_t>((i * 7) % 256);
        // Use format instead of std::format for better compatibility
        char hex_buffer[8];
        std::snprintf(hex_buffer, sizeof(hex_buffer), "0x%02X", expected_value);
        EXPECT_THAT(content_str, HasSubstr(std::string(hex_buffer)));
    }
}

TEST_F(MemoryRegionTest, ToStringEdgeCases) {
    // Test with invalid offset
    const std::string invalid_str = region_->to_string(2000, 10);
    EXPECT_THAT(invalid_str, HasSubstr("Invalid offset"));
    
    // Test with count larger than available data
    const std::string clamped_str = region_->to_string(1020, 10);
    // Should only show 4 bytes (1024 - 1020 = 4)
    EXPECT_THAT(clamped_str, HasSubstr("0x"));
    
    // Test empty region
    MemoryRegion empty_region;
    const std::string empty_str = empty_region.to_string(0, 10);
    EXPECT_THAT(empty_str, HasSubstr("Invalid offset"));
}

// =============================================================================
// ModContext Singleton Tests
// =============================================================================

TEST(ModContextSingletonTest, SingletonBehavior) {
    auto& instance1 = ModContext::instance();
    auto& instance2 = ModContext::instance();
    
    EXPECT_EQ(&instance1, &instance2);
}

TEST(ModContextSingletonTest, ThreadSafeSingleton) {
    constexpr int num_threads = 10;
    std::vector<ModContext*> instances(num_threads);
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&instances, i]() {
            instances[i] = &ModContext::instance();
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // All instances should be the same
    for (int i = 1; i < num_threads; ++i) {
        EXPECT_EQ(instances[0], instances[i]);
    }
}

// =============================================================================
// ModContext Memory Region Management Tests
// =============================================================================

TEST_F(ModContextTest, StoreAndRetrieveMemoryRegion) {
    auto region = create_test_region(256, 0x10000000, "Test Store Region");
    const auto* original_data = region.data.get();
    
    context_->store_memory_region("test_key_1", std::move(region));
    
    const auto* retrieved = context_->get_memory_region("test_key_1");
    ASSERT_NE(retrieved, nullptr);
    
    EXPECT_EQ(retrieved->size, 256);
    EXPECT_EQ(retrieved->original_address, 0x10000000);
    EXPECT_EQ(retrieved->description, "Test Store Region");
    EXPECT_EQ(retrieved->data.get(), original_data);
}

TEST_F(ModContextTest, RetrieveNonExistentRegion) {
    const auto* retrieved = context_->get_memory_region("non_existent_key");
    EXPECT_EQ(retrieved, nullptr);
}

TEST_F(ModContextTest, HasMemoryRegion) {
    auto region = create_test_region(128, 0x20000000, "Test Has Region");
    
    EXPECT_FALSE(context_->has_memory_region("test_key_2"));
    
    context_->store_memory_region("test_key_2", std::move(region));
    
    EXPECT_TRUE(context_->has_memory_region("test_key_2"));
    EXPECT_FALSE(context_->has_memory_region("test_key_3"));
}

TEST_F(ModContextTest, OverwriteExistingRegion) {
    // Store first region
    auto region1 = create_test_region(256, 0x30000000, "First Region");
    context_->store_memory_region("overwrite_key", std::move(region1));
    
    const auto* first_retrieval = context_->get_memory_region("overwrite_key");
    ASSERT_NE(first_retrieval, nullptr);
    EXPECT_EQ(first_retrieval->description, "First Region");
    
    // Overwrite with second region
    auto region2 = create_test_region(512, 0x40000000, "Second Region");
    context_->store_memory_region("overwrite_key", std::move(region2));
    
    const auto* second_retrieval = context_->get_memory_region("overwrite_key");
    ASSERT_NE(second_retrieval, nullptr);
    EXPECT_EQ(second_retrieval->description, "Second Region");
    EXPECT_EQ(second_retrieval->size, 512);
    EXPECT_EQ(second_retrieval->original_address, 0x40000000);
}

// =============================================================================
// ModContext Thread Safety Tests
// =============================================================================

TEST_F(ModContextTest, ConcurrentStoreAndRetrieve) {
    constexpr int num_threads = 8;
    constexpr int regions_per_thread = 10;
    
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    
    // Launch threads that store memory regions
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, regions_per_thread]() {
            for (int r = 0; r < regions_per_thread; ++r) {
                const std::string key = "thread_" + std::to_string(t) + "_region_" + std::to_string(r);
                const std::string desc = "Thread " + std::to_string(t) + " Region " + std::to_string(r);
                
                auto region = create_test_region(64 + r, 0x50000000 + t * 1000 + r, desc);
                context_->store_memory_region(key, std::move(region));
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all regions were stored correctly
    for (int t = 0; t < num_threads; ++t) {
        for (int r = 0; r < regions_per_thread; ++r) {
            const std::string key = "thread_" + std::to_string(t) + "_region_" + std::to_string(r);
            const std::string expected_desc = "Thread " + std::to_string(t) + " Region " + std::to_string(r);
            
            EXPECT_TRUE(context_->has_memory_region(key));
            
            const auto* region = context_->get_memory_region(key);
            ASSERT_NE(region, nullptr);
            EXPECT_EQ(region->description, expected_desc);
            EXPECT_EQ(region->size, 64 + r);
        }
    }
}

TEST_F(ModContextTest, ConcurrentReadAccess) {
    // Store some regions first
    constexpr int num_regions = 5;
    for (int i = 0; i < num_regions; ++i) {
        const std::string key = "read_test_" + std::to_string(i);
        const std::string desc = "Read Test Region " + std::to_string(i);
        auto region = create_test_region(128 + i * 64, 0x60000000 + i * 0x1000, desc);
        context_->store_memory_region(key, std::move(region));
    }
    
    // Launch multiple reader threads
    constexpr int num_readers = 10;
    constexpr int reads_per_thread = 100;
    
    std::vector<std::thread> readers;
    readers.reserve(num_readers);
    
    std::atomic<int> successful_reads{0};
    
    for (int t = 0; t < num_readers; ++t) {
        readers.emplace_back([this, &successful_reads, reads_per_thread, num_regions]() {
            for (int r = 0; r < reads_per_thread; ++r) {
                const int region_index = r % num_regions;
                const std::string key = "read_test_" + std::to_string(region_index);
                
                if (const auto* region = context_->get_memory_region(key); region != nullptr) {
                    successful_reads.fetch_add(1, std::memory_order_relaxed);
                    
                    // Verify data integrity
                    const auto expected_size = 128 + region_index * 64;
                    if (region->size == expected_size) {
                        // Read some data to ensure memory is accessible
                        volatile auto first_byte = region->data.get()[0];
                        (void)first_byte;  // Suppress unused variable warning
                    }
                }
            }
        });
    }
    
    for (auto& reader : readers) {
        reader.join();
    }
    
    // All reads should have been successful
    EXPECT_EQ(successful_reads.load(), num_readers * reads_per_thread);
}

// =============================================================================
// ModContext Const Access Tests
// =============================================================================

TEST_F(ModContextTest, ConstAccess) {
    auto region = create_test_region(256, 0x70000000, "Const Test Region");
    context_->store_memory_region("const_test_key", std::move(region));
    
    const auto* const_context = context_;
    const auto* retrieved = const_context->get_memory_region("const_test_key");
    
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->size, 256);
    EXPECT_EQ(retrieved->description, "Const Test Region");
    
    // Test const span access
    auto const_span = retrieved->span();
    EXPECT_EQ(const_span.size(), 256);
}

// =============================================================================
// Memory Management and Large Data Tests
// =============================================================================

TEST_F(ModContextTest, LargeMemoryRegions) {
    constexpr std::size_t large_size = 1024 * 1024;  // 1MB
    
    auto large_region = create_test_region(large_size, 0x80000000, "Large Test Region");
    context_->store_memory_region("large_region_key", std::move(large_region));
    
    const auto* retrieved = context_->get_memory_region("large_region_key");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->size, large_size);
    
    // Verify data integrity at various points
    const auto span = retrieved->span();
    EXPECT_EQ(span[0], 0);
    EXPECT_EQ(span[255], 255);
    EXPECT_EQ(span[512], 0);  // 512 % 256 = 0
}

TEST_F(ModContextTest, ManySmallRegions) {
    constexpr int num_regions = 1000;
    
    for (int i = 0; i < num_regions; ++i) {
        const std::string key = "small_region_" + std::to_string(i);
        const std::string desc = "Small Region " + std::to_string(i);
        auto region = create_test_region(16, 0x90000000 + i * 16, desc);
        context_->store_memory_region(key, std::move(region));
    }
    
    // Verify all regions exist
    for (int i = 0; i < num_regions; ++i) {
        const std::string key = "small_region_" + std::to_string(i);
        EXPECT_TRUE(context_->has_memory_region(key));
        
        const auto* region = context_->get_memory_region(key);
        ASSERT_NE(region, nullptr);
        EXPECT_EQ(region->size, 16);
    }
} 
