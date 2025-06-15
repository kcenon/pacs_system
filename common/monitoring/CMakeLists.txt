# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create monitoring library
add_library(pacs_monitoring
    monitoring_service.cpp
)

# Add alias
add_library(pacs::monitoring ALIAS pacs_monitoring)

# Configure include directories
target_include_directories(pacs_monitoring
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(pacs_monitoring
    PUBLIC
        pacs::logger
        pacs::config
)

# Set compile features
target_compile_features(pacs_monitoring PUBLIC cxx_std_20)

# Install library
install(TARGETS pacs_monitoring
    EXPORT pacs_targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(FILES
    monitoring_dashboard.h
    monitoring_service.h
    DESTINATION include/pacs/common/monitoring
)