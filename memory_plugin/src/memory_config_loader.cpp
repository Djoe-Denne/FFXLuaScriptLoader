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
        auto address_node = table.get("address");
        if (!address_node || !address_node->is_string()) {
            PLUGIN_LOG_ERROR("MemoryConfigLoader: Missing or invalid address field");
            return nullptr;
        }
        memory_config->set_address(parse_address(address_node->as_string()->get()));

        auto original_size_node = table.get("originalSize");
        if (!original_size_node || !original_size_node->is_integer()) {
            PLUGIN_LOG_ERROR("MemoryConfigLoader: Missing or invalid originalSize field");
            return nullptr;
        }
        memory_config->set_original_size(original_size_node->as_integer()->get());

        auto new_size_node = table.get("newSize");
        if (!new_size_node || !new_size_node->is_integer()) {
            PLUGIN_LOG_ERROR("MemoryConfigLoader: Missing or invalid newSize field");
            return nullptr;
        }
        memory_config->set_new_size(new_size_node->as_integer()->get());

        auto copy_after_node = table.get("copyAfter");
        if (!copy_after_node || !copy_after_node->is_string()) {
            PLUGIN_LOG_ERROR("MemoryConfigLoader: Missing or invalid copyAfter field");
            return nullptr;
        }
        memory_config->set_copy_after(parse_address(copy_after_node->as_string()->get()));

        // Parse optional fields
        auto description_node = table.get("description");
        if (description_node && description_node->is_string()) {
            memory_config->set_description(description_node->as_string()->get());
        }

        // Parse context configuration fields
        PLUGIN_LOG_DEBUG("MemoryConfigLoader: Looking for writeInContext in table for config: {}", name);
        
        auto write_in_context_node = table.get("writeInContext");
        if (write_in_context_node) {
            PLUGIN_LOG_DEBUG("MemoryConfigLoader: Found writeInContext node, type: {}", 
                           write_in_context_node->is_table() ? "table" : 
                           write_in_context_node->is_string() ? "string" :
                           write_in_context_node->is_boolean() ? "boolean" : "other");
            
            if (write_in_context_node->is_table()) {
                auto& write_context_table = *write_in_context_node->as_table();
                
                app_hook::config::WriteContextConfig write_config;
                
                auto enabled_node = write_context_table.get("enabled");
                if (enabled_node && enabled_node->is_boolean()) {
                    write_config.enabled = enabled_node->as_boolean()->get();
                    PLUGIN_LOG_DEBUG("MemoryConfigLoader: Parsed enabled: {}", write_config.enabled);
                }
                
                auto name_node = write_context_table.get("name");
                if (name_node && name_node->is_string()) {
                    write_config.name = name_node->as_string()->get();
                    PLUGIN_LOG_DEBUG("MemoryConfigLoader: Parsed name: '{}'", write_config.name);
                }
                
                // Log before moving
                PLUGIN_LOG_DEBUG("MemoryConfigLoader: Configured writeInContext - enabled: {}, name: '{}'", 
                               write_config.enabled, write_config.name);
                
                memory_config->set_write_in_context(std::move(write_config));
            }
        } else {
            PLUGIN_LOG_DEBUG("MemoryConfigLoader: No writeInContext node found");
        }

        auto read_from_context_node = table.get("readFromContext");
        if (read_from_context_node && read_from_context_node->is_string()) {
            memory_config->set_read_from_context(read_from_context_node->as_string()->get());
            PLUGIN_LOG_DEBUG("MemoryConfigLoader: Configured readFromContext: '{}'", 
                           read_from_context_node->as_string()->get());
        }

        PLUGIN_LOG_DEBUG("MemoryConfigLoader: Successfully parsed memory operation: {}", name);
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