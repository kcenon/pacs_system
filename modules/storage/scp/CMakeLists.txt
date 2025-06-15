# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create storage SCP library
add_library(pacs_storage_scp
    storage_scp.cpp
)

# Add alias
add_library(pacs::storage::scp ALIAS pacs_storage_scp)

# Configure include directories
target_include_directories(pacs_storage_scp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
        $<INSTALL_INTERFACE:include>
)

# Link libraries
target_link_libraries(pacs_storage_scp
    PUBLIC
        pacs::dicom
        pacs::common
        pacs::interfaces
        thread_pool
        thread_base
)

# Set compile features
target_compile_features(pacs_storage_scp PUBLIC cxx_std_20)