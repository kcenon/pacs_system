cmake_minimum_required(VERSION 3.20)

project(pacs_system
    VERSION 0.1.0
    DESCRIPTION "Modern C++20 PACS implementation"
    LANGUAGES CXX
)

##################################################
# C++ Standard and Build Configuration
##################################################

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build settings
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(PACS_SYSTEM_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/pacs_system")

##################################################
# Modular CMake Configuration
#
# Each include() file handles a specific concern:
#   options.cmake      - Build options, feature flags, utility functions
#   compiler.cmake     - Compiler warnings and the pacs_apply_warnings() function
#   dependencies.cmake - find_package, FetchContent, ecosystem deps (Tier 0-4)
#   targets.cmake      - PACS library targets and source file lists
#   install.cmake      - Install rules, export configuration
#   testing.cmake      - Test targets and CTest registration
#   examples.cmake     - Example binaries, benchmarks, CLI smoke tests
#   warnings.cmake     - Apply compiler warnings to all PACS targets
#   summary.cmake      - Build configuration summary output
##################################################

include(cmake/options.cmake)
include(cmake/compiler.cmake)
include(cmake/dependencies.cmake)
include(cmake/targets.cmake)
include(cmake/install.cmake)
include(cmake/testing.cmake)
include(cmake/examples.cmake)
include(cmake/warnings.cmake)
include(cmake/summary.cmake)
