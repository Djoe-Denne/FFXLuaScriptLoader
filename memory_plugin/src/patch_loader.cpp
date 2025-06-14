#include "config/patch_loader.hpp"

namespace app_hook::config {

ConfigResult<std::vector<InstructionPatch>> 
PatchLoader::load_patch_instructions(const std::string& file_path) {
    std::vector<InstructionPatch> patches;
    
    try {
        // Parse TOML file
        auto toml = toml::parse_file(file_path);
        
        // Look for instructions table
        auto instructions = toml["instructions"];
        if (!instructions) {
            return patches; // Return empty vector
        }
        
        if (!instructions.is_table()) {
            return std::unexpected(ConfigError::invalid_format);
        }
        
        // Process instruction entries
        auto instructions_table = instructions.as_table();
        
        for (const auto& [key, value] : *instructions_table) {
            auto patch_opt = parse_single_instruction(std::string{key.str()}, value);
            if (patch_opt && patch_opt->is_valid()) {
                patches.push_back(std::move(*patch_opt));
            }
        }
        
    } catch (const toml::parse_error& e) {
        return std::unexpected(ConfigError::parse_error);
    } catch (const std::exception& e) {
        return std::unexpected(ConfigError::parse_error);
    }
    
    return patches;
}

std::optional<InstructionPatch> 
PatchLoader::parse_single_instruction(const std::string& key_str, const toml::node& value) {
    if (!value.is_table()) {
        return std::nullopt;
    }
    
    InstructionPatch patch;
    
    // Parse address from key (format: 0x1234ABCD)
    try {
        patch.address = ConfigParsingUtils::parse_address(key_str);
    } catch (const std::exception& e) {
        return std::nullopt;
    }
    
    auto table = value.as_table();
    
    // Parse bytes field
    if (auto bytes_node = table->get("bytes")) {
        if (auto bytes_str = bytes_node->value<std::string>()) {
            try {
                patch.bytes = parse_bytes_string(*bytes_str);
            } catch (const std::exception& e) {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }
    
    // Parse offset field
    if (auto offset_node = table->get("offset")) {
        if (auto offset_str = offset_node->value<std::string>()) {
            try {
                patch.offset = ConfigParsingUtils::parse_offset(*offset_str);
            } catch (const std::exception& e) {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }
    
    return patch;
}

std::vector<std::uint8_t> PatchLoader::parse_bytes_string(const std::string& bytes_str) {
    std::vector<std::uint8_t> bytes;
    std::regex byte_regex(R"([0-9A-Fa-f]{2}|XX)");
    
    // Use ranges to process matches (C++23 style)
    auto matches = std::sregex_iterator(bytes_str.begin(), bytes_str.end(), byte_regex);
    auto end_matches = std::sregex_iterator{};
    
    for (auto it = matches; it != end_matches; ++it) {
        const std::string byte_str = it->str();
        if (byte_str == "XX") {
            bytes.push_back(0xFF); // Placeholder
        } else {
            bytes.push_back(static_cast<std::uint8_t>(std::stoul(byte_str, nullptr, 16)));
        }
    }
    
    if (bytes.empty()) {
        throw std::invalid_argument("No valid bytes found in string: " + bytes_str);
    }
    
    return bytes;
}

} // namespace app_hook::config 