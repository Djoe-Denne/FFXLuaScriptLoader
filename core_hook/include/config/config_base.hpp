#pragma once

#include <string>
#include <memory>
#include <string_view>
#include <cstdint>
#include <optional>

namespace app_hook::config {

/// @brief Configuration type enumeration
enum class ConfigType : std::uint8_t {
    Unknown = 0,
    Memory,     ///< Memory expansion configuration
    Patch,      ///< Instruction patch configuration
    Load,       ///< Load binary data configuration
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
        case ConfigType::Load:     return "load";
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
    if (type_str == "load")     return ConfigType::Load;
    if (type_str == "script")   return ConfigType::Script;
    if (type_str == "audio")    return ConfigType::Audio;
    if (type_str == "graphics") return ConfigType::Graphics;
    return ConfigType::Unknown;
}

/// @brief Interface for configurations that can provide hook addresses
/// @note Configurations implementing this interface can be used as address triggers for hooks
class AddressTrigger {
public:
    virtual ~AddressTrigger() = default;
    
    /// @brief Get the hook address for this configuration
    /// @return Hook address where the hook should be installed
    [[nodiscard]] virtual std::uintptr_t get_hook_address() const noexcept = 0;
    
    /// @brief Check if this configuration provides a valid hook address
    /// @return True if hook address is valid (non-zero)
    [[nodiscard]] virtual bool has_valid_hook_address() const noexcept {
        return get_hook_address() != 0;
    }
};

/// @brief Configuration for writing data to context
struct WriteContextConfig {
    bool enabled = false;           ///< Whether to write to context
    std::string name;              ///< Name/key to use when writing to context
    
    /// @brief Check if write context config is valid
    [[nodiscard]] bool is_valid() const noexcept {
        return !enabled || !name.empty();
    }
};

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
        , enabled_(true)
        , write_in_context_{}
        , read_from_context_{} {}

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

    /// @brief Get write context configuration
    /// @return Write context config
    [[nodiscard]] const WriteContextConfig& write_in_context() const noexcept { 
        return write_in_context_; 
    }

    /// @brief Get read from context key
    /// @return Key to read from context (empty if not used)
    [[nodiscard]] const std::string& read_from_context() const noexcept { 
        return read_from_context_; 
    }

    /// @brief Set write context configuration
    /// @param config Write context configuration
    void set_write_in_context(WriteContextConfig config) { 
        write_in_context_ = std::move(config); 
    }

    /// @brief Set read from context key
    /// @param key Key to read from context
    void set_read_from_context(std::string key) { 
        read_from_context_ = std::move(key); 
    }

    /// @brief Check if this configuration writes to context
    /// @return True if configured to write to context
    [[nodiscard]] bool writes_to_context() const noexcept {
        return write_in_context_.enabled;
    }

    /// @brief Check if this configuration reads from context
    /// @return True if configured to read from context
    [[nodiscard]] bool reads_from_context() const noexcept {
        return !read_from_context_.empty();
    }

    /// @brief Check if this configuration is valid
    /// @return True if valid (to be overridden by derived classes)
    [[nodiscard]] virtual bool is_valid() const noexcept {
        return !key_.empty() && !name_.empty() && write_in_context_.is_valid();
    }

    /// @brief Get a string representation for debugging
    /// @return Debug string
    [[nodiscard]] virtual std::string debug_string() const {
        return std::string(type_string()) + "[" + key_ + "]: " + name_ + 
               " (enabled: " + (enabled_ ? "true" : "false") + ")";
    }

    /// @brief Check if this configuration is an address trigger
    /// @return True if this config implements AddressTrigger interface
    [[nodiscard]] bool is_address_trigger() const noexcept {
        return dynamic_cast<const AddressTrigger*>(this) != nullptr;
    }

    /// @brief Get hook address if this configuration is an address trigger
    /// @return Hook address or 0 if not an address trigger
    [[nodiscard]] std::uintptr_t get_hook_address_if_trigger() const noexcept {
        if (auto* trigger = dynamic_cast<const AddressTrigger*>(this)) {
            return trigger->get_hook_address();
        }
        return 0;
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
    
    /// @brief Context write configuration
    WriteContextConfig write_in_context_;
    
    /// @brief Key to read from context
    std::string read_from_context_;
};

/// @brief Smart pointer type for configuration objects
using ConfigPtr = std::shared_ptr<ConfigBase>;

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
    return std::make_shared<ConfigT>(std::forward<Args>(args)...);
}

} // namespace app_hook::config 
