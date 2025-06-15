# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create audit library
add_library(pacs_audit
    audit_logger.cpp
)

# Add alias
add_library(pacs::audit ALIAS pacs_audit)

# Configure include directories
target_include_directories(pacs_audit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(pacs_audit
    PUBLIC
        pacs::logger
        pacs::config
)

# Set compile features
target_compile_features(pacs_audit PUBLIC cxx_std_20)

# Install library
install(TARGETS pacs_audit
    EXPORT pacs_targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(FILES
    audit_logger.h
    DESTINATION include/pacs/common/audit
)