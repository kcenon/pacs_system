# Image to DICOM Conversion Utility
# Converts JPEG/PNG images to DICOM format

add_executable(img_to_dcm
    main.cpp
)

target_include_directories(img_to_dcm
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(img_to_dcm
    PRIVATE
        pacs_core
        pacs_encoding
)

# Add JPEG support if available
if(PACS_JPEG_FOUND)
    target_compile_definitions(img_to_dcm PRIVATE PACS_JPEG_FOUND)
    target_link_libraries(img_to_dcm PRIVATE ${PACS_JPEG_TARGET})
endif()

# Set C++20 standard
target_compile_features(img_to_dcm PRIVATE cxx_std_20)

# Install target
install(TARGETS img_to_dcm
    RUNTIME DESTINATION bin
)
