cmake_minimum_required(VERSION 3.16)

set(SOURCE_FILES
    query_retrieve_scp.cpp
    query_retrieve_scp.h
)

add_library(pacs_query_retrieve_scp STATIC ${SOURCE_FILES})
target_include_directories(pacs_query_retrieve_scp PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}  # For core/result and other includes
)
target_link_libraries(pacs_query_retrieve_scp PUBLIC 
    pacs_core 
    pacs_common
    pacs_core_thread
    pacs_core_result
)

# DCMTK dependency
# Only link against DCMTK if it's available
if(NOT USE_DCMTK_PLACEHOLDER)
    target_link_libraries(pacs_query_retrieve_scp PUBLIC ${DCMTK_LIBRARIES})
    target_include_directories(pacs_query_retrieve_scp PUBLIC ${DCMTK_INCLUDE_DIRS})
endif()

