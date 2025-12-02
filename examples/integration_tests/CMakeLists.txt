# Integration Test Suite
# End-to-End workflow tests for PACS system
#
# @see Issue #111 - Integration Test Suite

add_executable(pacs_integration_tests
    main.cpp
    test_connectivity.cpp
    test_store_query.cpp
    test_worklist_mpps.cpp
    test_stress.cpp
    test_error_recovery.cpp
    test_xa_storage.cpp
)

target_include_directories(pacs_integration_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(pacs_integration_tests
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_storage
        pacs_integration
        Catch2::Catch2WithMain
        Threads::Threads
)

# Set C++20 standard
target_compile_features(pacs_integration_tests PRIVATE cxx_std_20)

# Register tests with CTest
include(Catch)
catch_discover_tests(pacs_integration_tests
    TEST_PREFIX "integration::"
    REPORTER junit
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
    OUTPUT_PREFIX integration_
    OUTPUT_SUFFIX .xml
)

# Copy test data files
file(GLOB TEST_DATA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/test_data/*")
if(TEST_DATA_FILES)
    file(COPY ${TEST_DATA_FILES}
         DESTINATION ${CMAKE_BINARY_DIR}/bin/test_data)
endif()

# Install target (optional)
install(TARGETS pacs_integration_tests
    RUNTIME DESTINATION bin/tests
)
