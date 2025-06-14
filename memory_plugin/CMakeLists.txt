# Memory Operations Plugin (x32 DLL)
cmake_minimum_required(VERSION 3.25)
project(MemoryPlugin VERSION 1.0.0 LANGUAGES CXX)

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

# Output directories - put tasks in bin/<mode>/tasks/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# MSVC settings for x32
if(MSVC)
    add_compile_options(/arch:IA32 /W4)
    add_link_options(/MACHINE:X86)
endif()

# Dependencies are now handled by core_hook

# Source files
set(SOURCES 
    src/memory_plugin.cpp
    src/memory_config_loader.cpp
)

# Create memory_plugin.dll
add_library(memory_plugin SHARED ${SOURCES})

set_target_properties(memory_plugin PROPERTIES
    OUTPUT_NAME "memory_plugin"
    PREFIX ""
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug/tasks"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release/tasks"
)

# Include directories
target_include_directories(memory_plugin PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(memory_plugin PRIVATE 
    core_hook       # Our shared static library
    kernel32
    user32
)

# Definitions
target_compile_definitions(memory_plugin PRIVATE 
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    MEMORY_PLUGIN_EXPORTS
)

# Install
install(TARGETS memory_plugin 
    RUNTIME DESTINATION bin/tasks
    COMPONENT TaskPlugins
) 