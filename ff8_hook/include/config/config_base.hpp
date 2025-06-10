#pragma once

#include <string>
#include <memory>
#include <string_view>

namespace ff8_hook::config {

/// @brief Configuration type enumeration
enum class ConfigType : std::uint8_t {
    Unknown = 0,
    Memory,     ///< Memory expansion configuration
    Patch,      ///< Instruction patch configuration
    Script,     ///< Script configuration (future)
    Audio,      ///< Audio configuration (future)
    Graphics    ///< Graphics configuration (future)
};

/// @brief Convert ConfigType to string representation
/// @param type Configuration type
/// @return String representation of the type
[[nodiscard]] constexpr std::string_view to_string(ConfigType type) noexcept {
    switch (type) {
        case ConfigType::Memory:   return "memory";
        case ConfigType::Patch:    return "patch";
        case ConfigType::Script:   return "script";
        case ConfigType::Audio:    return "audio";
        case ConfigType::Graphics: return "graphics";
        case ConfigType::Unknown:  
        default:                   return "unknown";
    }
}

/// @brief Convert string to ConfigType
/// @param type_str String representation of the type
/// @return Corresponding ConfigType
[[nodiscard]] constexpr ConfigType from_string(std::string_view type_str) noexcept {
    if (type_str == "memory")   return ConfigType::Memory;
    if (type_str == "patch")    return ConfigType::Patch;
    if (type_str == "script")   return ConfigType::Script;
    if (type_str == "audio")    return ConfigType::Audio;
    if (type_str == "graphics") return ConfigType::Graphics;
    return ConfigType::Unknown;
}

/// @brief Base class for all configuration types
/// @note Provides common interface and polymorphic behavior
class ConfigBase {
public:
    /// @brief Constructor with required fields
    /// @param type Configuration type
    /// @param key Unique identifier for this configuration
    /// @param name Display name of the configuration
    ConfigBase(ConfigType type, std::string key, std::string name) noexcept
        : type_(type)
        , key_(std::move(key))
        , name_(std::move(name))
        , description_{}
        , enabled_(true) {}

    /// @brief Virtual destructor for polymorphic behavior
    virtual ~ConfigBase() = default;

    // Move semantics (C++23 best practices)
    ConfigBase(ConfigBase&&) = default;
    ConfigBase& operator=(ConfigBase&&) = default;
    
    // Copy operations (needed for some derived classes)
    ConfigBase(const ConfigBase& other) = default;
    ConfigBase& operator=(const ConfigBase& other) = default;

    /// @brief Get configuration type
    /// @return Configuration type
    [[nodiscard]] constexpr ConfigType type() const noexcept { return type_; }

    /// @brief Get configuration type as string
    /// @return String representation of the type
    [[nodiscard]] constexpr std::string_view type_string() const noexcept { 
        return to_string(type_); 
    }

    /// @brief Get configuration key
    /// @return Unique identifier
    [[nodiscard]] const std::string& key() const noexcept { return key_; }

    /// @brief Get configuration name
    /// @return Display name
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    /// @brief Get configuration description
    /// @return Description (may be empty)
    [[nodiscard]] const std::string& description() const noexcept { return description_; }

    /// @brief Check if configuration is enabled
    /// @return True if enabled
    [[nodiscard]] constexpr bool enabled() const noexcept { return enabled_; }

    /// @brief Set configuration description
    /// @param desc Description text
    void set_description(std::string desc) { description_ = std::move(desc); }

    /// @brief Set enabled state
    /// @param enabled Whether configuration is enabled
    void set_enabled(bool enabled) noexcept { enabled_ = enabled; }

    /// @brief Check if this configuration is valid
    /// @return True if valid (to be overridden by derived classes)
    [[nodiscard]] virtual bool is_valid() const noexcept {
        return !key_.empty() && !name_.empty();
    }

    /// @brief Get a string representation for debugging
    /// @return Debug string
    [[nodiscard]] virtual std::string debug_string() const {
        return std::string(type_string()) + "[" + key_ + "]: " + name_ + 
               " (enabled: " + (enabled_ ? "true" : "false") + ")";
    }

protected:
    /// @brief Configuration type
    ConfigType type_;
    
    /// @brief Unique identifier
    std::string key_;
    
    /// @brief Display name
    std::string name_;
    
    /// @brief Optional description
    std::string description_;
    
    /// @brief Whether this configuration is enabled
    bool enabled_;
};

/// @brief Smart pointer type for configuration objects
using ConfigPtr = std::unique_ptr<ConfigBase>;

/// @brief Factory function signature for creating configurations
/// @param key Configuration key
/// @param name Configuration name
/// @return Smart pointer to created configuration
template<typename ConfigT>
concept ConfigurationConcept = std::derived_from<ConfigT, ConfigBase>;

/// @brief Create a configuration of the specified type
/// @tparam ConfigT Configuration type to create
/// @param key Configuration key
/// @param name Configuration name
/// @return Smart pointer to created configuration
template<ConfigurationConcept ConfigT, typename... Args>
[[nodiscard]] ConfigPtr make_config(Args&&... args) {
    return std::make_unique<ConfigT>(std::forward<Args>(args)...);
}

} // namespace ff8_hook::config 