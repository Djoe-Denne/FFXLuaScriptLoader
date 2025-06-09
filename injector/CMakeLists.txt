# Injector Module (EXE)
# Responsible for DLL injection into target processes

add_executable(ff8_injector
    src/injector.cpp
)

# Injector only needs basic Windows libraries (no external dependencies)
target_link_libraries(ff8_injector PRIVATE 
    kernel32
    user32
)

# Include directories
target_include_directories(ff8_injector PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Set properties
set_target_properties(ff8_injector PROPERTIES
    OUTPUT_NAME "ff8_injector"
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(ff8_injector PRIVATE 
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    
    # Set subsystem for console application
    set_target_properties(ff8_injector PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
endif()

# Debug/Release configurations
target_compile_definitions(ff8_injector PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# Compiler-specific options
if(MSVC)
    target_compile_options(ff8_injector PRIVATE
        /W4        # High warning level
        /permissive-  # Disable non-standard extensions
        $<$<CONFIG:Release>:/O2>  # Optimize for speed in release
    )
endif()

message(STATUS "Configured FF8 Injector (EXE)")

# Install
install(TARGETS ff8_injector
    RUNTIME DESTINATION bin
    COMPONENT Runtime
) 