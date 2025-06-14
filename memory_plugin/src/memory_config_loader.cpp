#include "memory_config_loader.hpp"
#include <toml++/toml.hpp>
#include <filesystem>
#include <fstream>

namespace memory_plugin {

std::vector<app_hook::config::ConfigType> MemoryConfigLoader::supported_types() const {
    return {
        app_hook::config::ConfigType::Memory,
        app_hook::config::ConfigType::Patch
    };
}

app_hook::config::ConfigResult<std::vector<app_hook::config::ConfigPtr>> 
MemoryConfigLoader::load_configs(
    app_hook::config::ConfigType type, 
    const std::string& file_path, 
    const std::string& task_name) {
    
    switch (type) {
        case app_hook::config::ConfigType::Memory:
            return load_memory_configs(file_path, task_name);
        
        case app_hook::config::ConfigType::Patch:
            return load_patch_configs(file_path, task_name);
        
        default:
            return std::unexpected(app_hook::config::ConfigError::invalid_format);
    }
}

std::string MemoryConfigLoader::get_name() const {
    return "Memory Operations Loader";
}

std::string MemoryConfigLoader::get_version() const {
    return "1.0.0";
}

app_hook::config::ConfigResult<std::vector<app_hook::config::ConfigPtr>> 
MemoryConfigLoader::load_memory_configs(const std::string& file_path, const std::string& task_name) {
    if (!std::filesystem::exists(file_path)) {
        return std::unexpected(app_hook::config::ConfigError::file_not_found);
    }

    try {
        auto config = toml::parse_file(file_path);
        std::vector<app_hook::config::ConfigPtr> configs;

        // Parse memory operations from TOML  
        if (auto memory_ops = config.get("memory")) {
            if (memory_ops && memory_ops->is_array()) {
                for (auto&& op : *memory_ops->as_array()) {
                    auto memory_config = parse_memory_operation(op, task_name);
                    if (memory_config) {
                        configs.push_back(std::move(memory_config));
                    }
                }
            }
        }

        return configs;
    } catch (const toml::parse_error&) {
        return std::unexpected(app_hook::config::ConfigError::parse_error);
    } catch (const std::exception&) {
        return std::unexpected(app_hook::config::ConfigError::invalid_format);
    }
}

app_hook::config::ConfigResult<std::vector<app_hook::config::ConfigPtr>> 
MemoryConfigLoader::load_patch_configs(const std::string& file_path, const std::string& task_name) {
    if (!std::filesystem::exists(file_path)) {
        return std::unexpected(app_hook::config::ConfigError::file_not_found);
    }

    try {
        auto config = toml::parse_file(file_path);
        std::vector<app_hook::config::ConfigPtr> configs;

        // Parse patch operations from TOML
        if (auto patch_ops = config.get("patch")) {
            if (patch_ops && patch_ops->is_array()) {
                auto patch_config = app_hook::config::make_config<app_hook::config::PatchConfig>(task_name, task_name);
                auto* patch_config_ptr = static_cast<app_hook::config::PatchConfig*>(patch_config.get());
                patch_config_ptr->set_patch_file_path(file_path);
                
                std::vector<app_hook::config::InstructionPatch> instructions;
                for (auto&& op : *patch_ops->as_array()) {
                    auto instruction = parse_patch_operation(op);
                    if (instruction) {
                        instructions.push_back(*instruction);
                    }
                }
                
                if (!instructions.empty()) {
                    patch_config_ptr->set_instructions(std::move(instructions));
                    configs.push_back(std::move(patch_config));
                }
            }
        }

        return configs;
    } catch (const toml::parse_error&) {
        return std::unexpected(app_hook::config::ConfigError::parse_error);
    } catch (const std::exception&) {
        return std::unexpected(app_hook::config::ConfigError::invalid_format);
    }
}

app_hook::config::ConfigPtr MemoryConfigLoader::parse_memory_operation(const toml::node& op, const std::string& task_name) {
    try {
        if (!op.is_table()) return nullptr;
        auto& table = *op.as_table();
        
        auto name = table.get("name");
        if (!name || !name->is_string()) return nullptr;
        
        auto config = app_hook::config::make_config<app_hook::config::CopyMemoryConfig>(
            task_name + "_" + name->as_string()->get(), name->as_string()->get());
        auto* memory_config = static_cast<app_hook::config::CopyMemoryConfig*>(config.get());

        // Parse required fields
        if (auto address_node = table.get("address")) {
            if (address_node->is_string()) {
                memory_config->set_address(parse_address(address_node->as_string()->get()));
            } else {
                return nullptr;
            }
        } else {
            return nullptr;
        }

        if (auto copy_after_node = table.get("copy_after")) {
            if (copy_after_node->is_string()) {
                memory_config->set_copy_after(parse_address(copy_after_node->as_string()->get()));
            } else {
                return nullptr;
            }
        } else {
            return nullptr;
        }

        if (auto original_size_node = table.get("original_size")) {
            if (original_size_node->is_integer()) {
                memory_config->set_original_size(static_cast<std::size_t>(original_size_node->as_integer()->get()));
            } else {
                return nullptr;
            }
        } else {
            return nullptr;
        }

        if (auto new_size_node = table.get("new_size")) {
            if (new_size_node->is_integer()) {
                memory_config->set_new_size(static_cast<std::size_t>(new_size_node->as_integer()->get()));
            } else {
                return nullptr;
            }
        } else {
            return nullptr;
        }

        // Optional fields
        if (auto description_node = table.get("description")) {
            if (description_node->is_string()) {
                memory_config->set_description(description_node->as_string()->get());
            }
        }

        if (auto enabled_node = table.get("enabled")) {
            if (enabled_node->is_boolean()) {
                memory_config->set_enabled(enabled_node->as_boolean()->get());
            }
        }

        return config;
    } catch (const std::exception&) {
        return nullptr;
    }
}

std::optional<app_hook::config::InstructionPatch> MemoryConfigLoader::parse_patch_operation(const toml::node& op) {
    try {
        if (!op.is_table()) return std::nullopt;
        auto& table = *op.as_table();
        
        app_hook::config::InstructionPatch patch;

        if (auto address_node = table.get("address")) {
            if (address_node->is_string()) {
                patch.address = parse_address(address_node->as_string()->get());
            } else {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }

        if (auto bytes_node = table.get("bytes")) {
            if (bytes_node->is_array()) {
                for (auto&& byte_node : *bytes_node->as_array()) {
                    if (byte_node.is_integer()) {
                        patch.bytes.push_back(static_cast<std::uint8_t>(byte_node.as_integer()->get()));
                    }
                }
            } else {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }

        if (auto offset_node = table.get("offset")) {
            if (offset_node->is_string()) {
                patch.offset = parse_offset(offset_node->as_string()->get());
            } else if (offset_node->is_integer()) {
                patch.offset = static_cast<std::int32_t>(offset_node->as_integer()->get());
            } else {
                patch.offset = 0;
            }
        } else {
            patch.offset = 0;
        }

        return patch.is_valid() ? std::make_optional(patch) : std::nullopt;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::uintptr_t MemoryConfigLoader::parse_address(const std::string& value) {
    if (value.starts_with("0x") || value.starts_with("0X")) {
        return static_cast<std::uintptr_t>(std::stoull(value, nullptr, 16));
    }
    return static_cast<std::uintptr_t>(std::stoull(value));
}

std::int32_t MemoryConfigLoader::parse_offset(const std::string& offset_str) {
    if (offset_str.starts_with("-")) {
        const auto positive_part = offset_str.substr(1);
        if (positive_part.starts_with("0x") || positive_part.starts_with("0X")) {
            return -static_cast<std::int32_t>(std::stoul(positive_part, nullptr, 16));
        }
        return -static_cast<std::int32_t>(std::stoul(positive_part));
    }
    
    if (offset_str.starts_with("0x") || offset_str.starts_with("0X")) {
        return static_cast<std::int32_t>(std::stoul(offset_str, nullptr, 16));
    }
    return static_cast<std::int32_t>(std::stoul(offset_str));
}

} // namespace memory_plugin 