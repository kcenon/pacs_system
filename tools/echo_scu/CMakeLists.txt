# Echo SCU Sample
# DICOM Connectivity Test Client demonstrating C-ECHO SCU

add_executable(echo_scu
    main.cpp
)

target_include_directories(echo_scu
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(echo_scu
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_integration
)

# Set C++20 standard
target_compile_features(echo_scu PRIVATE cxx_std_20)

# Install target
install(TARGETS echo_scu
    RUNTIME DESTINATION bin
)
