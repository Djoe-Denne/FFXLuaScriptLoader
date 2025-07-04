# Unit Tests for FFScriptLoader (x32)
cmake_minimum_required(VERSION 3.25)
project(FFScriptLoaderTests VERSION 1.0.0 LANGUAGES CXX)

# Force 32-bit architecture
set(CMAKE_GENERATOR_PLATFORM Win32)
set(CMAKE_VS_PLATFORM_NAME Win32)
set(CMAKE_SIZEOF_VOID_P 4)
set(VCPKG_TARGET_TRIPLET x32-windows CACHE STRING "")

# Verify 32-bit build
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Must build for x32 (32-bit) architecture!")
endif()

# C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC settings for x32
if(MSVC)
    add_compile_options(/arch:IA32 /W4)
    add_link_options(/MACHINE:X86)
    # Disable specific warnings for tests
    add_compile_options(/wd4996)  # Disable deprecated function warnings
endif()

# Download Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Test source files
set(TEST_SOURCES
    # Main test runner
    test_main.cpp
    
    # Core hook tests
    test_mod_context.cpp
    test_config_factory.cpp
    test_task_manager.cpp
    test_plugin_manager.cpp
    test_hook_task.cpp
    
    # Memory plugin tests
    test_memory_region.cpp
    test_copy_memory.cpp
    test_patch_memory.cpp
    test_memory_configs.cpp
    test_load_in_memory_config_loader.cpp
    test_load_in_memory_task.cpp
    test_load_in_memory_config.cpp
    
    # Mock implementations
    mock_plugin_host.cpp
)

# Memory plugin source files for testing
set(MEMORY_PLUGIN_SOURCES
    ${CMAKE_SOURCE_DIR}/memory_plugin/src/memory_config_loader.cpp
    ${CMAKE_SOURCE_DIR}/memory_plugin/src/patch_config_loader.cpp
    ${CMAKE_SOURCE_DIR}/memory_plugin/src/load_in_memory_config_loader.cpp
    ${CMAKE_SOURCE_DIR}/memory_plugin/src/patch_memory.cpp
    ${CMAKE_SOURCE_DIR}/memory_plugin/src/copy_memory.cpp
    ${CMAKE_SOURCE_DIR}/memory_plugin/src/load_in_memory.cpp
)

# Create test executable
add_executable(app_hook_tests ${TEST_SOURCES} ${MEMORY_PLUGIN_SOURCES})

# Include directories
target_include_directories(app_hook_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core_hook/include
    ${CMAKE_SOURCE_DIR}/memory_plugin/include
)

# Link libraries
target_link_libraries(app_hook_tests PRIVATE
    core_hook
    gtest
    gtest_main
    gmock
    gmock_main
    kernel32
    user32
)

# Definitions
target_compile_definitions(app_hook_tests PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    GTEST_HAS_STD_WSTRING=1
)

# Discover tests for CTest
include(GoogleTest)
gtest_discover_tests(app_hook_tests)

# Set output directory
set_target_properties(app_hook_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

message(STATUS "Unit tests configured for x32 architecture") 