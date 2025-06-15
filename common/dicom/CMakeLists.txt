# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create dicom library
add_library(pacs_dicom
    codec_manager.cpp
    dicom_error.cpp
    dicom_file.cpp
    dicom_object.cpp
    dicom_tag.cpp
    storage_client.cpp
    storage_server.cpp
)

# Add alias
add_library(pacs::dicom ALIAS pacs_dicom)

# Configure include directories
target_include_directories(pacs_dicom
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find packages
find_package(DCMTK QUIET)

# Link libraries
target_link_libraries(pacs_dicom
    PUBLIC
        pacs::logger
        pacs::security
)

if(DCMTK_FOUND AND TARGET DCMTK::DCMTK)
    target_link_libraries(pacs_dicom PUBLIC DCMTK::DCMTK)
else()
    target_compile_definitions(pacs_dicom PUBLIC USE_DCMTK_PLACEHOLDER)
endif()

# Set compile features
target_compile_features(pacs_dicom PUBLIC cxx_std_20)

# Install library
install(TARGETS pacs_dicom
    EXPORT pacs_targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(FILES
    codec_manager.h
    dicom_error.h
    dicom_file.h
    dicom_object.h
    dicom_tag.h
    storage_client.h
    storage_server.h
    DESTINATION include/pacs/common/dicom
)