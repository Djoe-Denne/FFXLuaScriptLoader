#include "../../include/config/patch_loader.hpp"

namespace ff8_hook::config {

ConfigResult<std::vector<InstructionPatch>> 
PatchLoader::load_patch_instructions(const std::string& file_path) {
    LOG_INFO("Loading patch instructions from: {}", file_path);
    
    std::vector<InstructionPatch> patches;
    
    try {
        // Parse TOML file
        auto toml = toml::parse_file(file_path);
        LOG_INFO("Successfully parsed TOML file: {}", file_path);
        
        // Look for instructions table
        auto instructions = toml["instructions"];
        if (!instructions) {
            LOG_WARNING("No 'instructions' table found in TOML file: {}", file_path);
            return patches; // Return empty vector
        }
        
        if (!instructions.is_table()) {
            LOG_ERROR("'instructions' is not a table in TOML file: {}", file_path);
            return std::unexpected(ConfigError::invalid_format);
        }
        
        // Process instruction entries
        auto instructions_table = instructions.as_table();
        
        for (const auto& [key, value] : *instructions_table) {
            auto patch_opt = parse_single_instruction(std::string{key.str()}, value);
            if (patch_opt && patch_opt->is_valid()) {
                patches.push_back(std::move(*patch_opt));
                LOG_DEBUG("Added valid patch for instruction '{}'", key.str());
            } else {
                LOG_WARNING("Invalid patch for instruction '{}' - skipping", key.str());
            }
        }
        
    } catch (const toml::parse_error& e) {
        LOG_ERROR("TOML parse error in file '{}': {} at line {}, column {}", 
                 file_path, e.description(), e.source().begin.line, e.source().begin.column);
        return std::unexpected(ConfigError::parse_error);
    } catch (const std::exception& e) {
        LOG_ERROR("Exception while loading patch file '{}': {}", file_path, e.what());
        return std::unexpected(ConfigError::parse_error);
    }
    
    LOG_INFO("Loaded {} patch instruction(s) from file: {}", patches.size(), file_path);
    
    // Log summary of loaded patches
    for (size_t i = 0; i < patches.size(); ++i) {
        LOG_DEBUG("Patch {}: address=0x{:X}, offset={}, bytes={}", 
                 i, patches[i].address, patches[i].offset, patches[i].bytes.size());
    }
    
    return patches;
}

std::optional<InstructionPatch> 
PatchLoader::parse_single_instruction(const std::string& key_str, const toml::node& value) {
    LOG_DEBUG("Processing instruction key: '{}'", key_str);
    
    if (!value.is_table()) {
        LOG_WARNING("Instruction '{}' is not a table, skipping", key_str);
        return std::nullopt;
    }
    
    InstructionPatch patch;
    
    // Parse address from key (format: 0x1234ABCD)
    try {
        patch.address = ConfigParsingUtils::parse_address(key_str);
        LOG_DEBUG("Parsed address from key '{}': 0x{:X}", key_str, patch.address);
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to parse address from key '{}': {}", key_str, e.what());
        return std::nullopt;
    }
    
    auto table = value.as_table();
    
    // Parse bytes field
    if (auto bytes_node = table->get("bytes")) {
        if (auto bytes_str = bytes_node->value<std::string>()) {
            try {
                patch.bytes = parse_bytes_string(*bytes_str);
                LOG_INFO("Parsed bytes: '{}' -> {} byte(s)", *bytes_str, patch.bytes.size());
            } catch (const std::exception& e) {
                LOG_WARNING("Failed to parse bytes '{}' for instruction '{}': {}", 
                           *bytes_str, key_str, e.what());
                return std::nullopt;
            }
        } else {
            LOG_WARNING("'bytes' field is not a string for instruction '{}'", key_str);
            return std::nullopt;
        }
    } else {
        LOG_WARNING("Missing 'bytes' field for instruction '{}'", key_str);
        return std::nullopt;
    }
    
    // Parse offset field
    if (auto offset_node = table->get("offset")) {
        if (auto offset_str = offset_node->value<std::string>()) {
            try {
                patch.offset = ConfigParsingUtils::parse_offset(*offset_str);
                LOG_DEBUG("Parsed offset: '{}' -> {}", *offset_str, patch.offset);
            } catch (const std::exception& e) {
                LOG_WARNING("Failed to parse offset '{}' for instruction '{}': {}", 
                           *offset_str, key_str, e.what());
                return std::nullopt;
            }
        } else {
            LOG_WARNING("'offset' field is not a string for instruction '{}'", key_str);
            return std::nullopt;
        }
    } else {
        LOG_WARNING("Missing 'offset' field for instruction '{}'", key_str);
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

} // namespace ff8_hook::config 