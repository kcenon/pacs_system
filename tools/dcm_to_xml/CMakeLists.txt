# DCM to XML
# DICOM to XML conversion utility (DICOM Native XML PS3.19)

add_executable(dcm_to_xml
    main.cpp
)

target_include_directories(dcm_to_xml
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
# Note: dcm_to_xml only needs core and encoding - no network dependencies
target_link_libraries(dcm_to_xml
    PRIVATE
        pacs_core
        pacs_encoding
)

# Set C++20 standard
target_compile_features(dcm_to_xml PRIVATE cxx_std_20)

# Install target
install(TARGETS dcm_to_xml
    RUNTIME DESTINATION bin
)
