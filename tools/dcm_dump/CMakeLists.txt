# DCM Dump Sample
# DICOM file inspection utility

add_executable(dcm_dump
    main.cpp
)

target_include_directories(dcm_dump
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
# Note: dcm_dump only needs core and encoding - no network dependencies
target_link_libraries(dcm_dump
    PRIVATE
        pacs_core
        pacs_encoding
)

# Set C++20 standard
target_compile_features(dcm_dump PRIVATE cxx_std_20)

# Install target
install(TARGETS dcm_dump
    RUNTIME DESTINATION bin
)
