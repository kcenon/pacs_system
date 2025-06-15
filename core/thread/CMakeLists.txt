# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create thread library (currently header-only, prepare for future implementation)
add_library(pacs_thread INTERFACE)

# Add alias
add_library(pacs::thread ALIAS pacs_thread)

# Configure include directories
target_include_directories(pacs_thread INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
    $<INSTALL_INTERFACE:include>
)

# Find Threads
find_package(Threads QUIET)

# Link dependencies
target_link_libraries(pacs_thread INTERFACE
    pacs::common
)

if(Threads_FOUND)
    target_link_libraries(pacs_thread INTERFACE Threads::Threads)
endif()

# Set compile features
target_compile_features(pacs_thread INTERFACE cxx_std_20)

# Install targets
install(TARGETS pacs_thread
    EXPORT pacs_targets
)