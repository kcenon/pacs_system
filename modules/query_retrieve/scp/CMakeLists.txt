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

# Only link against pacs_core which already includes common libraries
target_link_libraries(pacs_query_retrieve_scp PUBLIC 
    pacs_core
    pacs_core_thread
)

# DCMTK dependency
# Only link against DCMTK if it's available
if(NOT USE_DCMTK_PLACEHOLDER)
    target_link_libraries(pacs_query_retrieve_scp PUBLIC ${DCMTK_LIBRARIES})
    target_include_directories(pacs_query_retrieve_scp PUBLIC ${DCMTK_INCLUDE_DIRS})
endif()
