cmake_minimum_required(VERSION 3.16)

# Core thread module for PACS system
# Since the implementation files are empty, use INTERFACE library instead
add_library(pacs_core_thread INTERFACE)

target_include_directories(pacs_core_thread INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

# Using INTERFACE ensures libraries are only linked where they're used
target_link_libraries(pacs_core_thread INTERFACE 
    pacs_common
    pacs_core_result
    priority_thread_pool
    thread_base
    thread_pool
    logger
    utilities
)