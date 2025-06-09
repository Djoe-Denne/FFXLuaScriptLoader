# FF8 Hook Module (x32 DLL)
cmake_minimum_required(VERSION 3.25)
project(FF8Hook VERSION 1.0.0 LANGUAGES CXX)

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

# Fetch MinHook from GitHub
include(FetchContent)

FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        master
)

# Download and build spdlog  
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.12.0
)

# Download toml++
FetchContent_Declare(
  tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG v3.4.0
)

# Configure MinHook build options
set(MINHOOK_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(minhook spdlog tomlplusplus)

# Source files
set(SOURCES 
    src/dllmain.cpp
    src/context/mod_context.cpp
    src/memory/copy_memory.cpp
    src/config/config_loader.cpp
    src/hook/hook_manager.cpp
)

# Create ff8_hook.dll
add_library(ff8_hook SHARED ${SOURCES})

set_target_properties(ff8_hook PROPERTIES
    OUTPUT_NAME "ff8_hook"
    PREFIX ""
    SUFFIX ".dll"
)

# Include directories
target_include_directories(ff8_hook PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(ff8_hook PRIVATE 
    minhook
    spdlog::spdlog
    tomlplusplus::tomlplusplus
    kernel32
    user32
)

# Definitions
target_compile_definitions(ff8_hook PRIVATE 
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Install
install(TARGETS ff8_hook RUNTIME DESTINATION bin) 