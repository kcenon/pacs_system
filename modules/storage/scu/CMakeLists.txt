# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create storage SCU library
add_library(pacs_storage_scu
    storage_scu.cpp
)

# Add alias
add_library(pacs::storage::scu ALIAS pacs_storage_scu)

# Configure include directories
target_include_directories(pacs_storage_scu
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
        $<INSTALL_INTERFACE:include>
)

# Link libraries
target_link_libraries(pacs_storage_scu
    PUBLIC
        pacs::dicom
        pacs::common
        pacs::interfaces
)

# Set compile features
target_compile_features(pacs_storage_scu PUBLIC cxx_std_20)