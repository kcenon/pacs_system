cmake_minimum_required(VERSION 3.16)

# Add executable for simplified server
add_executable(simple_storage_server
    simple_storage_server.cpp
)

# Add executable for simplified client
add_executable(simple_storage_client
    simple_storage_client.cpp
)

# Link libraries for server
target_link_libraries(simple_storage_server
    PRIVATE
    pacs_common
    pacs_dicom
    priority_thread_pool_module
    logger_module
)

# Link libraries for client
target_link_libraries(simple_storage_client
    PRIVATE
    pacs_common
    pacs_dicom
    priority_thread_pool_module
    logger_module
)

# Set C++ standard
set_target_properties(simple_storage_server simple_storage_client
    PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Install targets
install(TARGETS simple_storage_server simple_storage_client
    RUNTIME DESTINATION bin
)