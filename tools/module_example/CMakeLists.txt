# Module Example
# Demonstrates C++20 module usage with pacs_system
#
# This example only builds when PACS_BUILD_MODULES=ON

add_executable(module_example
    main.cpp
)

target_include_directories(module_example
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link module library when modules are enabled, otherwise link headers
if(PACS_BUILD_MODULES AND TARGET pacs_system_modules)
    target_link_libraries(module_example
        PRIVATE
            pacs_system_modules
    )

    # Enable module scanning
    set_target_properties(module_example PROPERTIES
        CXX_SCAN_FOR_MODULES ON
    )
else()
    # Fallback to header-based linking
    target_link_libraries(module_example
        PRIVATE
            pacs_core
            pacs_encoding
    )
endif()

# Set C++20 standard
target_compile_features(module_example PRIVATE cxx_std_20)

# Install target
install(TARGETS module_example
    RUNTIME DESTINATION bin
)
