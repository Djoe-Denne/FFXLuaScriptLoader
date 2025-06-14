# Injector Module (EXE)
# Responsible for DLL injection into target processes

add_executable(app_injector
    src/injector.cpp
)

# Injector only needs basic Windows libraries (no external dependencies)
target_link_libraries(app_injector PRIVATE 
    kernel32
    user32
)

# Include directories
target_include_directories(app_injector PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Set properties
set_target_properties(app_injector PROPERTIES
    OUTPUT_NAME "app_injector"
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(app_injector PRIVATE 
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    
    # Set subsystem for console application
    set_target_properties(app_injector PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
endif()

# Debug/Release configurations
target_compile_definitions(app_injector PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# Compiler-specific options
if(MSVC)
    target_compile_options(app_injector PRIVATE
        /W4        # High warning level
        /permissive-  # Disable non-standard extensions
        $<$<CONFIG:Release>:/O2>  # Optimize for speed in release
    )
endif()

message(STATUS "Configured Application Injector (EXE)")

# Install
install(TARGETS app_injector
    RUNTIME DESTINATION bin
    COMPONENT Runtime
) 