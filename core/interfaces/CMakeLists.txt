# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create interfaces library (header-only)
add_library(pacs_interfaces INTERFACE)

# Add alias
add_library(pacs::interfaces ALIAS pacs_interfaces)

# Configure include directories
target_include_directories(pacs_interfaces INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

# Link dependencies
target_link_libraries(pacs_interfaces INTERFACE
    pacs::common
)

# Set compile features
target_compile_features(pacs_interfaces INTERFACE cxx_std_20)

# Install headers
install(FILES
    service_interface.h
    DESTINATION include/pacs/core/interfaces
)

install(FILES
    storage/storage_interface.h
    DESTINATION include/pacs/core/interfaces/storage
)

install(FILES
    worklist/worklist_interface.h
    DESTINATION include/pacs/core/interfaces/worklist
)

install(FILES
    query_retrieve/query_retrieve_interface.h
    DESTINATION include/pacs/core/interfaces/query_retrieve
)

install(FILES
    mpps/mpps_interface.h
    DESTINATION include/pacs/core/interfaces/mpps
)

# Install targets
install(TARGETS pacs_interfaces
    EXPORT pacs_targets
)