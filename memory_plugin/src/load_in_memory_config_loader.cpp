#include "../include/config/load_in_memory_config_loader.hpp"
#include "plugin/plugin_interface.hpp"
#include <toml++/toml.hpp>
#include <filesystem>
#include <fstream>

namespace memory_plugin {

std::vector<app_hook::config::ConfigType> LoadInMemoryConfigLoader::supported_types() const {
    return {
        app_hook::config::ConfigType::Load
    };
}

app_hook::config::ConfigResult<std::vector<app_hook::config::ConfigPtr>> 
LoadInMemoryConfigLoader::load_configs(
    app_hook::config::ConfigType type, 
    const std::string& file_path, 
    const std::string& task_name) {
    
    PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Loading configs for type {} from file: {}", static_cast<int>(type), file_path);
    
    if (type == app_hook::config::ConfigType::Load) {
        return load_load_in_memory_configs(file_path, task_name);
    }
    
    PLUGIN_LOG_ERROR("LoadInMemoryConfigLoader: Unsupported config type: {}", static_cast<int>(type));
    return std::unexpected(app_hook::config::ConfigError::invalid_format);
}

std::string LoadInMemoryConfigLoader::get_name() const {
    return "Load In Memory Operations Loader";
}

std::string LoadInMemoryConfigLoader::get_version() const {
    return "1.0.0";
}

app_hook::config::ConfigResult<std::vector<app_hook::config::ConfigPtr>> 
LoadInMemoryConfigLoader::load_load_in_memory_configs(const std::string& file_path, const std::string& task_name) {
    PLUGIN_LOG_INFO("LoadInMemoryConfigLoader: Loading load in memory configs from file: {} for task: {}", file_path, task_name);
    
    if (!std::filesystem::exists(file_path)) {
        PLUGIN_LOG_ERROR("LoadInMemoryConfigLoader: Config file not found: {}", file_path);
        return std::unexpected(app_hook::config::ConfigError::file_not_found);
    }

    try {
        PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Parsing TOML file: {}", file_path);
        auto config = toml::parse_file(file_path);
        std::vector<app_hook::config::ConfigPtr> configs;

        // Parse load operations from TOML - support both array and table formats
        if (auto load_section = config.get("load")) {
            if (load_section->is_array()) {
                PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Found load section as array format");
                // Array format: [[load]]
                auto load_array = load_section->as_array();
                PLUGIN_LOG_INFO("LoadInMemoryConfigLoader: Processing {} load operations from array", load_array->size());
                
                for (auto&& op : *load_array) {
                    auto load_config = parse_load_operation(op, task_name);
                    if (load_config) {
                        PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Successfully parsed load operation");
                        configs.push_back(std::move(load_config));
                    } else {
                        PLUGIN_LOG_WARN("LoadInMemoryConfigLoader: Failed to parse load operation from array");
                    }
                }
            } else if (load_section->is_table()) {
                PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Found load section as table format");
                // Table format: [load.item]
                auto& load_table = *load_section->as_table();
                PLUGIN_LOG_INFO("LoadInMemoryConfigLoader: Processing {} load operations from table", load_table.size());
                
                for (auto& [key, value] : load_table) {
                    if (value.is_table()) {
                        PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Parsing load operation with key: {}", std::string(key));
                        auto load_config = parse_load_operation(value, task_name, std::string(key));
                        if (load_config) {
                            PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Successfully parsed load operation: {}", std::string(key));
                            configs.push_back(std::move(load_config));
                        } else {
                            PLUGIN_LOG_WARN("LoadInMemoryConfigLoader: Failed to parse load operation: {}", std::string(key));
                        }
                    } else {
                        PLUGIN_LOG_WARN("LoadInMemoryConfigLoader: Invalid load operation format for key: {}", std::string(key));
                    }
                }
            }
        } else {
            PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: No load section found in config file");
        }

        PLUGIN_LOG_INFO("LoadInMemoryConfigLoader: Successfully loaded {} load in memory configurations", configs.size());
        return configs;
    } catch (const toml::parse_error& e) {
        PLUGIN_LOG_ERROR("LoadInMemoryConfigLoader: TOML parse error in file {}: {}", file_path, e.what());
        return std::unexpected(app_hook::config::ConfigError::parse_error);
    } catch (const std::exception& e) {
        PLUGIN_LOG_ERROR("LoadInMemoryConfigLoader: Exception while loading configs from {}: {}", file_path, e.what());
        return std::unexpected(app_hook::config::ConfigError::invalid_format);
    }
}

app_hook::config::ConfigPtr LoadInMemoryConfigLoader::parse_load_operation(const toml::node& op, const std::string& task_name, const std::string& config_name) {
    try {
        if (!op.is_table()) {
            PLUGIN_LOG_ERROR("LoadInMemoryConfigLoader: Load operation is not a table");
            return nullptr;
        }
        auto& table = *op.as_table();
        
        // Use provided config_name or try to get from table
        std::string name = config_name;
        if (name.empty()) {
            auto name_node = table.get("name");
            if (!name_node || !name_node->is_string()) {
                PLUGIN_LOG_ERROR("LoadInMemoryConfigLoader: Missing or invalid name field in load operation");
                return nullptr;
            }
            name = name_node->as_string()->get();
        }
        
        PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Creating load in memory config for: {}", name);
        auto config = app_hook::config::make_config<app_hook::config::LoadInMemoryConfig>(
            task_name + "_" + name, name);
        auto* load_config = static_cast<app_hook::config::LoadInMemoryConfig*>(config.get());

        // Parse required field: binary
        auto binary_node = table.get("binary");
        if (!binary_node || !binary_node->is_string()) {
            PLUGIN_LOG_ERROR("LoadInMemoryConfigLoader: Missing or invalid binary field");
            return nullptr;
        }
        load_config->set_binary_path(binary_node->as_string()->get());
        PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Set binary path: {}", binary_node->as_string()->get());

        // Parse optional field: offsetSecurity
        auto offset_security_node = table.get("offsetSecurity");
        if (offset_security_node) {
            if (offset_security_node->is_string()) {
                auto offset_security = parse_address(offset_security_node->as_string()->get());
                load_config->set_offset_security(offset_security);
                PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Set offset security: 0x{:X}", offset_security);
            } else if (offset_security_node->is_integer()) {
                auto offset_security = static_cast<std::uintptr_t>(offset_security_node->as_integer()->get());
                load_config->set_offset_security(offset_security);
                PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Set offset security: 0x{:X}", offset_security);
            } else {
                PLUGIN_LOG_WARN("LoadInMemoryConfigLoader: Invalid offsetSecurity field type, using default 0");
            }
        }

        // Parse optional field: description
        auto description_node = table.get("description");
        if (description_node && description_node->is_string()) {
            load_config->set_description(description_node->as_string()->get());
            PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Set description: {}", description_node->as_string()->get());
        }

        // Parse context configuration fields
        PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Looking for writeInContext in table for config: {}", name);
        
        auto write_in_context_node = table.get("writeInContext");
        if (write_in_context_node) {
            PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Found writeInContext node, type: {}", 
                           write_in_context_node->is_table() ? "table" : 
                           write_in_context_node->is_string() ? "string" :
                           write_in_context_node->is_boolean() ? "boolean" : "other");
            
            if (write_in_context_node->is_table()) {
                auto& write_context_table = *write_in_context_node->as_table();
                
                app_hook::config::WriteContextConfig write_config;
                
                auto enabled_node = write_context_table.get("enabled");
                if (enabled_node && enabled_node->is_boolean()) {
                    write_config.enabled = enabled_node->as_boolean()->get();
                    PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Parsed enabled: {}", write_config.enabled);
                }
                
                auto name_node = write_context_table.get("name");
                if (name_node && name_node->is_string()) {
                    write_config.name = name_node->as_string()->get();
                    PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Parsed name: '{}'", write_config.name);
                }
                
                // Log before moving
                PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Configured writeInContext - enabled: {}, name: '{}'", 
                               write_config.enabled, write_config.name);
                
                load_config->set_write_in_context(std::move(write_config));
            }
        } else {
            PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: No writeInContext node found");
        }

        auto read_from_context_node = table.get("readFromContext");
        if (read_from_context_node && read_from_context_node->is_string()) {
            load_config->set_read_from_context(read_from_context_node->as_string()->get());
            PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Configured readFromContext: '{}'", 
                           read_from_context_node->as_string()->get());
        }

        PLUGIN_LOG_DEBUG("LoadInMemoryConfigLoader: Successfully parsed load operation: {}", name);
        return config;

    } catch (const std::exception& e) {
        PLUGIN_LOG_ERROR("LoadInMemoryConfigLoader: Exception while parsing load operation: {}", e.what());
        return nullptr;
    }
}

std::uintptr_t LoadInMemoryConfigLoader::parse_address(const std::string& value) {
    // Note: Cannot use PLUGIN_LOG in static methods as they don't have access to host_
    if (value.starts_with("0x") || value.starts_with("0X")) {
        return static_cast<std::uintptr_t>(std::stoull(value, nullptr, 16));
    }
    return static_cast<std::uintptr_t>(std::stoull(value));
}

} // namespace memory_plugin 