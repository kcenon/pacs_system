cmake_minimum_required(VERSION 3.16)

set(SOURCE_FILES
    mpps_scp.cpp
    mpps_scp.h
)

add_library(pacs_mpps_scp STATIC ${SOURCE_FILES})
target_include_directories(pacs_mpps_scp PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}  # For core/result and other includes
)
target_link_libraries(pacs_mpps_scp PUBLIC 
    pacs_core 
    pacs_common
    pacs_core_thread
    pacs_core_result
)

# DCMTK dependency (only when available)
if(NOT USE_DCMTK_PLACEHOLDER)
    target_link_libraries(pacs_mpps_scp PUBLIC ${DCMTK_LIBRARIES})
    target_include_directories(pacs_mpps_scp PUBLIC ${DCMTK_INCLUDE_DIRS})
endif()

