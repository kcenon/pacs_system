# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create version library (header-only)
add_library(pacs_version INTERFACE)

# Add alias
add_library(pacs::version ALIAS pacs_version)

# Configure include directories
target_include_directories(pacs_version INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
    $<INSTALL_INTERFACE:include>
)

# Set compile features
target_compile_features(pacs_version INTERFACE cxx_std_20)

# Install headers
install(FILES
    api_version.h
    DESTINATION include/pacs/common/version
)

# Install targets
install(TARGETS pacs_version
    EXPORT pacs_targets
)