# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create database library
add_library(pacs_database
    database_interface.cpp
    database_manager.cpp
    postgresql_database.cpp
    sqlite_database.cpp
)

# Add alias
add_library(pacs::database ALIAS pacs_database)

# Configure include directories
target_include_directories(pacs_database
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find packages
find_package(PostgreSQL QUIET)
find_package(SQLite3 QUIET)

# Link libraries
target_link_libraries(pacs_database
    PUBLIC
        pacs::common
        pacs::interfaces
)

if(PostgreSQL_FOUND)
    target_link_libraries(pacs_database PRIVATE PostgreSQL::PostgreSQL)
    target_compile_definitions(pacs_database PRIVATE HAS_POSTGRESQL)
endif()

if(SQLite3_FOUND)
    target_link_libraries(pacs_database PRIVATE SQLite::SQLite3)
    target_compile_definitions(pacs_database PRIVATE HAS_SQLITE3)
endif()

# Set compile features
target_compile_features(pacs_database PUBLIC cxx_std_20)

# Install library
install(TARGETS pacs_database
    EXPORT pacs_targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(FILES
    database_interface.h
    database_manager.h
    postgresql_database.h
    sqlite_database.h
    DESTINATION include/pacs/core/database
)