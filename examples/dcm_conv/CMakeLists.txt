# DCM Conv Sample
# DICOM Transfer Syntax conversion utility

add_executable(dcm_conv
    main.cpp
)

target_include_directories(dcm_conv
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(dcm_conv
    PRIVATE
        pacs_core
        pacs_encoding
)

# Set C++20 standard
target_compile_features(dcm_conv PRIVATE cxx_std_20)

# Install target
install(TARGETS dcm_conv
    RUNTIME DESTINATION bin
)
