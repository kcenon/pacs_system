# Query SCU Sample
# DICOM C-FIND Client demonstrating Query/Retrieve SCU

add_executable(query_scu
    main.cpp
)

target_include_directories(query_scu
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(query_scu
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_integration
)

# Set C++20 standard
target_compile_features(query_scu PRIVATE cxx_std_20)

# Install target
install(TARGETS query_scu
    RUNTIME DESTINATION bin
)
