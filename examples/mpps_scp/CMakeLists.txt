# MPPS SCP Sample
# DICOM MPPS Server for receiving procedure status updates

add_executable(mpps_scp
    main.cpp
)

target_include_directories(mpps_scp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(mpps_scp
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_integration
)

# Set C++20 standard
target_compile_features(mpps_scp PRIVATE cxx_std_20)

# Install target
install(TARGETS mpps_scp
    RUNTIME DESTINATION bin
)
