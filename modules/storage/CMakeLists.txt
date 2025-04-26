cmake_minimum_required(VERSION 3.16)

# Add SCP and SCU components
add_subdirectory(scp)
add_subdirectory(scu)

# Create an INTERFACE library for storage
add_library(pacs_storage INTERFACE)
target_include_directories(pacs_storage INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(pacs_storage INTERFACE 
    pacs_core
    pacs_common
    pacs_storage_scp
    pacs_storage_scu
)
