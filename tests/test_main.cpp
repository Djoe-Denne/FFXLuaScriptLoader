#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <iostream>
#include <windows.h>

// Initialize logging for tests
#include "util/logger.hpp"

int main(int argc, char** argv) {
    // Initialize Google Test and Google Mock
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    
    // Initialize logging for tests
    app_hook::util::initialize_logging("test_log.txt", 1); // Debug level
    
    std::cout << "Running FFScriptLoader Unit Tests..." << std::endl;
    std::cout << "Target Architecture: x32 (32-bit)" << std::endl;
    std::cout << "Pointer size: " << sizeof(void*) << " bytes" << std::endl;
    
    // Run all tests
    int result = RUN_ALL_TESTS();
    
    // Cleanup logging
    app_hook::util::shutdown_logging();
    
    return result;
} 