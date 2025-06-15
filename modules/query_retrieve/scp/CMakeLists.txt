# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create query_retrieve SCP library
add_library(pacs_query_retrieve_scp
    query_retrieve_scp.cpp
)

# Add alias
add_library(pacs::query_retrieve::scp ALIAS pacs_query_retrieve_scp)

# Configure include directories
target_include_directories(pacs_query_retrieve_scp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
        $<INSTALL_INTERFACE:include>
)

# Link libraries
target_link_libraries(pacs_query_retrieve_scp
    PUBLIC
        pacs::dicom
        pacs::common
        pacs::interfaces
)

# Set compile features
target_compile_features(pacs_query_retrieve_scp PUBLIC cxx_std_20)