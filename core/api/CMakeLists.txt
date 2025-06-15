# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create API library
add_library(pacs_api
    rest_api_server.cpp
)

# Configure include directories
target_include_directories(pacs_api PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Find packages
find_package(nlohmann_json QUIET)

# Link libraries
target_link_libraries(pacs_api
    PUBLIC
        pacs_core
        pacs_logger
        pacs_security
        pacs_audit
        pacs_version
)

if(nlohmann_json_FOUND)
    target_link_libraries(pacs_api PUBLIC nlohmann_json::nlohmann_json)
    target_compile_definitions(pacs_api PRIVATE HAS_NLOHMANN_JSON)
endif()

# Set compile features
target_compile_features(pacs_api PUBLIC cxx_std_20)

# Install library
install(TARGETS pacs_api
    EXPORT pacs_targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(FILES
    rest_api_server.h
    DESTINATION include/pacs/core/api
)