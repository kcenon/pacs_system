cmake_minimum_required(VERSION 3.16)

set(INTERFACES_SOURCE_FILES
    mpps/mpps_interface.h
    storage/storage_interface.h
    worklist/worklist_interface.h
    query_retrieve/query_retrieve_interface.h
)

add_library(pacs_core_interfaces INTERFACE)
target_include_directories(pacs_core_interfaces INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(pacs_core_interfaces INTERFACE 
    pacs_common
    pacs_core_result)