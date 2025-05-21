cmake_minimum_required(VERSION 3.16)

# Add SCP and SCU components
add_subdirectory(scp)
add_subdirectory(scu)

# Create an INTERFACE library for MPPS
add_library(pacs_mpps INTERFACE)
target_include_directories(pacs_mpps INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

# Only link against direct dependencies
# pacs_core already includes pacs_common, so we don't need to link it again
target_link_libraries(pacs_mpps INTERFACE 
    pacs_core
    pacs_mpps_scp
    pacs_mpps_scu
)