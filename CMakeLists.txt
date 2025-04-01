cmake_minimum_required(VERSION 3.16)
project(PACSSystem)

# Set C++20 as the default standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Check for vcpkg toolchain
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR "Please provide the path to vcpkg toolchain file using -DCMAKE_TOOLCHAIN_FILE=<path_to_vcpkg_toolchain>")
endif()

# Set CMake module path
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Use thread_system from the project instead of internal thread_pool
message(STATUS "Using thread_system from project")
add_subdirectory(thread_system)

# For this simplified build, we'll use the placeholder implementation
message(STATUS "Using placeholder implementation for DCMTK")
# Define a variable to indicate DCMTK is not available
set(USE_DCMTK_PLACEHOLDER TRUE)
# Define dummy variables to keep the build going
set(DCMTK_INCLUDE_DIRS "")
set(DCMTK_LIBRARIES "")
add_definitions(-DDCMTK_NOT_AVAILABLE)

# Add subdirectories for each module
add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(modules)
add_subdirectory(apps)