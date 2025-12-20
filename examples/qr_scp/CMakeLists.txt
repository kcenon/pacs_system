# Query/Retrieve SCP Sample
# DICOM Query/Retrieve Server demonstrating C-FIND, C-MOVE, and C-GET SCP

add_executable(qr_scp
    main.cpp
)

target_include_directories(qr_scp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(qr_scp
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_storage
        pacs_integration
)

# Set C++20 standard
target_compile_features(qr_scp PRIVATE cxx_std_20)

# Install target
install(TARGETS qr_scp
    RUNTIME DESTINATION bin
)
