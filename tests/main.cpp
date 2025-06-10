/**
 * @file main.cpp
 * @brief Main entry point for FF8Hook unit tests
 * 
 * This file initializes Google Test and runs all the test suites
 * for the FF8Hook project following C++23 conventions.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <iostream>

/**
 * @brief Main entry point for the test executable
 * @param argc Number of command line arguments
 * @param argv Command line arguments
 * @return Exit code (0 for success)
 */
int main(int argc, char** argv) {
    std::cout << "FF8Hook Unit Tests - C++23 Edition\n";
    std::cout << "===================================\n";
    
    // Initialize Google Test and Google Mock
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    
    // Configure test behavior
    ::testing::FLAGS_gtest_catch_exceptions = true;
    ::testing::FLAGS_gtest_print_time = true;
    
    // Run all tests
    const int result = RUN_ALL_TESTS();
    
    std::cout << "\nTest execution completed.\n";
    return result;
} 