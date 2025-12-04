# Integration Test Suite
# End-to-End workflow tests for PACS system
#
# @see Issue #111 - Integration Test Suite

add_executable(pacs_integration_e2e
    main.cpp
    test_connectivity.cpp
    test_store_query.cpp
    test_worklist_mpps.cpp
    test_stress.cpp
    test_error_recovery.cpp
    test_xa_storage.cpp
    test_multimodal_workflow.cpp
    test_tls_integration.cpp
    test_stability.cpp
    test_data_generator.cpp
    test_data_generator_test.cpp
)

target_include_directories(pacs_integration_e2e
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(pacs_integration_e2e
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
target_compile_features(pacs_integration_e2e PRIVATE cxx_std_20)

# Register tests with CTest
# @see Issue #139 - CI/CD Integration
include(Catch)
catch_discover_tests(pacs_integration_e2e
    TEST_PREFIX "integration::"
    REPORTER junit
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
    OUTPUT_PREFIX integration_
    OUTPUT_SUFFIX .xml
    PROPERTIES
        LABELS "integration"
        TIMEOUT 300
)

# Copy test data files
file(GLOB TEST_DATA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/test_data/*")
if(TEST_DATA_FILES)
    file(COPY ${TEST_DATA_FILES}
         DESTINATION ${CMAKE_BINARY_DIR}/bin/test_data)
endif()

# Test data generator utility
# @see Issue #136 - Binary Integration Test Scripts
add_executable(generate_test_data
    generate_test_data.cpp
)

target_include_directories(generate_test_data
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(generate_test_data
    PRIVATE
        pacs_core
        pacs_encoding
        Threads::Threads
)

target_compile_features(generate_test_data PRIVATE cxx_std_20)

# Custom target to generate test data
add_custom_target(generate_binary_test_data
    COMMAND generate_test_data ${CMAKE_CURRENT_SOURCE_DIR}/test_data
    DEPENDS generate_test_data
    COMMENT "Generating DICOM test data files..."
)

# Install target (optional)
install(TARGETS pacs_integration_e2e generate_test_data
    RUNTIME DESTINATION bin/tests
)
