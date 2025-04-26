cmake_minimum_required(VERSION 3.16)

# Add SCP and SCU components
add_subdirectory(scp)
add_subdirectory(scu)

# Create an INTERFACE library for worklist
add_library(pacs_worklist INTERFACE)
target_include_directories(pacs_worklist INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(pacs_worklist INTERFACE 
    pacs_core
    pacs_common
    pacs_worklist_scp
    pacs_worklist_scu
)
