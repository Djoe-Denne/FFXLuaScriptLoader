#pragma once

#include "../task/hook_task.hpp"
#include <MinHook.h>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdint>
#include <expected>

namespace app_hook::hook {

// Forward declarations
void* get_or_create_hook_handler(std::uintptr_t address);
void* execute_hook_by_address(void* address);

/// @brief Error codes for hook operations
enum class HookError {
    success = 0,
    minhook_init_failed,
    minhook_create_failed,
    minhook_enable_failed,
    minhook_disable_failed,
    invalid_address,
    hook_already_exists,
    hook_not_found
};

/// @brief Hook operation result type
using HookResult = std::expected<void, HookError>;

/// @brief Represents a single hook point with multiple chained tasks
class Hook {
public:
    explicit Hook(std::uintptr_t address) : address_(address), trampoline_(nullptr) {}
    
    ~Hook() = default;
    
    // Non-copyable but movable
    Hook(const Hook&) = delete;
    Hook& operator=(const Hook&) = delete;
    Hook(Hook&&) = default;
    Hook& operator=(Hook&&) = default;
    
    /// @brief Add a task to this hook
    /// @param task Task to add
    void add_task(task::HookTaskPtr task) {
        if (task) {
            tasks_.push_back(std::move(task));
        }
    }
    
    /// @brief Execute all tasks chained to this hook
    void execute_tasks() {
        for (const auto& task : tasks_) {
            if (auto result = task->execute(); !result) {
                // Log error but continue with other tasks
                // Could add error handling/logging here
            }
        }
    }
    
    /// @brief Get the hook address
    /// @return Hook address
    [[nodiscard]] std::uintptr_t address() const noexcept { return address_; }
    
    /// @brief Get the trampoline pointer
    /// @return Trampoline pointer
    [[nodiscard]] void* trampoline() const noexcept { return trampoline_; }
    
    /// @brief Set the trampoline pointer
    /// @param trampoline Trampoline pointer from MinHook
    void set_trampoline(void* trampoline) noexcept { trampoline_ = trampoline; }
    
    /// @brief Set the handler pointer
    /// @param handler Handler pointer
    void set_handler(void* handler) noexcept { handler_ = handler; }
    
    /// @brief Get the handler pointer
    /// @return Handler pointer
    [[nodiscard]] void* handler() const noexcept { return handler_; }
    
    /// @brief Get the number of tasks
    /// @return Number of tasks
    [[nodiscard]] std::size_t task_count() const noexcept { return tasks_.size(); }
    
    /// @brief Check if hook has any tasks
    /// @return true if there are tasks
    [[nodiscard]] bool has_tasks() const noexcept { return !tasks_.empty(); }
    
private:
    std::uintptr_t address_;
    void* trampoline_;
    void* handler_;
    std::vector<task::HookTaskPtr> tasks_;
};

/// @brief Manages multiple hooks and their lifecycle
class HookManager {
public:
    HookManager() = default;
    ~HookManager() { uninstall_all(); }
    
    // Non-copyable, non-movable
    HookManager(const HookManager&) = delete;
    HookManager& operator=(const HookManager&) = delete;
    HookManager(HookManager&&) = delete;
    HookManager& operator=(HookManager&&) = delete;
    
    /// @brief Initialize MinHook library
    /// @return Result of initialization
    [[nodiscard]] HookResult initialize() {
        if (MH_Initialize() != MH_OK) {
            return std::unexpected(HookError::minhook_init_failed);
        }
        initialized_ = true;
        return {};
    }
    
    /// @brief Add a task to a hook at the specified address
    /// @param address Hook address
    /// @param task Task to add
    /// @return Result of operation
    [[nodiscard]] HookResult add_task_to_hook(std::uintptr_t address, task::HookTaskPtr task) {
        if (!task) {
            return {};
        }
        
        // Find or create hook
        auto it = hooks_.find(address);
        if (it == hooks_.end()) {
            // Create new hook
            auto hook = std::make_unique<Hook>(address);
            hook->add_task(std::move(task));
            hooks_[address] = std::move(hook);
        } else {
            // Add to existing hook
            it->second->add_task(std::move(task));
        }
        
        return {};
    }
    
    /// @brief Install all hooks
    /// @return Result of installation
    [[nodiscard]] HookResult install_all() {
        if (!initialized_) {
            if (auto result = initialize(); !result) {
                return result;
            }
        }
        
        for (auto& [address, hook] : hooks_) {
            if (hook->has_tasks()) {
                if (auto result = install_hook(*hook); !result) {
                    return result;
                }
            }
        }
        
        return {};
    }
    
    /// @brief Uninstall all hooks
    void uninstall_all() {
        for (auto& [address, hook] : hooks_) {
            MH_DisableHook(reinterpret_cast<LPVOID>(address));
        }
        
        if (initialized_) {
            MH_Uninitialize();
            initialized_ = false;
        }
        
        hooks_.clear();
    }
    
    /// @brief Get hook by address
    /// @param address Hook address
    /// @return Pointer to hook or nullptr if not found
    [[nodiscard]] Hook* get_hook(std::uintptr_t address) {
        auto it = hooks_.find(address);
        return (it != hooks_.end()) ? it->second.get() : nullptr;
    }
    
    /// @brief Get the number of hooks
    /// @return Number of hooks
    [[nodiscard]] std::size_t hook_count() const noexcept { return hooks_.size(); }
    
    /// @brief Get total number of tasks across all hooks
    /// @return Total task count
    [[nodiscard]] std::size_t total_task_count() const noexcept {
        std::size_t count = 0;
        for (const auto& [address, hook] : hooks_) {
            count += hook->task_count();
        }
        return count;
    }
    
    /// @brief Install a single hook
    /// @param hook Hook to install
    /// @return Result of installation
    [[nodiscard]] HookResult install_hook(Hook& hook);
    
private:
    /// @brief Create a hook handler function for the specified address
    /// @param address Hook address
    /// @return Function pointer to hook handler
    [[nodiscard]] void* create_hook_handler(std::uintptr_t address);
    
private:
    std::unordered_map<std::uintptr_t, std::unique_ptr<Hook>> hooks_;
    bool initialized_ = false;
};

} // namespace app_hook::hook
