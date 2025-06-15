# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create mpps SCP library
add_library(pacs_mpps_scp
    mpps_scp.cpp
)

# Add alias
add_library(pacs::mpps::scp ALIAS pacs_mpps_scp)

# Configure include directories
target_include_directories(pacs_mpps_scp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
        $<INSTALL_INTERFACE:include>
)

# Link libraries
target_link_libraries(pacs_mpps_scp
    PUBLIC
        pacs::dicom
        pacs::common
        pacs::interfaces
)

# Set compile features
target_compile_features(pacs_mpps_scp PUBLIC cxx_std_20)