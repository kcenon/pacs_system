cmake_minimum_required(VERSION 3.16)

# Add subdirectories
add_subdirectory(config)
add_subdirectory(dicom)  # Always build dicom module (uses placeholder if DCMTK not found)
add_subdirectory(logger)
add_subdirectory(security)
add_subdirectory(audit)
add_subdirectory(storage)
add_subdirectory(network)
add_subdirectory(version)
add_subdirectory(monitoring)

set(COMMON_SOURCE_FILES
    dicom_util.cpp
    dicom_util.h
    pacs_common.h
    service_config.cpp
    service_config.h
    performance_standalone.cpp
    performance.h
)

add_library(pacs_common STATIC ${COMMON_SOURCE_FILES})
add_library(pacs::common ALIAS pacs_common)
target_include_directories(pacs_common PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Explicitly tell CMake to only include C++ standard library once
set_target_properties(pacs_common PROPERTIES 
    LINK_FLAGS_DEBUG ""
    LINK_FLAGS_RELEASE ""
)

# DCMTK dependency (only when available)
if(NOT USE_DCMTK_PLACEHOLDER)
    find_package(DCMTK QUIET)
    if(DCMTK_FOUND AND TARGET DCMTK::DCMTK)
        target_link_libraries(pacs_common PUBLIC DCMTK::DCMTK)
    else()
        target_compile_definitions(pacs_common PUBLIC USE_DCMTK_PLACEHOLDER)
    endif()
else()
    target_compile_definitions(pacs_common PUBLIC USE_DCMTK_PLACEHOLDER)
endif()