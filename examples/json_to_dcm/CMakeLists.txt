# JSON to DCM
# JSON to DICOM conversion utility (DICOM PS3.18)

add_executable(json_to_dcm
    main.cpp
)

target_include_directories(json_to_dcm
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
# Note: json_to_dcm only needs core and encoding - no network dependencies
target_link_libraries(json_to_dcm
    PRIVATE
        pacs_core
        pacs_encoding
)

# Set C++20 standard
target_compile_features(json_to_dcm PRIVATE cxx_std_20)

# Install target
install(TARGETS json_to_dcm
    RUNTIME DESTINATION bin
)
