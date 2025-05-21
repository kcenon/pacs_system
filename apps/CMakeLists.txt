cmake_minimum_required(VERSION 3.16)

# Sample app
add_executable(pacs_server
    pacs_server.cpp
)

# Link against the required libraries for the main application
target_link_libraries(pacs_server PRIVATE
    pacs_mpps
    pacs_storage
    pacs_worklist
    pacs_query_retrieve
    pacs_common
    pacs_config
    pacs_logger
    pacs_security
)

# DCMTK dependency
# Only link against DCMTK if it's available
if(NOT USE_DCMTK_PLACEHOLDER)
    target_link_libraries(pacs_server PRIVATE ${DCMTK_LIBRARIES})
    target_include_directories(pacs_server PRIVATE ${DCMTK_INCLUDE_DIRS})
endif()