# Database Browser Sample
# PACS index database inspection utility

add_executable(db_browser
    main.cpp
)

target_include_directories(db_browser
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
# Note: db_browser needs storage for database access
target_link_libraries(db_browser
    PRIVATE
        pacs_storage
)

# Set C++20 standard
target_compile_features(db_browser PRIVATE cxx_std_20)

# Install target
install(TARGETS db_browser
    RUNTIME DESTINATION bin
)
