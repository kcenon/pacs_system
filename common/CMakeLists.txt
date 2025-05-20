cmake_minimum_required(VERSION 3.16)

# Add subdirectories
add_subdirectory(config)
add_subdirectory(dicom)
add_subdirectory(logger)
add_subdirectory(security)

set(COMMON_SOURCE_FILES
    dicom_util.cpp
    dicom_util.h
    pacs_common.h
    service_config.cpp
    service_config.h
    performance.cpp
    performance.h
)

add_library(pacs_common STATIC ${COMMON_SOURCE_FILES})
target_include_directories(pacs_common PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

# DCMTK dependency (only when available)
if(NOT USE_DCMTK_PLACEHOLDER)
    find_package(DCMTK REQUIRED)
    target_link_libraries(pacs_common PUBLIC ${DCMTK_LIBRARIES})
    target_include_directories(pacs_common PUBLIC ${DCMTK_INCLUDE_DIRS})
endif()