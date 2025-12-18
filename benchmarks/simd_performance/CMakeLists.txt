# SIMD Performance Benchmarks
# Measures performance of SIMD optimizations for byte swapping and RLE codec
#
# @see Issue #332 - Add SIMD optimization benchmarks
# @see Issue #313 - Apply SIMD Optimization (Image Processing)

##################################################
# Benchmark Executable
##################################################

add_executable(simd_performance_benchmarks
    byte_swap_benchmark.cpp
    rle_simd_benchmark.cpp
)

target_include_directories(simd_performance_benchmarks
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(simd_performance_benchmarks
    PRIVATE
        pacs_encoding
        Catch2::Catch2WithMain
        Threads::Threads
)

# Set C++20 standard
target_compile_features(simd_performance_benchmarks PRIVATE cxx_std_20)

# Apply PACS warning flags
if(COMMAND pacs_apply_warnings)
    pacs_apply_warnings(simd_performance_benchmarks)
endif()

# Enable SIMD optimizations
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Enable SSE/AVX on x86/x64
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686")
        target_compile_options(simd_performance_benchmarks PRIVATE
            -msse2 -msse3 -mssse3 -msse4.1 -msse4.2
        )
        # AVX2 is optional - enable if available
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
        if(COMPILER_SUPPORTS_AVX2)
            target_compile_options(simd_performance_benchmarks PRIVATE -mavx2)
        endif()
    endif()
    # NEON is automatically enabled on ARM
endif()

if(MSVC)
    # Enable AVX2 on MSVC
    target_compile_options(simd_performance_benchmarks PRIVATE /arch:AVX2)
endif()

##################################################
# CTest Integration
##################################################

include(Catch)

# Register benchmark tests with CTest
# Use longer timeout for benchmark tests
catch_discover_tests(simd_performance_benchmarks
    TEST_PREFIX "benchmark::simd::"
    REPORTER junit
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
    OUTPUT_PREFIX simd_benchmark_
    OUTPUT_SUFFIX .xml
    PROPERTIES
        LABELS "benchmark;simd"
        TIMEOUT 600
)

##################################################
# Custom Targets for Running Benchmarks
##################################################

# Quick benchmark run (summary only)
add_custom_target(run_simd_benchmarks_quick
    COMMAND simd_performance_benchmarks
        "[benchmark][simd][summary]"
        --reporter console
    DEPENDS simd_performance_benchmarks
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running quick SIMD benchmark summary..."
)

# Byte swap benchmarks only
add_custom_target(run_simd_benchmarks_byte_swap
    COMMAND simd_performance_benchmarks
        "[benchmark][simd][byte_swap]"
        --reporter console
    DEPENDS simd_performance_benchmarks
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running byte swap benchmarks..."
)

# RLE SIMD benchmarks only
add_custom_target(run_simd_benchmarks_rle
    COMMAND simd_performance_benchmarks
        "[benchmark][simd][rle]"
        --reporter console
    DEPENDS simd_performance_benchmarks
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running RLE SIMD benchmarks..."
)

# Full benchmark run
add_custom_target(run_simd_benchmarks_full
    COMMAND simd_performance_benchmarks
        "[benchmark][simd]"
        --reporter console
    DEPENDS simd_performance_benchmarks
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running full SIMD benchmark suite..."
)

# Catch2 BENCHMARK mode (for detailed timing)
add_custom_target(run_simd_benchmarks_detailed
    COMMAND simd_performance_benchmarks
        "[benchmark][simd][catch2]"
        --reporter console
    DEPENDS simd_performance_benchmarks
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running SIMD benchmarks with Catch2 BENCHMARK..."
)

# Benchmark with JUnit output for CI
add_custom_target(run_simd_benchmarks_ci
    COMMAND simd_performance_benchmarks
        "[benchmark][simd]"
        --reporter junit
        --out ${CMAKE_BINARY_DIR}/simd_benchmark_results.xml
    DEPENDS simd_performance_benchmarks
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running SIMD benchmarks with JUnit output..."
)

##################################################
# Install Target
##################################################

install(TARGETS simd_performance_benchmarks
    RUNTIME DESTINATION bin/benchmarks
)

##################################################
# Documentation
##################################################

message(STATUS "")
message(STATUS "=== SIMD Performance Benchmarks ===")
message(STATUS "  Target: simd_performance_benchmarks")
message(STATUS "  Run quick:    cmake --build . --target run_simd_benchmarks_quick")
message(STATUS "  Run full:     cmake --build . --target run_simd_benchmarks_full")
message(STATUS "  Run detailed: cmake --build . --target run_simd_benchmarks_detailed")
message(STATUS "  Run CI:       cmake --build . --target run_simd_benchmarks_ci")
message(STATUS "")
