# DCM Info Sample
# DICOM file summary utility

add_executable(dcm_info
    main.cpp
)

target_include_directories(dcm_info
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
# Note: dcm_info only needs core and encoding - no network dependencies
target_link_libraries(dcm_info
    PRIVATE
        pacs_core
        pacs_encoding
)

# Set C++20 standard
target_compile_features(dcm_info PRIVATE cxx_std_20)

# Install target
install(TARGETS dcm_info
    RUNTIME DESTINATION bin
)
