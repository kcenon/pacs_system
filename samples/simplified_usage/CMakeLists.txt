cmake_minimum_required(VERSION 3.16)

# Add executable for simple DICOM operations
add_executable(simple_dicom_operations
    simple_dicom_operations.cpp
)

# Link libraries
target_link_libraries(simple_dicom_operations
    PRIVATE
    pacs_common
    pacs_dicom
    priority_thread_pool_module
    logger_module
)

# Set C++ standard
set_target_properties(simple_dicom_operations
    PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Install target
install(TARGETS simple_dicom_operations
    RUNTIME DESTINATION bin
)