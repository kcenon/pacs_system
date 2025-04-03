cmake_minimum_required(VERSION 3.16)

set(SOURCE_FILES
    result.h
)

add_library(pacs_core_result INTERFACE)
target_include_directories(pacs_core_result INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/sources  # For core/result includes
)
target_link_libraries(pacs_core_result INTERFACE pacs_common)

