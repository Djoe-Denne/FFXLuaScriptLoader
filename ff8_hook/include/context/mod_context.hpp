#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <span>
#include <cstdint>
#include <sstream>
#include <iomanip>

namespace ff8_hook::context {

/// @brief Memory region information stored in the context
struct MemoryRegion {
    std::unique_ptr<std::uint8_t[]> data;
    std::size_t size;
    std::uintptr_t original_address;
    std::string description;
    
    MemoryRegion() = default;
    MemoryRegion(std::size_t sz, std::uintptr_t addr, std::string desc)
        : data(std::make_unique<std::uint8_t[]>(sz))
        , size(sz)
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
        ss << "MemoryRegion: " << description << " [0x" << std::hex << original_address << " - 0x" << std::hex << original_address + size << "]";
        return ss.str();
    }

    /// @brief Get the memory content as a string
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
            ss << "0x" << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(data.get()[offset + i]);
        }
        return ss.str();
    }
};

/// @brief Thread-safe context for mod operations
class ModContext {
public:
    ModContext() = default;
    ~ModContext() = default;
    
    // Non-copyable, non-movable singleton-like
    ModContext(const ModContext&) = delete;
    ModContext& operator=(const ModContext&) = delete;
    ModContext(ModContext&&) = delete;
    ModContext& operator=(ModContext&&) = delete;
    
    /// @brief Store a memory region with the given key
    /// @param key Memory region key (e.g., "memory.K_MAGIC")
    /// @param region Memory region to store
    void store_memory_region(const std::string& key, MemoryRegion region) {
        std::lock_guard lock{mutex_};
        memory_regions_[key] = std::move(region);
    }
    
    /// @brief Get a memory region by key
    /// @param key Memory region key
    /// @return Pointer to memory region or nullptr if not found
    [[nodiscard]] MemoryRegion* get_memory_region(const std::string& key) {
        std::lock_guard lock{mutex_};
        if (auto it = memory_regions_.find(key); it != memory_regions_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    /// @brief Get a const memory region by key
    /// @param key Memory region key
    /// @return Const pointer to memory region or nullptr if not found
    [[nodiscard]] const MemoryRegion* get_memory_region(const std::string& key) const {
        std::lock_guard lock{mutex_};
        if (auto it = memory_regions_.find(key); it != memory_regions_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    /// @brief Check if a memory region exists
    /// @param key Memory region key
    /// @return true if region exists
    [[nodiscard]] bool has_memory_region(const std::string& key) const {
        std::lock_guard lock{mutex_};
        return memory_regions_.contains(key);
    }
    
    /// @brief Get the global instance
    [[nodiscard]] static ModContext& instance() {
        static ModContext instance;
        return instance;
    }
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, MemoryRegion> memory_regions_;
};

} // namespace ff8_hook::context
