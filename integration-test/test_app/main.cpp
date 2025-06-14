#include <Windows.h>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

int main() {
    std::cout << "Integration Test Application Starting...\n";
    std::cout << "Process ID: " << GetCurrentProcessId() << "\n";
    std::cout << "Waiting for termination signal...\n";
    
    // Create a named event for termination signal
    std::string eventName = "IntegrationTest_Terminate_" + std::to_string(GetCurrentProcessId());
    HANDLE hTerminateEvent = CreateEventA(NULL, TRUE, FALSE, eventName.c_str());
    
    if (!hTerminateEvent) {
        std::cerr << "Failed to create termination event: " << GetLastError() << "\n";
        return 1;
    }
    
    std::cout << "Created termination event: " << eventName << "\n";
    std::cout << "App is ready for injection. Will wait up to 10 seconds for termination signal.\n";
    
    // Wait for termination signal with timeout
    auto start_time = std::chrono::steady_clock::now();
    const auto timeout_duration = std::chrono::seconds(10);
    
    while (true) {
        // Check if termination event is signaled
        DWORD result = WaitForSingleObject(hTerminateEvent, 100); // Check every 100ms
        
        if (result == WAIT_OBJECT_0) {
            std::cout << "Termination signal received! Test PASSED - Plugin injection working!\n";
            CloseHandle(hTerminateEvent);
            return 0; // Success
        }
        
        // Check timeout
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= timeout_duration) {
            std::cout << "Timeout reached without termination signal. Test FAILED - Plugin injection not working!\n";
            CloseHandle(hTerminateEvent);
            return 1; // Failure
        }
        
        // Show we're still alive
        static int heartbeat = 0;
        if (++heartbeat % 10 == 0) { // Every second
            auto seconds_elapsed = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            std::cout << "Still waiting... (" << seconds_elapsed << "s elapsed)\n";
        }
    }
    
    return 1; // Should never reach here
} 