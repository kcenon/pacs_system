cmake_minimum_required(VERSION 3.16)
project(PACSSystemSamples)

# Set C++20 as the default standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Add the sample subdirectories
add_subdirectory(storage_sample)
add_subdirectory(worklist_sample)
add_subdirectory(query_retrieve_sample)
add_subdirectory(mpps_sample)
add_subdirectory(integrated_sample)

# Add simplified API samples
add_subdirectory(simplified_storage)
add_subdirectory(simplified_usage)

# Add database, config, logger, and security samples
add_subdirectory(database_sample)
add_subdirectory(config_sample)
add_subdirectory(logger_sample)
add_subdirectory(security_sample)

# Add DICOM test program
add_executable(simple_dicom_test simple_dicom_test.cpp)
target_link_libraries(simple_dicom_test
    pacs_dicom
    pacs_common
)

# Add storage test program
add_executable(storage_test storage_test.cpp)
target_link_libraries(storage_test
    pacs_dicom
    pacs_common
)