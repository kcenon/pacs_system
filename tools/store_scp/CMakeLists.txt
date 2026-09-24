# Storage SCP Sample
# DICOM Image Receiver Server demonstrating C-STORE SCP

add_executable(store_scp
    main.cpp
)

target_include_directories(store_scp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(store_scp
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_storage
        pacs_integration
)

# Set C++20 standard
target_compile_features(store_scp PRIVATE cxx_std_20)

# Install target
install(TARGETS store_scp
    RUNTIME DESTINATION bin
)
