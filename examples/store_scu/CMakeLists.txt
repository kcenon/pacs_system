# Store SCU Sample
# DICOM Image Sender demonstrating C-STORE SCU

add_executable(store_scu
    main.cpp
)

target_include_directories(store_scu
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(store_scu
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_integration
)

# Set C++20 standard
target_compile_features(store_scu PRIVATE cxx_std_20)

# Install target
install(TARGETS store_scu
    RUNTIME DESTINATION bin
)
