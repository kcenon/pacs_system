cmake_minimum_required(VERSION 3.16)

# Add subdirectories
add_subdirectory(result)
add_subdirectory(interfaces)
add_subdirectory(database)
add_subdirectory(thread)

# Create an INTERFACE library instead since we don't have direct source files
add_library(pacs_core INTERFACE)
target_include_directories(pacs_core INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(pacs_core INTERFACE 
    pacs_common
    pacs_core_result
    pacs_core_interfaces
    pacs_core_database
    pacs_core_thread
    # Direct thread_system dependencies
    priority_thread_pool
    thread_base
    thread_pool
    logger
    utilities
)

# DCMTK dependency (only when available)
if(NOT USE_DCMTK_PLACEHOLDER)
    find_package(DCMTK REQUIRED)
    target_link_libraries(pacs_core INTERFACE ${DCMTK_LIBRARIES})
    target_include_directories(pacs_core INTERFACE ${DCMTK_INCLUDE_DIRS})
endif()