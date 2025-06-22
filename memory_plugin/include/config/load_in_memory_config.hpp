#pragma once

#include <config/config_base.hpp>
#include <string>
#include <cstdint>

namespace app_hook::config {

/// @brief Configuration for loading binary data into memory
class LoadInMemoryConfig : public ConfigBase {
public:
    /// @brief Constructor
    /// @param key Configuration key
    /// @param name Display name
    LoadInMemoryConfig(std::string key, std::string name)
        : ConfigBase(ConfigType::Load, std::move(key), std::move(name))
        , binary_path_{}
        , offset_security_(0) {}

    /// @brief Copy constructor
    /// @param other Another LoadInMemoryConfig to copy from
    LoadInMemoryConfig(const LoadInMemoryConfig& other)
        : ConfigBase(ConfigType::Load, other.key(), other.name())
        , binary_path_(other.binary_path_)
        , offset_security_(other.offset_security_) {
        set_description(other.description());
        set_enabled(other.enabled());
        set_write_in_context(other.write_in_context());
        set_read_from_context(other.read_from_context());
    }

    /// @brief Default destructor
    ~LoadInMemoryConfig() override = default;

    // Accessors following C++23 conventions
    [[nodiscard]] const std::string& binary_path() const noexcept { return binary_path_; }
    [[nodiscard]] constexpr std::uintptr_t offset_security() const noexcept { return offset_security_; }

    // Mutators
    void set_binary_path(std::string path) { binary_path_ = std::move(path); }
    void set_offset_security(std::uintptr_t offset) noexcept { offset_security_ = offset; }

    /// @brief Check if this configuration is valid
    /// @return True if all required fields are properly set
    [[nodiscard]] bool is_valid() const noexcept override {
        return ConfigBase::is_valid() && !binary_path_.empty();
    }

    /// @brief Get debug string representation
    /// @return Debug string with load-specific information
    [[nodiscard]] std::string debug_string() const override {
        return ConfigBase::debug_string() + 
               " binary=" + binary_path_ +
               " offsetSecurity=0x" + std::to_string(offset_security_);
    }

private:
    std::string binary_path_;              ///< Path to the binary file to load
    std::uintptr_t offset_security_;       ///< Security offset to apply when loading
};

} // namespace app_hook::config 