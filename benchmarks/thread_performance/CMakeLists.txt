# Thread Performance Benchmarks
# Baseline measurements for thread_system migration
#
# @see Issue #154 - Establish performance baseline benchmarks for thread migration

##################################################
# Benchmark Executable
##################################################

add_executable(thread_performance_benchmarks
    association_benchmark.cpp
    throughput_benchmark.cpp
    concurrent_load_benchmark.cpp
    shutdown_benchmark.cpp
)

target_include_directories(thread_performance_benchmarks
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(thread_performance_benchmarks
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_integration
        Catch2::Catch2WithMain
        Threads::Threads
)

# Set C++20 standard
target_compile_features(thread_performance_benchmarks PRIVATE cxx_std_20)

# Apply PACS warning flags
if(COMMAND pacs_apply_warnings)
    pacs_apply_warnings(thread_performance_benchmarks)
endif()

##################################################
# CTest Integration
##################################################

include(Catch)

# Register benchmark tests with CTest
# Use longer timeout for benchmark tests
catch_discover_tests(thread_performance_benchmarks
    TEST_PREFIX "benchmark::"
    REPORTER junit
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
    OUTPUT_PREFIX benchmark_
    OUTPUT_SUFFIX .xml
    PROPERTIES
        LABELS "benchmark"
        TIMEOUT 600
)

##################################################
# Custom Targets for Running Benchmarks
##################################################

# Quick benchmark run (subset of tests)
add_custom_target(run_quick_benchmarks
    COMMAND thread_performance_benchmarks
        "[benchmark][association]"
        "[benchmark][throughput][echo]"
        --reporter console
    DEPENDS thread_performance_benchmarks
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running quick benchmark suite..."
)

# Full benchmark run
add_custom_target(run_full_benchmarks
    COMMAND thread_performance_benchmarks
        "[benchmark]"
        --reporter console
    DEPENDS thread_performance_benchmarks
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running full benchmark suite..."
)

# Benchmark with JUnit output for CI
add_custom_target(run_benchmarks_ci
    COMMAND thread_performance_benchmarks
        "[benchmark]"
        --reporter junit
        --out ${CMAKE_BINARY_DIR}/benchmark_results.xml
    DEPENDS thread_performance_benchmarks
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running benchmarks with JUnit output..."
)

##################################################
# Install Target
##################################################

install(TARGETS thread_performance_benchmarks
    RUNTIME DESTINATION bin/benchmarks
)

##################################################
# Documentation
##################################################

message(STATUS "")
message(STATUS "=== Thread Performance Benchmarks ===")
message(STATUS "  Target: thread_performance_benchmarks")
message(STATUS "  Run quick: cmake --build . --target run_quick_benchmarks")
message(STATUS "  Run full:  cmake --build . --target run_full_benchmarks")
message(STATUS "  Run CI:    cmake --build . --target run_benchmarks_ci")
message(STATUS "")
