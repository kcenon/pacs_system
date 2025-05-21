cmake_minimum_required(VERSION 3.16)

# Add SCP and SCU components
add_subdirectory(scp)
add_subdirectory(scu)

# Create an INTERFACE library for query_retrieve
add_library(pacs_query_retrieve INTERFACE)
target_include_directories(pacs_query_retrieve INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

# Only link against direct dependencies
# pacs_core already includes pacs_common, so we don't need to link it again
target_link_libraries(pacs_query_retrieve INTERFACE 
    pacs_core
    pacs_query_retrieve_scp
    pacs_query_retrieve_scu
)