#pragma once

#include <config/config_base.hpp>
#include <string>
#include <cstdint>
#include <optional>

namespace app_hook::config {

/// @brief Configuration for memory copy operations
class CopyMemoryConfig : public ConfigBase, public AddressTrigger {
public:
    /// @brief Constructor
    /// @param key Configuration key
    /// @param name Display name
    CopyMemoryConfig(std::string key, std::string name)
        : ConfigBase(ConfigType::Memory, std::move(key), std::move(name))
        , address_(0)
        , copy_after_(0)
        , original_size_(0)
        , new_size_(0) {}

    /// @brief Copy constructor
    /// @param other Another CopyMemoryConfig to copy from
    CopyMemoryConfig(const CopyMemoryConfig& other)
        : ConfigBase(ConfigType::Memory, other.key(), other.name())
        , address_(other.address_)
        , copy_after_(other.copy_after_)
        , original_size_(other.original_size_)
        , new_size_(other.new_size_) {
        set_description(other.description());
        set_enabled(other.enabled());
        set_write_in_context(other.write_in_context());
        set_read_from_context(other.read_from_context());
    }

    /// @brief Default destructor
    ~CopyMemoryConfig() override = default;

    // Accessors following C++23 conventions
    [[nodiscard]] constexpr std::uintptr_t address() const noexcept { return address_; }
    [[nodiscard]] constexpr std::uintptr_t copy_after() const noexcept { return copy_after_; }
    [[nodiscard]] constexpr std::size_t original_size() const noexcept { return original_size_; }
    [[nodiscard]] constexpr std::size_t new_size() const noexcept { return new_size_; }

    // Mutators
    void set_address(std::uintptr_t addr) noexcept { address_ = addr; }
    void set_copy_after(std::uintptr_t addr) noexcept { copy_after_ = addr; }
    void set_original_size(std::size_t size) noexcept { original_size_ = size; }
    void set_new_size(std::size_t size) noexcept { new_size_ = size; }

    // AddressTrigger interface implementation
    /// @brief Get the hook address for this memory configuration
    /// @return The copy_after address where the hook should be installed
    [[nodiscard]] std::uintptr_t get_hook_address() const noexcept override {
        return copy_after_;
    }

    /// @brief Check if this configuration is valid
    /// @return True if all required fields are properly set
    [[nodiscard]] bool is_valid() const noexcept override {
        return ConfigBase::is_valid() && 
               address_ != 0 && copy_after_ != 0 && 
               original_size_ > 0 && new_size_ >= original_size_;
    }

    /// @brief Get debug string representation
    /// @return Debug string with memory-specific information
    [[nodiscard]] std::string debug_string() const override {
        return ConfigBase::debug_string() + 
               " addr=0x" + std::to_string(address_) +
               " copy_after=0x" + std::to_string(copy_after_) +
               " size=" + std::to_string(original_size_) + "->" + std::to_string(new_size_);
    }

private:
    std::uintptr_t address_;                   ///< Source address to copy from
    std::uintptr_t copy_after_;                ///< Address after which to place new memory
    std::size_t original_size_;                ///< Original size of the memory region
    std::size_t new_size_;                     ///< New size for the expanded memory region
};

} // namespace app_hook::config 