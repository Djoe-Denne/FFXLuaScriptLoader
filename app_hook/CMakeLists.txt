# Application Hook Module (x32 DLL)
cmake_minimum_required(VERSION 3.25)
project(AppHook VERSION 1.0.0 LANGUAGES CXX)

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

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# MSVC settings for x32
if(MSVC)
    add_compile_options(/arch:IA32 /W4)
    add_link_options(/MACHINE:X86)
endif()

# Dependencies are now handled by core_hook

# Source files (app_hook specific only - shared functionality is in core_hook)
set(SOURCES 
    src/dllmain.cpp
)

# Create app_hook.dll
add_library(app_hook SHARED ${SOURCES})

set_target_properties(app_hook PROPERTIES
    OUTPUT_NAME "app_hook"
    PREFIX ""
    SUFFIX ".dll"
)

# Include directories (core_hook provides all headers)
# target_include_directories not needed - core_hook provides all includes

# Link libraries
target_link_libraries(app_hook PRIVATE 
    core_hook       # Our shared static library
    kernel32
    user32
)

# Definitions
target_compile_definitions(app_hook PRIVATE 
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Install
install(TARGETS app_hook RUNTIME DESTINATION bin) 