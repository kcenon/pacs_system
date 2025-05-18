cmake_minimum_required(VERSION 3.16)
project(WorklistSample)

# Set C++20 as the default standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add the executables
add_executable(worklist_scp_sample worklist_scp_sample.cpp)
add_executable(worklist_scu_sample worklist_scu_sample.cpp)

# Link against the required libraries
target_link_libraries(worklist_scp_sample
    pacs_common
    pacs_core
    pacs_worklist_scp
)

target_link_libraries(worklist_scu_sample
    pacs_common
    pacs_core
    pacs_worklist_scu
)

# Include directories
target_include_directories(worklist_scp_sample PRIVATE
    ${CMAKE_SOURCE_DIR}/..
)

target_include_directories(worklist_scu_sample PRIVATE
    ${CMAKE_SOURCE_DIR}/..
)