# samples/05_production_pacs/CMakeLists.txt
# Level 5: Production PACS - Enterprise-grade DICOM server
#
# This sample demonstrates production-ready features:
# - YAML configuration management
# - TLS security for DICOM connections
# - Role-based access control (RBAC)
# - Automatic anonymization
# - REST API for web access
# - Health monitoring and metrics
# - Event-driven architecture

cmake_minimum_required(VERSION 3.20)

# Executable name
set(SAMPLE_NAME production_pacs)

# Source files
set(SAMPLE_SOURCES
    main.cpp
    config_loader.cpp
    production_pacs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../04_mini_pacs/mini_pacs.cpp
)

# Header files (for IDE visibility)
set(SAMPLE_HEADERS
    config_loader.hpp
    production_pacs.hpp
)

# Create executable
add_executable(${SAMPLE_NAME}
    ${SAMPLE_SOURCES}
    ${SAMPLE_HEADERS}
)

# Include directories
target_include_directories(${SAMPLE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../04_mini_pacs
)

# Link dependencies
target_link_libraries(${SAMPLE_NAME} PRIVATE
    pacs_core
    pacs_encoding
    pacs_network
    pacs_services
    pacs_storage
    pacs_security
    pacs_web
    pacs_monitoring
    pacs_samples_common
)

# C++ standard
target_compile_features(${SAMPLE_NAME} PRIVATE cxx_std_20)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${SAMPLE_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
elseif(MSVC)
    target_compile_options(${SAMPLE_NAME} PRIVATE
        /W4
    )
endif()

# Set output directory
set_target_properties(${SAMPLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/samples"
)

# Copy configuration files to build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/pacs_config.yaml
    ${CMAKE_BINARY_DIR}/samples/config/pacs_config.yaml
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/pacs_config.example.yaml
    ${CMAKE_BINARY_DIR}/samples/config/pacs_config.example.yaml
    COPYONLY
)
