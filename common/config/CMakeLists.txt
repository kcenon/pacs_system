# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create config library
add_library(pacs_config
    config_manager.cpp
)

# Add alias
add_library(pacs::config ALIAS pacs_config)

# Configure include directories
target_include_directories(pacs_config
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find packages
find_package(nlohmann_json QUIET)

# Link libraries
target_link_libraries(pacs_config
    PUBLIC
        pacs::logger
)

if(nlohmann_json_FOUND)
    target_link_libraries(pacs_config PUBLIC nlohmann_json::nlohmann_json)
    target_compile_definitions(pacs_config PRIVATE HAS_NLOHMANN_JSON)
endif()

# Set compile features
target_compile_features(pacs_config PUBLIC cxx_std_20)

# Install library
install(TARGETS pacs_config
    EXPORT pacs_targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(FILES
    config_manager.h
    DESTINATION include/pacs/common/config
)