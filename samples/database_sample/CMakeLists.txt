cmake_minimum_required(VERSION 3.16)

add_executable(database_sample
    database_sample.cpp
)

target_link_libraries(database_sample 
    pacs_core
    pacs_core_database
)

target_include_directories(database_sample PRIVATE
    ${CMAKE_SOURCE_DIR}
)