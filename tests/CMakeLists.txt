# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Enable testing
enable_testing()

# Find packages
find_package(GTest QUIET)
find_package(benchmark QUIET)

# Add DCMTK integration tests
if(GTest_FOUND)
    add_executable(test_dcmtk_integration
        test_dcmtk_integration.cpp
    )

    # Always link these libraries
    target_link_libraries(test_dcmtk_integration
        PRIVATE
            pacs_dicom
            pacs_security
            pacs_common
            GTest::gtest
            GTest::gtest_main
            # Thread system libraries
            thread_base
            logger
    )
    
    # Link DCMTK if found
    if(DCMTK_FOUND)
        target_link_libraries(test_dcmtk_integration
            PRIVATE
                DCMTK::DCMTK
        )
    endif()
    
    # Link storage module which is now fixed
    if(DCMTK_FOUND AND NOT USE_DCMTK_PLACEHOLDER)
        target_link_libraries(test_dcmtk_integration
            PRIVATE
                pacs::storage::scp
                pacs::storage::scu
                pacs::query_retrieve::scp
                pacs::query_retrieve::scu
                pacs::worklist::scp
                pacs::worklist::scu
                pacs::mpps::scp
                pacs::mpps::scu
        )
    endif()
    
    # Add library search path
    target_link_directories(test_dcmtk_integration
        PRIVATE
            ${CMAKE_BINARY_DIR}/vcpkg_installed/arm64-osx/lib
    )
    
    target_compile_features(test_dcmtk_integration PRIVATE cxx_std_20)
    
    # Pass USE_DCMTK_PLACEHOLDER definition to the test
    if(USE_DCMTK_PLACEHOLDER)
        target_compile_definitions(test_dcmtk_integration PRIVATE USE_DCMTK_PLACEHOLDER)
    endif()
    
    # Add test
    add_test(NAME dcmtk_integration COMMAND test_dcmtk_integration)
endif()

# Add other tests here

# Add performance subdirectory if benchmark is found
if(benchmark_FOUND)
    add_subdirectory(performance)
else()
    message(STATUS "benchmark not found, skipping performance tests")
endif()