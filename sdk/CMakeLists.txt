# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# PACS System SDK Package Configuration

cmake_minimum_required(VERSION 3.16)

# Create SDK package
add_library(PACSSystemSDK INTERFACE)

# Add alias for consistent naming
add_library(PACS::SDK ALIAS PACSSystemSDK)

# SDK includes all major components
target_link_libraries(PACSSystemSDK
    INTERFACE
        pacs::core
        pacs::storage::scp
        pacs::storage::scu
        pacs::query_retrieve::scp
        pacs::query_retrieve::scu
        pacs::worklist::scp
        pacs::worklist::scu
        pacs::mpps::scp
        pacs::mpps::scu
        pacs::common
        pacs::security
        pacs::monitoring
        pacs::performance
        pacs::compliance
        pacs::integration
        pacs::audit
        pacs::network
)

# Install SDK headers
install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/core/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pacs/core
    FILES_MATCHING PATTERN "*.h"
)

install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/modules/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pacs/modules
    FILES_MATCHING PATTERN "*.h"
)

install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/common/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pacs/common
    FILES_MATCHING PATTERN "*.h"
)