# samples/04_mini_pacs/CMakeLists.txt
# Level 4: Mini PACS - Complete DICOM server with all services
#
# This sample demonstrates integrating all DICOM services:
# - Verification SCP (C-ECHO)
# - Storage SCP (C-STORE)
# - Query SCP (C-FIND)
# - Retrieve SCP (C-MOVE/C-GET)
# - Modality Worklist SCP (MWL)
# - MPPS SCP

cmake_minimum_required(VERSION 3.20)

# Executable name
set(SAMPLE_NAME mini_pacs)

# Source files
set(SAMPLE_SOURCES
    main.cpp
    mini_pacs.cpp
)

# Header files (for IDE visibility)
set(SAMPLE_HEADERS
    mini_pacs.hpp
)

# Create executable
add_executable(${SAMPLE_NAME}
    ${SAMPLE_SOURCES}
    ${SAMPLE_HEADERS}
)

# Include directories
target_include_directories(${SAMPLE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# Link dependencies
target_link_libraries(${SAMPLE_NAME} PRIVATE
    pacs_core
    pacs_encoding
    pacs_network
    pacs_services
    pacs_storage
    pacs_samples_common
)

# C++ standard
target_compile_features(${SAMPLE_NAME} PRIVATE cxx_std_20)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${SAMPLE_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
elseif(MSVC)
    target_compile_options(${SAMPLE_NAME} PRIVATE
        /W4
    )
endif()

# Set output directory
set_target_properties(${SAMPLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/samples"
)
