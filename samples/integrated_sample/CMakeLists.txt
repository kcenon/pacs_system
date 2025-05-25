cmake_minimum_required(VERSION 3.16)
project(IntegratedSample)

# Set C++20 as the default standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add the executables
add_executable(integrated_pacs_server integrated_pacs_server.cpp)
add_executable(integrated_pacs_client integrated_pacs_client.cpp)

# Link against the required libraries
target_link_libraries(integrated_pacs_server
    pacs_common
    pacs_core
    pacs_storage_scp
    pacs_worklist_scp
    pacs_query_retrieve_scp
    pacs_mpps_scp
)

target_link_libraries(integrated_pacs_client
    pacs_common
    pacs_core
    pacs_storage_scu
    pacs_worklist_scu
    pacs_query_retrieve_scu
    pacs_mpps_scu
)

# Include directories
target_include_directories(integrated_pacs_server PRIVATE
    ${CMAKE_SOURCE_DIR}/..
)

target_include_directories(integrated_pacs_client PRIVATE
    ${CMAKE_SOURCE_DIR}/..
)