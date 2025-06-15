# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create network library
add_library(pacs_network
    dicom_connection_pool.cpp
)

# Add alias
add_library(pacs::network ALIAS pacs_network)

# Configure include directories
target_include_directories(pacs_network
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(pacs_network
    PUBLIC
        pacs::logger
        pacs::dicom
)

# Set compile features
target_compile_features(pacs_network PUBLIC cxx_std_20)

# Install library
install(TARGETS pacs_network
    EXPORT pacs_targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(FILES
    connection_pool.h
    dicom_connection_pool.h
    retry_policy.h
    DESTINATION include/pacs/common/network
)