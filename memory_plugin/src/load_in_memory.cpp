#include "../include/memory/load_in_memory.hpp"
#include "../include/memory/memory_region.hpp"
#include "../../core_hook/include/task/hook_task.hpp"
#include "../../core_hook/include/context/mod_context.hpp"
#include "plugin/plugin_interface.hpp"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstring>

namespace app_hook::memory {

task::TaskResult LoadInMemoryTask::execute() {
    PLUGIN_LOG_DEBUG("Executing LoadInMemoryTask for key '{}'", config_.key());
    PLUGIN_LOG_DEBUG("Loading binary from file: {}", config_.binary_path());
    PLUGIN_LOG_DEBUG("Offset security: 0x{:X}", config_.offset_security());
    
    if (!config_.is_valid()) {
        PLUGIN_LOG_ERROR("Invalid configuration for LoadInMemoryTask '{}'", config_.key());
        return std::unexpected(task::TaskError::invalid_config);
    }
    
    try {
        // Check if binary file exists
        if (!std::filesystem::exists(config_.binary_path())) {
            PLUGIN_LOG_ERROR("Binary file not found: {}", config_.binary_path());
            return std::unexpected(task::TaskError::file_not_found);
        }
        
        // Get file size
        const auto file_size = std::filesystem::file_size(config_.binary_path());
        PLUGIN_LOG_DEBUG("Binary file size: {} bytes", file_size);
        
        if (file_size == 0) {
            PLUGIN_LOG_WARN("Binary file is empty: {}", config_.binary_path());
            return std::unexpected(task::TaskError::file_read_error);
        }
        
        // Determine base address for loading
        std::uintptr_t base_address = 0;
        std::uintptr_t injection_address = 0;
        std::size_t total_size = static_cast<std::size_t>(file_size) + config_.offset_security();
        
        if (config_.reads_from_context()) {
            // Read base address from context
            const std::string context_key = config_.read_from_context();
            PLUGIN_LOG_DEBUG("Reading base address from context key: '{}'", context_key);
            
            auto memory_region = host_ ? 
                host_->get_mod_context().get_data<MemoryRegion>(context_key) :
                app_hook::context::ModContext::instance().get_data<MemoryRegion>(context_key);
                
            if (!memory_region) {
                PLUGIN_LOG_ERROR("Memory region '{}' not found in context for LoadInMemoryTask", context_key);
                return std::unexpected(task::TaskError::invalid_address);
            }
            
            // Log detailed memory region information for Cheat Engine inspection
            const std::uintptr_t region_base = reinterpret_cast<std::uintptr_t>(memory_region->data.get());
            injection_address = region_base + memory_region->original_size + config_.offset_security();
            base_address = injection_address;
            
            PLUGIN_LOG_INFO("=== MEMORY INJECTION DETAILS ===");
            PLUGIN_LOG_INFO("Context Memory Region: '{}'", context_key);
            PLUGIN_LOG_INFO("  Original Address: 0x{:016X}", memory_region->original_address);
            PLUGIN_LOG_INFO("  Allocated Base:   0x{:016X}", region_base);
            PLUGIN_LOG_INFO("  Region Size:      {} bytes (0x{:X})", memory_region->size, memory_region->size);
            PLUGIN_LOG_INFO("  Original Size:    {} bytes (0x{:X})", memory_region->original_size, memory_region->original_size);
            PLUGIN_LOG_INFO("Binary Injection Details:");
            PLUGIN_LOG_INFO("  File:             {}", config_.binary_path());
            PLUGIN_LOG_INFO("  File Size:        {} bytes (0x{:X})", file_size, file_size);
            PLUGIN_LOG_INFO("  Offset Security:  {} bytes (0x{:X})", config_.offset_security(), config_.offset_security());
            PLUGIN_LOG_INFO("  Injection Address: 0x{:016X} [CHEAT ENGINE TARGET]", injection_address);
            PLUGIN_LOG_INFO("  End Address:      0x{:016X}", injection_address + file_size);
            PLUGIN_LOG_INFO("================================");
            
            PLUGIN_LOG_DEBUG("Using context-based address: 0x{:X} (region size: {}, offset: 0x{:X})", 
                           base_address, memory_region->size, config_.offset_security());
        } else {
            PLUGIN_LOG_ERROR("LoadInMemoryTask '{}' requires readFromContext to be set", config_.key());
            return std::unexpected(task::TaskError::invalid_config);
        }
        
        // Create memory region for the loaded data
        MemoryRegion region{
            total_size,
            static_cast<std::size_t>(file_size),
            base_address,
            config_.description().empty() ? 
                ("Loaded binary data from " + config_.binary_path()) : 
                config_.description()
        };
        
        // Read binary file into memory
        std::ifstream file(config_.binary_path(), std::ios::binary);
        if (!file) {
            PLUGIN_LOG_ERROR("Failed to open binary file: {}", config_.binary_path());
            return std::unexpected(task::TaskError::file_not_found);
        }
        
        file.read(reinterpret_cast<char*>(region.data.get()), static_cast<std::streamsize>(file_size));
        if (!file) {
            PLUGIN_LOG_ERROR("Failed to read binary file: {}", config_.binary_path());
            return std::unexpected(task::TaskError::file_read_error);
        }
        
        PLUGIN_LOG_DEBUG("Successfully loaded {} bytes from binary file", file_size);
        
        // CRITICAL: Actually inject the data into the target memory address!
        PLUGIN_LOG_INFO("Injecting {} bytes to target address 0x{:016X}", file_size, injection_address);
        
        // Copy the loaded data to the injection address
        std::memcpy(reinterpret_cast<void*>(injection_address), region.data.get(), static_cast<std::size_t>(file_size));
        
        PLUGIN_LOG_INFO("Successfully injected {} bytes into memory at 0x{:016X}", file_size, injection_address);
        
        // Verify injection by reading back from the target address
        if (file_size > 0) {
            const auto preview_size = std::min(static_cast<std::size_t>(16), static_cast<std::size_t>(file_size));
            std::string hex_preview;
            
            // Read from the actual injected memory location for verification
            const std::uint8_t* injected_data = reinterpret_cast<const std::uint8_t*>(injection_address);
            for (std::size_t i = 0; i < preview_size; ++i) {
                char hex_byte[8];
                std::snprintf(hex_byte, sizeof(hex_byte), "%02X ", injected_data[i]);
                hex_preview += hex_byte;
            }
            PLUGIN_LOG_INFO("INJECTION VERIFIED: First {} bytes at 0x{:016X}: {}", preview_size, injection_address, hex_preview);
            
            // Additional Cheat Engine inspection info
            const std::size_t pattern_length = std::min(hex_preview.size(), static_cast<std::size_t>(23));
            PLUGIN_LOG_INFO("CHEAT ENGINE: Search for pattern '{}' at address 0x{:016X}", 
                          hex_preview.substr(0, pattern_length), injection_address);
        }
        
        // Store in context if configured
        if (config_.writes_to_context()) {
            const std::string context_key = config_.write_in_context().name;
            if (context_key.empty()) {
                PLUGIN_LOG_ERROR("LoadInMemoryTask '{}' has empty context key name", config_.key());
                return std::unexpected(task::TaskError::invalid_config);
            }
            
            PLUGIN_LOG_DEBUG("Storing loaded data in context with key: '{}'", context_key);
            if (host_) {
                host_->get_mod_context().store_data(context_key, std::move(region));
            } else {
                // Fallback to singleton for backward compatibility
                PLUGIN_LOG_WARN("Using singleton ModContext for backward compatibility");
                app_hook::context::ModContext::instance().store_data(context_key, std::move(region));
            }
        }
        
        PLUGIN_LOG_INFO("Successfully executed LoadInMemoryTask for key '{}' - Data injected at 0x{:016X}", 
                       config_.key(), injection_address);
        return {};
        
    } catch (const std::filesystem::filesystem_error& e) {
        PLUGIN_LOG_ERROR("Filesystem error in LoadInMemoryTask '{}': {}", config_.key(), e.what());
        return std::unexpected(task::TaskError::file_not_found);
    } catch (const std::bad_alloc&) {
        PLUGIN_LOG_ERROR("Memory allocation failed for LoadInMemoryTask '{}'", config_.key());
        return std::unexpected(task::TaskError::memory_allocation_failed);
    } catch (const std::exception& e) {
        PLUGIN_LOG_ERROR("Exception in LoadInMemoryTask '{}': {}", config_.key(), e.what());
        return std::unexpected(task::TaskError::unknown_error);
    } catch (...) {
        PLUGIN_LOG_ERROR("Unknown exception in LoadInMemoryTask '{}'", config_.key());
        return std::unexpected(task::TaskError::unknown_error);
    }
}

} // namespace app_hook::memory 