#include "../include/config/patch_config_loader.hpp"
#include "../../core_hook/include/util/logger.hpp"
#include <toml++/toml.hpp>
#include <filesystem>

namespace memory_plugin {

std::vector<app_hook::config::ConfigType> PatchConfigLoader::supported_types() const {
    return {
        app_hook::config::ConfigType::Patch
    };
}

app_hook::config::ConfigResult<std::vector<app_hook::config::ConfigPtr>> 
PatchConfigLoader::load_configs(
    app_hook::config::ConfigType type, 
    const std::string& file_path, 
    const std::string& task_name) {
    
    LOG_DEBUG("PatchConfigLoader: Loading configs for type {} from file: {}", static_cast<int>(type), file_path);
    
    if (type == app_hook::config::ConfigType::Patch) {
        return load_patch_configs(file_path, task_name);
    }
    
    LOG_ERROR("PatchConfigLoader: Unsupported config type: {}", static_cast<int>(type));
    return std::unexpected(app_hook::config::ConfigError::invalid_format);
}

std::string PatchConfigLoader::get_name() const {
    return "Patch Operations Loader";
}

std::string PatchConfigLoader::get_version() const {
    return "1.0.0";
}

app_hook::config::ConfigResult<std::vector<app_hook::config::ConfigPtr>> 
PatchConfigLoader::load_patch_configs(const std::string& file_path, const std::string& task_name) {
    LOG_INFO("PatchConfigLoader: Loading patch configs from file: {} for task: {}", file_path, task_name);
    
    if (!std::filesystem::exists(file_path)) {
        LOG_ERROR("PatchConfigLoader: Config file not found: {}", file_path);
        return std::unexpected(app_hook::config::ConfigError::file_not_found);
    }

    try {
        LOG_DEBUG("PatchConfigLoader: Parsing TOML file: {}", file_path);
        auto config = toml::parse_file(file_path);
        std::vector<app_hook::config::ConfigPtr> configs;

        // Parse patch operations from TOML
        if (auto patch_section = config.get("patch")) {
            if (patch_section->is_table()) {
                LOG_DEBUG("PatchConfigLoader: Found patch section as table format");
                // Table format: [patch.item]
                auto& patch_table = *patch_section->as_table();
                LOG_INFO("PatchConfigLoader: Processing {} patch instructions", patch_table.size());
                
                auto patch_config = app_hook::config::make_config<app_hook::config::PatchConfig>(task_name, task_name);
                auto* patch_config_ptr = static_cast<app_hook::config::PatchConfig*>(patch_config.get());
                patch_config_ptr->set_patch_file_path(file_path);
                
                std::vector<app_hook::config::InstructionPatch> instructions;
                for (auto& [key, value] : patch_table) {
                    if (value.is_table()) {
                        LOG_DEBUG("PatchConfigLoader: Parsing instruction with key: {}", std::string(key));
                        auto instruction = parse_single_instruction(std::string(key), value);
                        if (instruction) {
                            LOG_DEBUG("PatchConfigLoader: Successfully parsed instruction: {}", std::string(key));
                            instructions.push_back(*instruction);
                        } else {
                            LOG_WARN("PatchConfigLoader: Failed to parse instruction: {}", std::string(key));
                        }
                    } else {
                        LOG_WARN("PatchConfigLoader: Invalid instruction format for key: {}", std::string(key));
                    }
                }
                
                if (!instructions.empty()) {
                    LOG_INFO("PatchConfigLoader: Successfully parsed {} instructions", instructions.size());
                    patch_config_ptr->set_instructions(std::move(instructions));
                    configs.push_back(std::move(patch_config));
                } else {
                    LOG_WARN("PatchConfigLoader: No valid instructions found in patch section");
                }
            } else {
                LOG_WARN("PatchConfigLoader: Patch section is not a table");
            }
        } else {
            LOG_DEBUG("PatchConfigLoader: No patch section found in config file");
        }

        LOG_INFO("PatchConfigLoader: Successfully loaded {} patch configurations", configs.size());
        return configs;
    } catch (const toml::parse_error& e) {
        LOG_ERROR("PatchConfigLoader: TOML parse error in file {}: {}", file_path, e.what());
        return std::unexpected(app_hook::config::ConfigError::parse_error);
    } catch (const std::exception& e) {
        LOG_ERROR("PatchConfigLoader: Exception while loading configs from {}: {}", file_path, e.what());
        return std::unexpected(app_hook::config::ConfigError::invalid_format);
    }
}

std::optional<app_hook::config::InstructionPatch> 
PatchConfigLoader::parse_single_instruction(const std::string& key_str, const toml::node& value) {
    LOG_TRACE("PatchConfigLoader: Parsing instruction with key: {}", key_str);
    
    if (!value.is_table()) {
        LOG_ERROR("PatchConfigLoader: Instruction value is not a table for key: {}", key_str);
        return std::nullopt;
    }
    
    app_hook::config::InstructionPatch patch;
    
    // Parse address from key (format: 0x1234ABCD)
    try {
        patch.address = parse_address(key_str);
        LOG_DEBUG("PatchConfigLoader: Parsed address: 0x{:X}", patch.address);
    } catch (const std::exception& e) {
        LOG_ERROR("PatchConfigLoader: Failed to parse address from key '{}': {}", key_str, e.what());
        return std::nullopt;
    }
    
    auto table = value.as_table();
    
    // Parse bytes field
    if (auto bytes_node = table->get("bytes")) {
        if (auto bytes_str = bytes_node->value<std::string>()) {
            try {
                patch.bytes = parse_bytes_string(*bytes_str);
                LOG_DEBUG("PatchConfigLoader: Parsed {} bytes from string: {}", patch.bytes.size(), *bytes_str);
            } catch (const std::exception& e) {
                LOG_ERROR("PatchConfigLoader: Failed to parse bytes string '{}': {}", *bytes_str, e.what());
                return std::nullopt;
            }
        } else {
            LOG_ERROR("PatchConfigLoader: Bytes field is not a string for instruction: {}", key_str);
            return std::nullopt;
        }
    } else {
        LOG_ERROR("PatchConfigLoader: Missing bytes field for instruction: {}", key_str);
        return std::nullopt;
    }
    
    // Parse offset field
    if (auto offset_node = table->get("offset")) {
        if (auto offset_str = offset_node->value<std::string>()) {
            try {
                patch.offset = parse_offset(*offset_str);
                LOG_DEBUG("PatchConfigLoader: Parsed offset: {} from string: {}", patch.offset, *offset_str);
            } catch (const std::exception& e) {
                LOG_ERROR("PatchConfigLoader: Failed to parse offset string '{}': {}", *offset_str, e.what());
                return std::nullopt;
            }
        } else {
            LOG_ERROR("PatchConfigLoader: Offset field is not a string for instruction: {}", key_str);
            return std::nullopt;
        }
    } else {
        LOG_ERROR("PatchConfigLoader: Missing offset field for instruction: {}", key_str);
        return std::nullopt;
    }
    
    bool is_valid = patch.is_valid();
    LOG_DEBUG("PatchConfigLoader: Instruction validation result: {}", is_valid);
    
    return is_valid ? std::make_optional(patch) : std::nullopt;
}

std::vector<std::uint8_t> PatchConfigLoader::parse_bytes_string(const std::string& bytes_str) {
    LOG_TRACE("PatchConfigLoader: Parsing bytes string: {}", bytes_str);
    
    std::vector<std::uint8_t> bytes;
    std::regex byte_regex(R"([0-9A-Fa-f]{2}|XX)");
    
    auto matches = std::sregex_iterator(bytes_str.begin(), bytes_str.end(), byte_regex);
    auto end_matches = std::sregex_iterator{};
    
    int byte_count = 0;
    for (auto it = matches; it != end_matches; ++it) {
        const std::string byte_str = it->str();
        if (byte_str == "XX") {
            bytes.push_back(0xFF); // Placeholder
            LOG_TRACE("PatchConfigLoader: Added placeholder byte (XX)");
        } else {
            auto byte_value = static_cast<std::uint8_t>(std::stoul(byte_str, nullptr, 16));
            bytes.push_back(byte_value);
            LOG_TRACE("PatchConfigLoader: Added byte: 0x{:02X}", byte_value);
        }
        byte_count++;
    }
    
    if (bytes.empty()) {
        LOG_ERROR("PatchConfigLoader: No valid bytes found in string: {}", bytes_str);
        throw std::invalid_argument("No valid bytes found in string: " + bytes_str);
    }
    
    LOG_DEBUG("PatchConfigLoader: Successfully parsed {} bytes", byte_count);
    return bytes;
}

std::uintptr_t PatchConfigLoader::parse_address(const std::string& value) {
    LOG_TRACE("PatchConfigLoader: Parsing address string: {}", value);
    if (value.starts_with("0x") || value.starts_with("0X")) {
        return static_cast<std::uintptr_t>(std::stoull(value, nullptr, 16));
    }
    return static_cast<std::uintptr_t>(std::stoull(value));
}

std::int32_t PatchConfigLoader::parse_offset(const std::string& offset_str) {
    LOG_TRACE("PatchConfigLoader: Parsing offset string: {}", offset_str);
    
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