cmake_minimum_required(VERSION 3.16)

# Test executables
add_executable(test_dcmtk_integration test_dcmtk_integration.cpp)

# Include directories
target_include_directories(test_dcmtk_integration PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/thread_system/sources
    ${CMAKE_SOURCE_DIR}/thread_system/sources/utilities
    ${CMAKE_SOURCE_DIR}/thread_system/sources/thread_base
    ${CMAKE_SOURCE_DIR}/thread_system/sources/logger
)

# Link libraries
target_link_libraries(test_dcmtk_integration PRIVATE
    pacs_common
    pacs_core
    pacs_storage_scp
    pacs_storage_scu
    pacs_query_retrieve_scp
    pacs_query_retrieve_scu
    pacs_worklist_scp
    pacs_worklist_scu
    pacs_mpps_scp
    pacs_mpps_scu
    ${DCMTK_LIBRARIES}
    thread_pool
    thread_base
    utilities
    logger
)

# Add test
add_test(NAME DCMTKIntegrationTest COMMAND test_dcmtk_integration)

# Set test properties
set_tests_properties(DCMTKIntegrationTest PROPERTIES
    TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)