# Application Hook Unit Tests
cmake_minimum_required(VERSION 3.25)
project(AppHook_Tests VERSION 1.0.0 LANGUAGES CXX)

# Force 32-bit architecture for tests too
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
endif()

# Fetch Google Test from GitHub
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# Configure Google Test options
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # Use shared runtime for MSVC
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Include Google Test's cmake utilities
include(GoogleTest)

# Fetch dependencies needed for tests (same as main project)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG master
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)

set(MINHOOK_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(minhook spdlog tomlplusplus)

# Test source files
set(TEST_SOURCES
    test_config_base.cpp
    test_logger.cpp
    test_hook_task.cpp
    test_mod_context.cpp
    test_copy_memory.cpp
    test_config_factory.cpp
    main.cpp
)

# Create test executable
add_executable(app_hook_tests ${TEST_SOURCES})

# Include directories
target_include_directories(app_hook_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../app_hook/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(app_hook_tests PRIVATE
    gtest
    gtest_main
    gmock
    gmock_main
    spdlog::spdlog
    tomlplusplus::tomlplusplus
    minhook
    kernel32
    user32
)

# Compile definitions
target_compile_definitions(app_hook_tests PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    GTEST_HAS_STD_WSTRING=1
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
    _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
)

# Add tests to CTest
gtest_discover_tests(app_hook_tests)

# Copy test data files if any exist
# file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

message(STATUS "Application Hook tests configured successfully") 