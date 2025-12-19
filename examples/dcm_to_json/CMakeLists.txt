# DCM to JSON
# DICOM to JSON conversion utility (DICOM PS3.18)

add_executable(dcm_to_json
    main.cpp
)

target_include_directories(dcm_to_json
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
# Note: dcm_to_json only needs core and encoding - no network dependencies
target_link_libraries(dcm_to_json
    PRIVATE
        pacs_core
        pacs_encoding
)

# Set C++20 standard
target_compile_features(dcm_to_json PRIVATE cxx_std_20)

# Install target
install(TARGETS dcm_to_json
    RUNTIME DESTINATION bin
)
