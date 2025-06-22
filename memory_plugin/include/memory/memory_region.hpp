#pragma once

#include <memory>
#include <string>
#include <span>
#include <cstdint>
#include <sstream>
#include <iomanip>

namespace app_hook::memory {

/// @brief Memory region information for memory operations
/// @note This class manages allocated memory regions with metadata
struct MemoryRegion {
    std::unique_ptr<std::uint8_t[]> data;
    std::size_t size;
    std::size_t original_size;
    std::uintptr_t original_address;
    std::string description;
    
    /// @brief Default constructor
    MemoryRegion() : data(nullptr), size(0), original_size(0), original_address(0), description() {}
    
    /// @brief Constructor with parameters
    /// @param sz Size of the memory region
    /// @param addr Original address
    /// @param desc Description of the memory region
    MemoryRegion(std::size_t sz, std::size_t original_sz, std::uintptr_t addr, std::string desc)
        : data(std::make_unique<std::uint8_t[]>(sz))
        , size(sz)
        , original_size(original_sz)
        , original_address(addr)
        , description(std::move(desc)) {}
        
    // Non-copyable but movable
    MemoryRegion(const MemoryRegion&) = delete;
    MemoryRegion& operator=(const MemoryRegion&) = delete;
    MemoryRegion(MemoryRegion&&) = default;
    MemoryRegion& operator=(MemoryRegion&&) = default;
    
    /// @brief Get a span view of the memory region
    [[nodiscard]] std::span<std::uint8_t> span() noexcept {
        return {data.get(), size};
    }
    
    /// @brief Get a const span view of the memory region
    [[nodiscard]] std::span<const std::uint8_t> span() const noexcept {
        return {data.get(), size};
    }

    /// @brief Get the memory region as a string
    [[nodiscard]] std::string to_string() const noexcept {
        std::stringstream ss;
        ss << "MemoryRegion: " << description << " [0x" << std::hex << original_address 
           << " - 0x" << std::hex << original_address + size << "]";
        return ss.str();
    }

    /// @brief Get the memory content as a string
    /// @param offset Starting offset in the memory region
    /// @param count Number of bytes to display
    /// @return Formatted hex string of the memory content
    [[nodiscard]] std::string to_string(std::size_t offset, std::size_t count) const noexcept {
        // Safety checks
        if (!data || offset >= size) {
            return "Invalid offset or null data";
        }
        
        // Clamp count to available data
        const std::size_t max_count = size - offset;
        const std::size_t actual_count = (count > max_count) ? max_count : count;
        
        std::stringstream ss;
        ss << std::hex << std::uppercase;
        
        for (std::size_t i = 0; i < actual_count; ++i) {
            if (i > 0 && i % 16 == 0) {
                ss << std::endl;
            } else if (i > 0) {
                ss << " ";
            }
            ss << "0x" << std::setfill('0') << std::setw(2) 
               << static_cast<unsigned int>(data.get()[offset + i]);
        }
        return ss.str();
    }
};

} // namespace app_hook::memory 