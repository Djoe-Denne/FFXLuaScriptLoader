# Core Hook Static Library (x32)
cmake_minimum_required(VERSION 3.25)
project(CoreHook VERSION 1.0.0 LANGUAGES CXX)

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
endif()

# Download dependencies
include(FetchContent)

FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        master
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

# Source files
set(SOURCES 
    src/config/config_factory.cpp
    src/config/config_loader.cpp  
    src/config/task_loader.cpp
    src/context/mod_context.cpp
    src/hook/hook_factory.cpp
    src/hook/hook_manager.cpp
    src/plugin/plugin_manager.cpp
    src/task/task_factory.cpp
    src/util/logger.cpp
    src/util/task_manager.cpp
)

# Create core_hook static library
add_library(core_hook STATIC ${SOURCES})

# Include directories
target_include_directories(core_hook PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(core_hook PUBLIC
    minhook
    spdlog::spdlog
    tomlplusplus::tomlplusplus
    kernel32
    user32
)

# Definitions
target_compile_definitions(core_hook PUBLIC
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Export targets for use by other projects
set_target_properties(core_hook PROPERTIES
    EXPORT_NAME CoreHook
    OUTPUT_NAME "core_hook"
)

# Install
install(TARGETS core_hook
    EXPORT CoreHookTargets
    ARCHIVE DESTINATION lib
    COMPONENT Development
)

install(DIRECTORY include/
    DESTINATION include
    COMPONENT Development
) 