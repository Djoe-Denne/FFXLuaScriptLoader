#include "config/copy_memory_loader.hpp"

namespace app_hook::config {

ConfigResult<std::vector<CopyMemoryConfig>> 
CopyMemoryLoader::load_memory_configs(const std::string& file_path) {
    // Simple logging replacement for plugin environment
    // LOG_INFO("Attempting to load configuration from: {}", file_path);
    
    std::vector<CopyMemoryConfig> configs;
    
    try {
        // Parse TOML file
        auto toml = toml::parse_file(file_path);
        
        // Process memory sections using ranges (C++23 style)
        auto memory_sections = extract_memory_sections(toml);
        
        for (const auto& [section_key, section_value] : memory_sections) {
            if (auto config = parse_memory_section(section_key, section_value)) {
                configs.push_back(std::move(*config));
            }
        }
        
    } catch (const toml::parse_error& e) {
        return std::unexpected(ConfigError::parse_error);
    } catch (const std::exception& e) {
        return std::unexpected(ConfigError::parse_error);
    }
    
    return configs;
}

std::vector<std::pair<std::string, const toml::table*>>
CopyMemoryLoader::extract_memory_sections(const toml::table& toml) {
    std::vector<std::pair<std::string, const toml::table*>> sections;
    
    // Handle nested TOML structure: [memory.K_MAGIC] becomes memory -> K_MAGIC
    for (const auto& [key, value] : toml) {
        std::string key_str{key.str()};
        
        // Look for "memory" section
        if (key_str != "memory") {
            continue;
        }
        
        if (!value.is_table()) {
            continue;
        }
        
        // Process all sub-tables within the memory section
        auto memory_table = value.as_table();
        for (const auto& [sub_key, sub_value] : *memory_table) {
            std::string sub_key_str{sub_key.str()};
            std::string full_key = "memory." + sub_key_str;
            
            if (!sub_value.is_table()) {
                continue;
            }
            
            sections.emplace_back(full_key, sub_value.as_table());
        }
    }
    
    return sections;
}

std::optional<CopyMemoryConfig> 
CopyMemoryLoader::parse_memory_section(const std::string& section_key, const toml::table* table) {
    // Create a temporary struct-style config for backward compatibility
    struct TempCopyMemoryConfig {
        std::string key;
        std::uintptr_t address = 0;
        std::uintptr_t copy_after = 0;
        std::size_t original_size = 0;
        std::size_t new_size = 0;
        std::string description;
        std::optional<std::string> patch;
        
        bool is_valid() const noexcept {
            return address != 0 && copy_after != 0 && 
                   original_size > 0 && new_size >= original_size;
        }
    };
    
    TempCopyMemoryConfig temp_config;
    temp_config.key = section_key;
    
    // Parse required fields using modern C++ features
    auto parse_field = [&]<typename T>(const std::string& field_name, auto setter) -> bool {
        if (auto field = table->get(field_name)) {
            if (auto value = field->value<T>()) {
                setter(*value);
                return true;
            }
        }
        return false;
    };
    
    // Parse address
    if (!parse_field.template operator()<std::string>("address", [&](const std::string& addr_str) {
        temp_config.address = ConfigParsingUtils::parse_address(addr_str);
    })) {
        return std::nullopt;
    }
    
    // Parse originalSize
    if (!parse_field.template operator()<std::int64_t>("originalSize", [&](std::int64_t size_val) {
        temp_config.original_size = static_cast<std::size_t>(size_val);
    })) {
        return std::nullopt;
    }
    
    // Parse newSize
    if (!parse_field.template operator()<std::int64_t>("newSize", [&](std::int64_t size_val) {
        temp_config.new_size = static_cast<std::size_t>(size_val);
    })) {
        return std::nullopt;
    }
    
    // Parse copyAfter
    if (!parse_field.template operator()<std::string>("copyAfter", [&](const std::string& addr_str) {
        temp_config.copy_after = ConfigParsingUtils::parse_address(addr_str);
    })) {
        return std::nullopt;
    }
    
    // Parse optional fields
    parse_field.template operator()<std::string>("description", [&](const std::string& desc_str) {
        temp_config.description = desc_str;
    });
    
    // Validate configuration
    if (!temp_config.is_valid()) {
        return std::nullopt;
    }
    
    // Convert to proper CopyMemoryConfig object
    auto config = CopyMemoryConfig(temp_config.key, temp_config.key);
    config.set_address(temp_config.address);
    config.set_copy_after(temp_config.copy_after);
    config.set_original_size(temp_config.original_size);
    config.set_new_size(temp_config.new_size);
    config.set_description(temp_config.description);
    
    return config;
}

} // namespace app_hook::config 