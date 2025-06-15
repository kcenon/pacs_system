cmake_minimum_required(VERSION 3.16)

# Add subdirectories
add_subdirectory(result)
add_subdirectory(interfaces)
add_subdirectory(database)
add_subdirectory(thread)
add_subdirectory(api)

# Create an INTERFACE library instead since we don't have direct source files
add_library(pacs_core INTERFACE)
target_include_directories(pacs_core INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Link only our core components - their dependencies will be transitively included
# without duplicating them here
target_link_libraries(pacs_core INTERFACE 
    pacs_common
    pacs_core_result
    pacs_interfaces
    pacs_database
    pacs_thread
    # Remove direct thread_system dependencies - we get these through pacs_core_thread
)

# DCMTK dependency (only when available)
if(NOT USE_DCMTK_PLACEHOLDER)
    find_package(DCMTK QUIET)
    if(DCMTK_FOUND AND TARGET DCMTK::DCMTK)
        target_link_libraries(pacs_core INTERFACE DCMTK::DCMTK)
    endif()
endif()