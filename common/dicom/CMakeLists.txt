cmake_minimum_required(VERSION 3.16)

# Create the DICOM abstraction library
add_library(pacs_dicom
    dicom_object.cpp
    dicom_tag.cpp
    dicom_file.cpp
    dicom_error.cpp
    storage_client.cpp
    storage_server.cpp
    codec_manager.cpp
)

# Set include directories
target_include_directories(pacs_dicom
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${DCMTK_INCLUDE_DIRS}
)

# Link against DCMTK and other dependencies
target_link_libraries(pacs_dicom
    PUBLIC
    pacs_common
    ${DCMTK_LIBRARIES}
    priority_thread_pool_module
    logger_module
)

# Set compilation options
target_compile_features(pacs_dicom PUBLIC cxx_std_20)

# Add DCMTK preprocessor definitions
target_compile_definitions(pacs_dicom PRIVATE ${DCMTK_DEFINITIONS})

# Install the library
install(TARGETS pacs_dicom
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install headers
install(FILES
    dicom_object.h
    dicom_tag.h
    dicom_file.h
    dicom_error.h
    storage_client.h
    storage_server.h
    codec_manager.h
    DESTINATION include/pacs/common/dicom
)