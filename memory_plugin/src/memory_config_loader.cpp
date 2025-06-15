#include "../include/config/memory_config_loader.hpp"
#include "plugin/plugin_interface.hpp"
#include <toml++/toml.hpp>
#include <filesystem>
#include <fstream>

namespace memory_plugin {

std::vector<app_hook::config::ConfigType> MemoryConfigLoader::supported_types() const {
    return {
        app_hook::config::ConfigType::Memory
    };
}

app_hook::config::ConfigResult<std::vector<app_hook::config::ConfigPtr>> 
MemoryConfigLoader::load_configs(
    app_hook::config::ConfigType type, 
    const std::string& file_path, 
    const std::string& task_name) {
    
    PLUGIN_LOG_DEBUG("MemoryConfigLoader: Loading configs for type {} from file: {}", static_cast<int>(type), file_path);
    
    if (type == app_hook::config::ConfigType::Memory) {
        return load_memory_configs(file_path, task_name);
    }
    
    PLUGIN_LOG_ERROR("MemoryConfigLoader: Unsupported config type: {}", static_cast<int>(type));
    return std::unexpected(app_hook::config::ConfigError::invalid_format);
}

std::string MemoryConfigLoader::get_name() const {
    return "Memory Operations Loader";
}

std::string MemoryConfigLoader::get_version() const {
    return "1.0.0";
}

app_hook::config::ConfigResult<std::vector<app_hook::config::ConfigPtr>> 
MemoryConfigLoader::load_memory_configs(const std::string& file_path, const std::string& task_name) {
    PLUGIN_LOG_INFO("MemoryConfigLoader: Loading memory configs from file: {} for task: {}", file_path, task_name);
    
    if (!std::filesystem::exists(file_path)) {
        PLUGIN_LOG_ERROR("MemoryConfigLoader: Config file not found: {}", file_path);
        return std::unexpected(app_hook::config::ConfigError::file_not_found);
    }

    try {
        PLUGIN_LOG_DEBUG("MemoryConfigLoader: Parsing TOML file: {}", file_path);
        auto config = toml::parse_file(file_path);
        std::vector<app_hook::config::ConfigPtr> configs;

        // Parse memory operations from TOML - support both array and table formats
        if (auto memory_section = config.get("memory")) {
            if (memory_section->is_array()) {
                PLUGIN_LOG_DEBUG("MemoryConfigLoader: Found memory section as array format");
                // Array format: [[memory]]
                auto memory_array = memory_section->as_array();
                PLUGIN_LOG_INFO("MemoryConfigLoader: Processing {} memory operations from array", memory_array->size());
                
                for (auto&& op : *memory_array) {
                    auto memory_config = parse_memory_operation(op, task_name);
                    if (memory_config) {
                        PLUGIN_LOG_DEBUG("MemoryConfigLoader: Successfully parsed memory operation");
                        configs.push_back(std::move(memory_config));
                    } else {
                        PLUGIN_LOG_WARN("MemoryConfigLoader: Failed to parse memory operation from array");
                    }
                }
            } else if (memory_section->is_table()) {
                PLUGIN_LOG_DEBUG("MemoryConfigLoader: Found memory section as table format");
                // Table format: [memory.item]
                auto& memory_table = *memory_section->as_table();
                PLUGIN_LOG_INFO("MemoryConfigLoader: Processing {} memory operations from table", memory_table.size());
                
                for (auto& [key, value] : memory_table) {
                    if (value.is_table()) {
                        PLUGIN_LOG_DEBUG("MemoryConfigLoader: Parsing memory operation with key: {}", std::string(key));
                        auto memory_config = parse_memory_operation(value, task_name, std::string(key));
                        if (memory_config) {
                            PLUGIN_LOG_DEBUG("MemoryConfigLoader: Successfully parsed memory operation: {}", std::string(key));
                            configs.push_back(std::move(memory_config));
                        } else {
                            PLUGIN_LOG_WARN("MemoryConfigLoader: Failed to parse memory operation: {}", std::string(key));
                        }
                    } else {
                        PLUGIN_LOG_WARN("MemoryConfigLoader: Invalid memory operation format for key: {}", std::string(key));
                    }
                }
            }
        } else {
            PLUGIN_LOG_DEBUG("MemoryConfigLoader: No memory section found in config file");
        }

        PLUGIN_LOG_INFO("MemoryConfigLoader: Successfully loaded {} memory configurations", configs.size());
        return configs;
    } catch (const toml::parse_error& e) {
        PLUGIN_LOG_ERROR("MemoryConfigLoader: TOML parse error in file {}: {}", file_path, e.what());
        return std::unexpected(app_hook::config::ConfigError::parse_error);
    } catch (const std::exception& e) {
        PLUGIN_LOG_ERROR("MemoryConfigLoader: Exception while loading configs from {}: {}", file_path, e.what());
        return std::unexpected(app_hook::config::ConfigError::invalid_format);
    }
}

app_hook::config::ConfigPtr MemoryConfigLoader::parse_memory_operation(const toml::node& op, const std::string& task_name, const std::string& config_name) {
    try {
        if (!op.is_table()) {
            PLUGIN_LOG_ERROR("MemoryConfigLoader: Memory operation is not a table");
            return nullptr;
        }
        auto& table = *op.as_table();
        
        // Use provided config_name or try to get from table
        std::string name = config_name;
        if (name.empty()) {
            auto name_node = table.get("name");
            if (!name_node || !name_node->is_string()) {
                PLUGIN_LOG_ERROR("MemoryConfigLoader: Missing or invalid name field in memory operation");
                return nullptr;
            }
            name = name_node->as_string()->get();
        }
        
        PLUGIN_LOG_DEBUG("MemoryConfigLoader: Creating memory config for: {}", name);
        auto config = app_hook::config::make_config<app_hook::config::CopyMemoryConfig>(
            task_name + "_" + name, name);
        auto* memory_config = static_cast<app_hook::config::CopyMemoryConfig*>(config.get());

        // Parse required fields
        if (auto address_node = table.get("address")) {
            if (address_node->is_string()) {
                auto address = parse_address(address_node->as_string()->get());
                PLUGIN_LOG_DEBUG("MemoryConfigLoader: Parsed address: 0x{:X}", address);
                memory_config->set_address(address);
            } else {
                PLUGIN_LOG_ERROR("MemoryConfigLoader: Invalid address field type in memory operation: {}", name);
                return nullptr;
            }
        } else {
            PLUGIN_LOG_ERROR("MemoryConfigLoader: Missing address field in memory operation: {}", name);
            return nullptr;
        }

        if (auto copy_after_node = table.get("copyAfter")) {
            if (copy_after_node->is_string()) {
                auto copy_after = parse_address(copy_after_node->as_string()->get());
                PLUGIN_LOG_DEBUG("MemoryConfigLoader: Parsed copyAfter: 0x{:X}", copy_after);
                memory_config->set_copy_after(copy_after);
            } else {
                PLUGIN_LOG_ERROR("MemoryConfigLoader: Invalid copyAfter field type in memory operation: {}", name);
                return nullptr;
            }
        } else {
            PLUGIN_LOG_ERROR("MemoryConfigLoader: Missing copyAfter field in memory operation: {}", name);
            return nullptr;
        }

        if (auto original_size_node = table.get("originalSize")) {
            if (original_size_node->is_integer()) {
                auto original_size = static_cast<std::size_t>(original_size_node->as_integer()->get());
                PLUGIN_LOG_DEBUG("MemoryConfigLoader: Parsed originalSize: {}", original_size);
                memory_config->set_original_size(original_size);
            } else {
                PLUGIN_LOG_ERROR("MemoryConfigLoader: Invalid originalSize field type in memory operation: {}", name);
                return nullptr;
            }
        } else {
            PLUGIN_LOG_ERROR("MemoryConfigLoader: Missing originalSize field in memory operation: {}", name);
            return nullptr;
        }

        if (auto new_size_node = table.get("newSize")) {
            if (new_size_node->is_integer()) {
                auto new_size = static_cast<std::size_t>(new_size_node->as_integer()->get());
                PLUGIN_LOG_DEBUG("MemoryConfigLoader: Parsed newSize: {}", new_size);
                memory_config->set_new_size(new_size);
            } else {
                PLUGIN_LOG_ERROR("MemoryConfigLoader: Invalid newSize field type in memory operation: {}", name);
                return nullptr;
            }
        } else {
            PLUGIN_LOG_ERROR("MemoryConfigLoader: Missing newSize field in memory operation: {}", name);
            return nullptr;
        }

        // Optional fields
        if (auto description_node = table.get("description")) {
            if (description_node->is_string()) {
                auto description = description_node->as_string()->get();
                PLUGIN_LOG_DEBUG("MemoryConfigLoader: Parsed description: {}", description);
                memory_config->set_description(description);
            }
        }

        if (auto enabled_node = table.get("enabled")) {
            if (enabled_node->is_boolean()) {
                auto enabled = enabled_node->as_boolean()->get();
                PLUGIN_LOG_DEBUG("MemoryConfigLoader: Parsed enabled: {}", enabled);
                memory_config->set_enabled(enabled);
            }
        }

        PLUGIN_LOG_INFO("MemoryConfigLoader: Successfully created memory config: {}", name);
        return config;
    } catch (const std::exception& e) {
        PLUGIN_LOG_ERROR("MemoryConfigLoader: Exception while parsing memory operation: {}", e.what());
        return nullptr;
    }
}

std::uintptr_t MemoryConfigLoader::parse_address(const std::string& value) {
    // Note: Cannot use PLUGIN_LOG in static methods as they don't have access to host_
    if (value.starts_with("0x") || value.starts_with("0X")) {
        return static_cast<std::uintptr_t>(std::stoull(value, nullptr, 16));
    }
    return static_cast<std::uintptr_t>(std::stoull(value));
}

} // namespace memory_plugin 