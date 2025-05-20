cmake_minimum_required(VERSION 3.16)

# Database module for PACS system
add_library(pacs_core_database
    database_interface.cpp
    database_manager.cpp
    sqlite_database.cpp
)

target_include_directories(pacs_core_database PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

# Find SQLite3 dependency
find_package(SQLite3 REQUIRED)

target_link_libraries(pacs_core_database PUBLIC 
    pacs_common
    pacs_core_result
    utilities
    logger
    ${SQLite3_LIBRARIES}
)

target_include_directories(pacs_core_database PUBLIC 
    ${SQLite3_INCLUDE_DIRS}
)