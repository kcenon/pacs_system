cmake_minimum_required(VERSION 3.16)

# Core thread module for PACS system
add_library(pacs_core_thread
    thread_manager.cpp
    thread_pool.cpp
)

target_include_directories(pacs_core_thread PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(pacs_core_thread PUBLIC 
    pacs_common
    pacs_core_result
    priority_thread_pool
    thread_base
    thread_pool
    logger
    utilities
)