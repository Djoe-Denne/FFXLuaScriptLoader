cmake_minimum_required(VERSION 3.22)

# Integration test components
project(integration_test)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include parent directories for accessing core_hook
include_directories(../core_hook/include)

# Test application that waits for termination signal
add_executable(test_app
    test_app/main.cpp
)

# Test plugin that sends termination signal
add_library(test_plugin SHARED
    test_plugin/src/test_plugin.cpp
)

target_link_libraries(test_plugin PRIVATE 
    core_hook
)

# Set output directories to put everything in the integration-test folder
set_target_properties(test_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/integration-test"
)

# Copy test_app.exe to the integration test directory (from Release subdir)
add_custom_command(TARGET test_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:test_app>"
        "${CMAKE_BINARY_DIR}/integration-test/"
    COMMENT "Copying test_app.exe to integration test directory"
)

set_target_properties(test_plugin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug/tasks"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release/tasks"
    OUTPUT_NAME "test_plugin"
)

# Copy required files after build
add_custom_command(TARGET test_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:app_hook>"
        "${CMAKE_BINARY_DIR}/integration-test/"
    COMMENT "Copying app_hook.dll to integration test directory"
)

add_custom_command(TARGET test_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:app_injector>"
        "${CMAKE_BINARY_DIR}/integration-test/"
    COMMENT "Copying app_injector.exe to integration test directory"
)

# Create tasks directory for integration test
add_custom_command(TARGET test_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/integration-test/tasks"
    COMMENT "Creating integration test tasks directory"
)

add_custom_command(TARGET test_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:memory_plugin>"
        "${CMAKE_BINARY_DIR}/integration-test/tasks/"
    COMMENT "Copying memory_plugin.dll to integration test tasks directory"
)

add_custom_command(TARGET test_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:test_plugin>"
        "${CMAKE_BINARY_DIR}/integration-test/tasks/"
    COMMENT "Copying test_plugin.dll to integration test tasks directory"
)

# Copy test configuration
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/test_config.toml" 
    "${CMAKE_BINARY_DIR}/integration-test/config/test_config.toml" 
    COPYONLY
)

# Copy the test script
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/run_test.bat" 
    "${CMAKE_BINARY_DIR}/integration-test/run_test.bat" 
    COPYONLY
)

# Add dependencies to ensure correct build order
add_dependencies(test_app app_hook app_injector)
add_dependencies(test_plugin memory_plugin) 