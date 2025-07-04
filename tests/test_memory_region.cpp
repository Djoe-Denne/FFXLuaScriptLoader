#include <gtest/gtest.h>
#include "memory/memory_region.hpp"
#include <array>
#include <algorithm>
#include <numeric>

namespace app_hook::memory {

class MemoryRegionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common test setup
    }
    
    void TearDown() override {
        // Common test cleanup
    }
};

TEST_F(MemoryRegionTest, DefaultConstructor) {
    MemoryRegion region;
    
    EXPECT_EQ(region.data.get(), nullptr);
    EXPECT_EQ(region.size, 0);
    EXPECT_EQ(region.original_size, 0);
    EXPECT_EQ(region.original_address, 0);
    EXPECT_TRUE(region.description.empty());
}

TEST_F(MemoryRegionTest, ParameterizedConstructor) {
    const std::size_t size = 1024;
    const std::size_t original_size = 512;
    const std::uintptr_t address = 0x12345678;
    const std::string description = "Test memory region";
    
    MemoryRegion region(size, original_size, address, description);
    
    EXPECT_NE(region.data.get(), nullptr);
    EXPECT_EQ(region.size, size);
    EXPECT_EQ(region.original_size, original_size);
    EXPECT_EQ(region.original_address, address);
    EXPECT_EQ(region.description, description);
}

TEST_F(MemoryRegionTest, ParameterizedConstructorWithStringMove) {
    const std::size_t size = 256;
    const std::size_t original_size = 128;
    const std::uintptr_t address = 0xABCDEF00;
    std::string description = "Movable description";
    std::string description_copy = description;
    
    MemoryRegion region(size, original_size, address, std::move(description));
    
    EXPECT_NE(region.data.get(), nullptr);
    EXPECT_EQ(region.size, size);
    EXPECT_EQ(region.original_size, original_size);
    EXPECT_EQ(region.original_address, address);
    EXPECT_EQ(region.description, description_copy);
    EXPECT_TRUE(description.empty()); // Original should be moved from
}

TEST_F(MemoryRegionTest, NonCopyable) {
    // Verify that MemoryRegion is not copyable
    EXPECT_FALSE(std::is_copy_constructible_v<MemoryRegion>);
    EXPECT_FALSE(std::is_copy_assignable_v<MemoryRegion>);
}

TEST_F(MemoryRegionTest, Movable) {
    // Verify that MemoryRegion is movable
    EXPECT_TRUE(std::is_move_constructible_v<MemoryRegion>);
    EXPECT_TRUE(std::is_move_assignable_v<MemoryRegion>);
}

TEST_F(MemoryRegionTest, MoveConstructor) {
    const std::size_t size = 512;
    const std::size_t original_size = 256;
    const std::uintptr_t address = 0x11223344;
    const std::string description = "Move test";
    
    MemoryRegion original(size, original_size, address, description);
    std::uint8_t* original_data_ptr = original.data.get();
    
    MemoryRegion moved(std::move(original));
    
    // Check that data was moved
    EXPECT_EQ(moved.data.get(), original_data_ptr);
    EXPECT_EQ(moved.size, size);
    EXPECT_EQ(moved.original_size, original_size);
    EXPECT_EQ(moved.original_address, address);
    EXPECT_EQ(moved.description, description);
    
    // Check that original was moved from
    EXPECT_EQ(original.data.get(), nullptr);
}

TEST_F(MemoryRegionTest, MoveAssignment) {
    const std::size_t size1 = 256;
    const std::size_t size2 = 512;
    const std::uintptr_t address1 = 0x11111111;
    const std::uintptr_t address2 = 0x22222222;
    
    MemoryRegion region1(size1, size1, address1, "Region 1");
    MemoryRegion region2(size2, size2, address2, "Region 2");
    
    std::uint8_t* region2_data_ptr = region2.data.get();
    
    region1 = std::move(region2);
    
    // Check that region1 now has region2's data
    EXPECT_EQ(region1.data.get(), region2_data_ptr);
    EXPECT_EQ(region1.size, size2);
    EXPECT_EQ(region1.original_address, address2);
    EXPECT_EQ(region1.description, "Region 2");
    
    // Check that region2 was moved from
    EXPECT_EQ(region2.data.get(), nullptr);
}

TEST_F(MemoryRegionTest, SpanAccess) {
    const std::size_t size = 128;
    MemoryRegion region(size, size, 0x12345678, "Span test");
    
    // Test non-const span
    auto span = region.span();
    EXPECT_EQ(span.data(), region.data.get());
    EXPECT_EQ(span.size(), size);
    
    // Fill with test data
    std::iota(span.begin(), span.end(), static_cast<std::uint8_t>(0));
    
    // Verify data was written
    for (std::size_t i = 0; i < size; ++i) {
        EXPECT_EQ(region.data.get()[i], static_cast<std::uint8_t>(i));
    }
}

TEST_F(MemoryRegionTest, ConstSpanAccess) {
    const std::size_t size = 64;
    MemoryRegion region(size, size, 0x87654321, "Const span test");
    
    // Fill with test data
    for (std::size_t i = 0; i < size; ++i) {
        region.data.get()[i] = static_cast<std::uint8_t>(i + 1);
    }
    
    // Test const span
    const auto& const_region = region;
    auto const_span = const_region.span();
    EXPECT_EQ(const_span.data(), region.data.get());
    EXPECT_EQ(const_span.size(), size);
    
    // Verify data can be read
    for (std::size_t i = 0; i < size; ++i) {
        EXPECT_EQ(const_span[i], static_cast<std::uint8_t>(i + 1));
    }
}

TEST_F(MemoryRegionTest, ToStringBasic) {
    const std::uintptr_t address = 0x12345678;
    const std::size_t size = 256;
    const std::string description = "Test region";
    
    MemoryRegion region(size, size, address, description);
    std::string result = region.to_string();
    
    EXPECT_TRUE(result.find(description) != std::string::npos);
    EXPECT_TRUE(result.find("12345678") != std::string::npos);
}

TEST_F(MemoryRegionTest, ToStringWithOffsetAndCount) {
    const std::size_t size = 32;
    MemoryRegion region(size, size, 0x11111111, "Hex test");
    
    // Fill with known pattern
    for (std::size_t i = 0; i < size; ++i) {
        region.data.get()[i] = static_cast<std::uint8_t>(i);
    }
    
    // Test with valid offset and count
    std::string result = region.to_string(0, 4);
    EXPECT_TRUE(result.find("0x00") != std::string::npos);
    EXPECT_TRUE(result.find("0x01") != std::string::npos);
    EXPECT_TRUE(result.find("0x02") != std::string::npos);
    EXPECT_TRUE(result.find("0x03") != std::string::npos);
    
    // Test with offset
    result = region.to_string(2, 2);
    EXPECT_TRUE(result.find("0x02") != std::string::npos);
    EXPECT_TRUE(result.find("0x03") != std::string::npos);
    EXPECT_FALSE(result.find("0x00") != std::string::npos);
}

TEST_F(MemoryRegionTest, ToStringInvalidOffset) {
    const std::size_t size = 16;
    MemoryRegion region(size, size, 0x22222222, "Invalid offset test");
    
    // Test with invalid offset
    std::string result = region.to_string(size + 1, 4);
    EXPECT_EQ(result, "Invalid offset or null data");
}

TEST_F(MemoryRegionTest, ToStringNullData) {
    MemoryRegion region; // Default constructor creates null data
    
    std::string result = region.to_string(0, 4);
    EXPECT_EQ(result, "Invalid offset or null data");
}

TEST_F(MemoryRegionTest, ToStringCountClamping) {
    const std::size_t size = 8;
    MemoryRegion region(size, size, 0x33333333, "Clamping test");
    
    // Fill with test data
    for (std::size_t i = 0; i < size; ++i) {
        region.data.get()[i] = static_cast<std::uint8_t>(0x10 + i);
    }
    
    // Request more bytes than available
    std::string result = region.to_string(4, 10); // Only 4 bytes available from offset 4
    
    // Should only contain the available 4 bytes
    EXPECT_TRUE(result.find("0x14") != std::string::npos); // byte at offset 4
    EXPECT_TRUE(result.find("0x17") != std::string::npos); // byte at offset 7
    
    // Count occurrences of "0x" to verify we only got 4 bytes
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = result.find("0x", pos)) != std::string::npos) {
        ++count;
        pos += 2;
    }
    EXPECT_EQ(count, 4);
}

TEST_F(MemoryRegionTest, ToStringLargeDataWithNewlines) {
    const std::size_t size = 32;
    MemoryRegion region(size, size, 0x44444444, "Newline test");
    
    // Fill with incrementing pattern
    for (std::size_t i = 0; i < size; ++i) {
        region.data.get()[i] = static_cast<std::uint8_t>(i);
    }
    
    std::string result = region.to_string(0, size);
    
    // Should contain newlines every 16 bytes
    std::size_t newline_count = std::count(result.begin(), result.end(), '\n');
    EXPECT_EQ(newline_count, 1); // Should have 1 newline for 32 bytes (after first 16)
}

TEST_F(MemoryRegionTest, ZeroSizeRegion) {
    MemoryRegion region(0, 0, 0x55555555, "Zero size");
    
    EXPECT_NE(region.data.get(), nullptr); // unique_ptr should still be allocated
    EXPECT_EQ(region.size, 0);
    
    auto span = region.span();
    EXPECT_EQ(span.size(), 0);
    
    std::string result = region.to_string(0, 1);
    EXPECT_EQ(result, "Invalid offset or null data");
}

} // namespace app_hook::memory 