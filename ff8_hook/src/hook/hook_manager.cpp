#include "../../include/hook/hook_manager.hpp"
#include "../../include/util/logger.hpp"
#include <unordered_map>
#include <functional>
#include <sstream>
#include <iomanip>

namespace ff8_hook::hook {

// Global map to associate addresses with hook instances
// This is needed because MinHook requires static function pointers
static std::unordered_map<std::uintptr_t, Hook*> g_hook_map;

// C++ function called by hook handlers
void* execute_hook_by_address(void* address) {
    std::uintptr_t addr_int = reinterpret_cast<std::uintptr_t>(address);
    LOG_INFO("Hook triggered at address: 0x{:X}", addr_int);

    if (auto it = g_hook_map.find(addr_int); it != g_hook_map.end()) {
        LOG_DEBUG("Found hook for address 0x{:X}, executing {} task(s)", addr_int, it->second->task_count());
        it->second->execute_tasks();
        LOG_DEBUG("Completed execution for hook at address 0x{:X}", addr_int);
        LOG_INFO("returning to 0x{:X}", reinterpret_cast<std::uintptr_t>(it->second->trampoline()));
        return it->second->trampoline();
    }
    LOG_WARNING("No hook found for triggered address: 0x{:X}", addr_int);
    LOG_DEBUG("Available hooks:");
    for (const auto& [hook_addr, hook_ptr] : g_hook_map) {
        LOG_DEBUG("  - Hook at address: 0x{:X}", hook_addr);
    }
    return nullptr;
}

// Remplacer 0xDEADBEEF par l'adresse réelle au moment de l'allocation
unsigned char hook_stub[] = {
    0x60,                               // pushad
    0x9C,                               // pushfd
    0xB8, 0, 0, 0, 0,                   // mov eax, address of newfunc
    0x50,                               // push eax
    0xE8, 0, 0, 0, 0,                   // call execute_hook_by_address (offset à patcher)
    0x83, 0xC4, 0x04,                   // add esp, 4
    0x9D,                               // popfd
    0x61,                               // popad
    0xFF, 0, 0, 0, 0                    // jmp to patch after trampoline creation
};

void* create_hook_instance() {
    size_t funcSize = sizeof(hook_stub);
    // Allouer de la mémoire exécutable
    void* newFunc = VirtualAlloc(nullptr, funcSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (newFunc) {
        // Copier le code de la fonction générique
        memcpy(newFunc, hook_stub, sizeof(hook_stub));
        
        // Patch the address parameter for mov eax instruction (offset 3)
        std::uintptr_t funcAddr = reinterpret_cast<std::uintptr_t>(newFunc);
        memcpy(reinterpret_cast<char*>(newFunc) + 3, &funcAddr, 4);
        
        // Patch the call offset for execute_hook_by_address (offset 9)
        std::uintptr_t callSite = reinterpret_cast<std::uintptr_t>(newFunc) + 8 + 5; // address after the call instruction
        std::uintptr_t targetAddr = reinterpret_cast<std::uintptr_t>(&execute_hook_by_address);
        std::int32_t callOffset = static_cast<std::int32_t>(targetAddr - callSite);
        memcpy(reinterpret_cast<char*>(newFunc) + 9, &callOffset, 4);
    }
    return newFunc;
}

void patch_hook(void* handler, void* trampoline) {
    if (!handler || !trampoline) {
        LOG_ERROR("Invalid handler or trampoline address for patching");
        return;
    }
    
    // Patch the jmp instruction at the end of the hook stub
    // The jmp instruction starts at offset 18 (0xFF, 0, 0, 0, 0)
    // Change it to: 0xE9 followed by relative offset to trampoline
    char* hookCode = reinterpret_cast<char*>(handler);
    
    // Calculate relative offset for direct jmp (0xE9)
    std::uintptr_t jmpSite = reinterpret_cast<std::uintptr_t>(handler) + 18 + 5; // address after jmp instruction
    std::uintptr_t trampolineAddr = reinterpret_cast<std::uintptr_t>(trampoline);
    std::int32_t jmpOffset = static_cast<std::int32_t>(trampolineAddr - jmpSite);
    
    // Patch: 0xE9 (direct jmp) + 4-byte relative offset
    hookCode[18] = 0xE9;
    memcpy(hookCode + 19, &jmpOffset, 4);
    
    LOG_DEBUG("Patched hook at 0x{:X} to jump to trampoline at 0x{:X}", 
              reinterpret_cast<std::uintptr_t>(handler), trampolineAddr);
}


// Hook handler registry
static std::unordered_map<std::uintptr_t, void*> g_handler_registry;

// Generate hook handlers for any address from configuration
void* get_or_create_hook_handler(std::uintptr_t address) {
    auto it = g_handler_registry.find(address);
    if (it != g_handler_registry.end()) {
        return it->second;
    }
    
    // Use the generic handler for all addresses
    // The handler will determine the actual hook address at runtime
    auto handler = create_hook_instance();
    g_handler_registry[address] = handler;
    return handler;
}

void* HookManager::create_hook_handler(std::uintptr_t address) {
    return get_or_create_hook_handler(address);
}

HookResult HookManager::install_hook(Hook& hook) {
    const auto address = hook.address();
    const auto address_ptr = reinterpret_cast<LPVOID>(address);
    void* trampoline = nullptr;
    
    
    // Get or create hook handler
    auto handler = create_hook_handler(address);
    if (!handler) {
        return std::unexpected(HookError::minhook_create_failed);
    }
    // Register hook in global map
    g_hook_map[reinterpret_cast<std::uintptr_t>(handler)] = &hook;

    hook.set_handler(reinterpret_cast<void*>(handler));
    
    if (MH_CreateHook(address_ptr, handler, &trampoline) != MH_OK) {
        g_hook_map.erase(reinterpret_cast<std::uintptr_t>(handler));
        return std::unexpected(HookError::minhook_create_failed);
    }

    LOG_INFO("Trampoline: 0x{:X}", reinterpret_cast<std::uintptr_t>(trampoline));
    
    hook.set_trampoline(trampoline);
    
    // Patch the hook handler to jump to the trampoline
    patch_hook(handler, trampoline);
    
    if (MH_EnableHook(address_ptr) != MH_OK) {
        g_hook_map.erase(reinterpret_cast<std::uintptr_t>(handler));
        return std::unexpected(HookError::minhook_enable_failed);
    }
    
    return {};
}

} // namespace ff8_hook::hook 