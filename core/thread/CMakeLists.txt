cmake_minimum_required(VERSION 3.16)

# Use the INTERFACE library approach to provide direct access to thread_system
add_library(pacs_core_thread INTERFACE)

# Make thread_system headers available through this interface
target_include_directories(pacs_core_thread INTERFACE 
    ${CMAKE_SOURCE_DIR}/thread_system/sources
)

# Link directly to thread_system libraries
target_link_libraries(pacs_core_thread INTERFACE 
    thread_pool
    priority_thread_pool
)