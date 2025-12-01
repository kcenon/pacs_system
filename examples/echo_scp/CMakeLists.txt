# Echo SCP Sample
# DICOM Connectivity Test Server demonstrating C-ECHO SCP

add_executable(echo_scp
    main.cpp
)

target_include_directories(echo_scp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(echo_scp
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_integration
)

# Set C++20 standard
target_compile_features(echo_scp PRIVATE cxx_std_20)

# Install target
install(TARGETS echo_scp
    RUNTIME DESTINATION bin
)
