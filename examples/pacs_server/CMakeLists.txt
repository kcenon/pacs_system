# PACS Server Example
# Complete DICOM archive demonstrating all PACS functionality

add_executable(pacs_server
    main.cpp
    config.cpp
    server_app.cpp
)

target_include_directories(pacs_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(pacs_server
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_storage
        pacs_integration
)

# Set C++20 standard
target_compile_features(pacs_server PRIVATE cxx_std_20)

# Install target
install(TARGETS pacs_server
    RUNTIME DESTINATION bin
)

# Copy example configuration
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/pacs_server.yaml.example
    ${CMAKE_BINARY_DIR}/bin/pacs_server.yaml.example
    COPYONLY
)
