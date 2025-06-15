# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Logger is provided by thread_system, so this is just an interface library
add_library(pacs_logger INTERFACE)
add_library(pacs::logger ALIAS pacs_logger)

# Configure include directories
target_include_directories(pacs_logger INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Link to thread_system logger
target_link_libraries(pacs_logger INTERFACE logger)

# Install headers
install(FILES
    log_module.h
    DESTINATION include/pacs/common/logger
)