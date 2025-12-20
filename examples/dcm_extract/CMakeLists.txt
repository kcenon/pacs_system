# DICOM Extract - Pixel Data Extraction Utility
# Extracts pixel data from DICOM files to standard image formats

add_executable(dcm_extract
    main.cpp
)

target_include_directories(dcm_extract
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(dcm_extract
    PRIVATE
        pacs_core
        pacs_encoding
)

# Add JPEG support if available
if(PACS_JPEG_FOUND)
    target_compile_definitions(dcm_extract PRIVATE PACS_JPEG_FOUND)
    target_link_libraries(dcm_extract PRIVATE ${PACS_JPEG_TARGET})
endif()

# Add PNG support if available
if(PACS_PNG_FOUND)
    target_compile_definitions(dcm_extract PRIVATE PACS_PNG_FOUND)
    target_link_libraries(dcm_extract PRIVATE ${PACS_PNG_TARGET})
endif()

# Set C++20 standard
target_compile_features(dcm_extract PRIVATE cxx_std_20)

# Install target
install(TARGETS dcm_extract
    RUNTIME DESTINATION bin
)
