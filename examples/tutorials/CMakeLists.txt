# examples/CMakeLists.txt
# Root CMake configuration for developer tutorials (formerly samples/, see #1139)
#
# This directory contains progressive tutorials demonstrating PACS system
# features from basic DICOM file operations through a production-ready PACS.

cmake_minimum_required(VERSION 3.20)

# Common utilities library (used by all tutorials)
add_subdirectory(common)

# Tutorial applications
add_subdirectory(01_hello_dicom)       # Level 1: Basic DICOM file operations
add_subdirectory(02_echo_server)       # Level 2: C-ECHO SCP/SCU
add_subdirectory(03_storage_server)    # Level 3: C-STORE SCP/SCU
add_subdirectory(04_mini_pacs)         # Level 4: Full PACS with query/retrieve
add_subdirectory(05_production_pacs)   # Level 5: Production-ready features
