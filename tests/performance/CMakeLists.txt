# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create performance test framework library
add_library(performance_test_framework
    performance_test_framework.cpp
)

target_link_libraries(performance_test_framework
    PUBLIC
        pacs::logger
        benchmark::benchmark
)

target_compile_features(performance_test_framework PUBLIC cxx_std_20)

# Database performance tests
add_executable(database_performance_tests
    database_performance_tests.cpp
)

target_link_libraries(database_performance_tests
    PRIVATE
        performance_test_framework
        pacs::database
        benchmark::benchmark_main
)

target_compile_features(database_performance_tests PRIVATE cxx_std_20)

# DICOM performance tests
add_executable(dicom_performance_tests
    dicom_performance_tests.cpp
)

target_link_libraries(dicom_performance_tests
    PRIVATE
        performance_test_framework
        pacs::dicom
        benchmark::benchmark_main
)

target_compile_features(dicom_performance_tests PRIVATE cxx_std_20)

# Network performance tests
add_executable(network_performance_tests
    network_performance_tests.cpp
)

target_link_libraries(network_performance_tests
    PRIVATE
        performance_test_framework
        pacs::network
        benchmark::benchmark_main
)

target_compile_features(network_performance_tests PRIVATE cxx_std_20)

# Add tests
add_test(NAME DatabasePerformanceTest COMMAND database_performance_tests)
add_test(NAME DICOMPerformanceTest COMMAND dicom_performance_tests)
add_test(NAME NetworkPerformanceTest COMMAND network_performance_tests)