# DCM Anonymize Sample
# DICOM de-identification/anonymization utility

add_executable(dcm_anonymize
    main.cpp
)

target_include_directories(dcm_anonymize
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(dcm_anonymize
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_security
)

# Set C++20 standard
target_compile_features(dcm_anonymize PRIVATE cxx_std_20)

# Install target
install(TARGETS dcm_anonymize
    RUNTIME DESTINATION bin
)
