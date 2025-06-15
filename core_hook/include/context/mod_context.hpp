#pragma once

#include "util/logger.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <any>
#include <typeinfo>
#include <type_traits>
#include <vector>

namespace app_hook::context {

/// @brief Thread-safe generic context for mod operations
/// @note Stores any type of data using std::any for maximum flexibility
/// @note For move-only types, data is wrapped in shared_ptr automatically
class ModContext {
public:
    ModContext() = default;
    ~ModContext() = default;
    
    // Non-copyable, non-movable singleton-like
    ModContext(const ModContext&) = delete;
    ModContext& operator=(const ModContext&) = delete;
    ModContext(ModContext&&) = delete;
    ModContext& operator=(ModContext&&) = delete;
    
    /// @brief Store data with the given key
    /// @tparam T Type of data to store
    /// @param key Data key (e.g., "ff8.magic.k_magic_data")
    /// @param data Data to store
    template<typename T>
    void store_data(const std::string& key, T&& data) {
        std::lock_guard lock{mutex_};
        
        // For move-only types, wrap in shared_ptr
        if constexpr (!std::is_copy_constructible_v<std::decay_t<T>>) {
            LOG_DEBUG("Storing move-only type for key: {}", key);
            data_[key] = std::make_shared<std::decay_t<T>>(std::forward<T>(data));
        } else {
            LOG_DEBUG("Storing copyable type for key: {}", key);
            data_[key] = std::forward<T>(data);
        }
    }
    
    /// @brief Get data by key
    /// @tparam T Expected type of the data
    /// @param key Data key
    /// @return Pointer to data of type T, or nullptr if not found or wrong type
    template<typename T>
    [[nodiscard]] T* get_data(const std::string& key) {
        std::lock_guard lock{mutex_};
        if (auto it = data_.find(key); it != data_.end()) {
            try {
                // Try direct access first
                if (auto* direct = std::any_cast<T>(&it->second)) {
                    return direct;
                }
                // Try shared_ptr access for move-only types
                if (auto* shared = std::any_cast<std::shared_ptr<T>>(&it->second)) {
                    return shared->get();
                }
            } catch (const std::bad_any_cast&) {
                // Type mismatch
                LOG_ERROR("Type mismatch for key: {}", key);
            }
        }
        return nullptr;
    }
    
    /// @brief Get const data by key
    /// @tparam T Expected type of the data
    /// @param key Data key
    /// @return Const pointer to data of type T, or nullptr if not found or wrong type
    template<typename T>
    [[nodiscard]] const T* get_data(const std::string& key) const {
        std::lock_guard lock{mutex_};
        if (auto it = data_.find(key); it != data_.end()) {
            try {
                // Try direct access first
                if (const auto* direct = std::any_cast<T>(&it->second)) {
                    return direct;
                }
                // Try shared_ptr access for move-only types
                if (const auto* shared = std::any_cast<std::shared_ptr<T>>(&it->second)) {
                    return shared->get();
                }
            } catch (const std::bad_any_cast&) {
                // Type mismatch
            }
        }
        return nullptr;
    }
    
    /// @brief Check if data exists with the given key
    /// @param key Data key
    /// @return true if data exists
    [[nodiscard]] bool has_data(const std::string& key) const {
        std::lock_guard lock{mutex_};
        return data_.contains(key);
    }
    
    /// @brief Get the type info of stored data
    /// @param key Data key
    /// @return Type info of the stored data, or nullptr if key not found
    [[nodiscard]] const std::type_info* get_data_type(const std::string& key) const {
        std::lock_guard lock{mutex_};
        if (auto it = data_.find(key); it != data_.end()) {
            return &it->second.type();
        }
        return nullptr;
    }
    
    /// @brief Remove data with the given key
    /// @param key Data key
    /// @return true if data was removed
    [[nodiscard]] bool remove_data(const std::string& key) {
        std::lock_guard lock{mutex_};
        return data_.erase(key) > 0;
    }
    
    /// @brief Get all keys in the context
    /// @return Vector of all stored keys
    [[nodiscard]] std::vector<std::string> get_all_keys() const {
        std::lock_guard lock{mutex_};
        std::vector<std::string> keys;
        keys.reserve(data_.size());
        for (const auto& [key, _] : data_) {
            keys.push_back(key);
        }
        return keys;
    }
    
    /// @brief Get the global instance
    [[nodiscard]] static ModContext& instance() {
        static ModContext instance;
        LOG_DEBUG("ModContext instance : 0x{:X}", reinterpret_cast<uintptr_t>(&instance));
        return instance;
    }
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::any> data_;
};

} // namespace app_hook::context
