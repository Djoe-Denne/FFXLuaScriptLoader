// ff8_function_hook_logger.cpp using Microsoft Detours
// Very basic FF8 function hook logger with Detours and ASLR-aware offset calculation

#include <windows.h>
#include <fstream>
#include <iostream>
#include <detours/detours.h> // Requires Detours library from Microsoft

// Static offset from IDA (must match function entry point)
#define FUNC_OFFSET 0x0052D400  // RVA of smPcReadFileReadAll

// Type of the original function
using OriginalFunc = unsigned int(__cdecl*)(const char* filename, void* dstBuf);
OriginalFunc realFunc = nullptr;

// Hooked version
unsigned int __cdecl hookedFunc(const char* filename, void* dstBuf) {
    MessageBoxA(NULL, filename ? filename : "<null>", "hookedFunc called", MB_OK);
    return realFunc(filename, dstBuf);
}

DWORD WINAPI MainThread(LPVOID) {
    std::ofstream debug("hook_debug.txt", std::ios::app);
    debug << "=== DLL MainThread started ===\n";

    BYTE* targetAddr = (BYTE*)FUNC_OFFSET;
    debug << "Target addr: 0x" << std::hex << (uintptr_t)targetAddr << "\n";

    // Prepare detour transaction
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    realFunc = (OriginalFunc)targetAddr;
    DetourAttach((void**)&realFunc, hookedFunc);

    if (DetourTransactionCommit() == NO_ERROR) {
        debug << "Detour attach succeeded\n";
    } else {
        debug << "Detour attach failed\n";
    }

    debug.close();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (realFunc) {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourDetach((void**)&realFunc, hookedFunc);
            DetourTransactionCommit();
        }
    }
    return TRUE;
}
