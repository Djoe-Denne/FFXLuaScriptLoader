# FF8 Script Loader - Main Build Configuration
# Builds both ff8_hook (DLL) and injector (EXE) as x32 (32-bit) versions

cmake_minimum_required(VERSION 3.25)
project(FFScriptLoader VERSION 1.0.0 LANGUAGES CXX)

# Force 32-bit architecture for all targets
set(CMAKE_GENERATOR_PLATFORM Win32)
set(CMAKE_VS_PLATFORM_NAME Win32)
set(CMAKE_SIZEOF_VOID_P 4)
set(VCPKG_TARGET_TRIPLET x32-windows CACHE STRING "")

# Verify 32-bit build
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Must build for x32 (32-bit) architecture!")
endif()

# Global C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable testing
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# MSVC settings for x32
if(MSVC)
    add_compile_options(/arch:IA32 /W4)
    add_link_options(/MACHINE:X86)
    message(STATUS "Using MSVC with x32 architecture")
endif()

# Global definitions
add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

message(STATUS "Building FFScriptLoader components as x32 (32-bit)")

# Add subdirectories
add_subdirectory(ff8_hook)
add_subdirectory(injector)

# Add tests if enabled
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Optional: Create install target
install(TARGETS ff8_hook ff8_injector
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

message(STATUS "Configuration complete. Both ff8_hook.dll and ff8_injector.exe will be built as x32.")

# Custom target to generate compile_commands.json using PowerShell script
add_custom_target(generate_compdb
    COMMAND powershell.exe -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/generate_compile_commands.ps1"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating compile_commands.json for IDE support"
    VERBATIM
)

message(STATUS "Added 'generate_compdb' target. Run 'cmake --build . --target generate_compdb' to generate compile_commands.json") 