# XML to DCM
# XML to DICOM conversion utility (DICOM Native XML PS3.19)

add_executable(xml_to_dcm
    main.cpp
)

target_include_directories(xml_to_dcm
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
# Note: xml_to_dcm only needs core and encoding - no network dependencies
target_link_libraries(xml_to_dcm
    PRIVATE
        pacs_core
        pacs_encoding
)

# Set C++20 standard
target_compile_features(xml_to_dcm PRIVATE cxx_std_20)

# Install target
install(TARGETS xml_to_dcm
    RUNTIME DESTINATION bin
)
