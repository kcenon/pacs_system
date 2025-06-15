cmake_minimum_required(VERSION 3.16)

# Result library (header-only)
add_library(pacs_core_result INTERFACE)
target_include_directories(pacs_core_result INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Only need to link against pacs_common without thread_base or utilities
target_link_libraries(pacs_core_result INTERFACE 
    pacs_common
)