cmake_minimum_required(VERSION 3.16)

set(SOURCE_FILES
    storage_scu.cpp
    storage_scu.h
)

add_library(pacs_storage_scu STATIC ${SOURCE_FILES})
target_include_directories(pacs_storage_scu PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}  # For core/result and other includes
)
target_link_libraries(pacs_storage_scu PUBLIC 
    pacs_core 
    pacs_common
    pacs_core_thread
    pacs_core_result
)

# DCMTK dependency (only when available)
if(NOT USE_DCMTK_PLACEHOLDER)
    target_link_libraries(pacs_storage_scu PUBLIC ${DCMTK_LIBRARIES})
    target_include_directories(pacs_storage_scu PUBLIC ${DCMTK_INCLUDE_DIRS})
endif()

