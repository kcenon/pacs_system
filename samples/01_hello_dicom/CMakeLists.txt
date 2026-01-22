# samples/01_hello_dicom/CMakeLists.txt
# Level 1: Hello DICOM - Introduction to DICOM fundamentals

add_executable(sample_01_hello_dicom
    main.cpp
)

target_link_libraries(sample_01_hello_dicom PRIVATE
    pacs_core
    pacs_encoding
    pacs_samples_common
)

# Require C++20
target_compile_features(sample_01_hello_dicom PRIVATE cxx_std_20)

# Set output properties
set_target_properties(sample_01_hello_dicom PROPERTIES
    OUTPUT_NAME "hello_dicom"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/samples"
)

# Add alias for consistent naming
add_executable(pacs::samples::hello_dicom ALIAS sample_01_hello_dicom)
