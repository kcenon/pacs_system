# DCM Modify Sample
# DICOM tag modification and anonymization utility

add_executable(dcm_modify
    main.cpp
)

target_include_directories(dcm_modify
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
# The compatibility anonymizer delegates to pacs_security (no network dependency).
target_link_libraries(dcm_modify
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_security
)

# Set C++20 standard
target_compile_features(dcm_modify PRIVATE cxx_std_20)

# Install target
install(TARGETS dcm_modify
    RUNTIME DESTINATION bin
)
