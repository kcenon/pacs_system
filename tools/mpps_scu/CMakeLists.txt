# MPPS SCU Sample
# Modality Performed Procedure Step Client demonstrating N-CREATE and N-SET

add_executable(mpps_scu
    main.cpp
)

target_include_directories(mpps_scu
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(mpps_scu
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_integration
)

# Set C++20 standard
target_compile_features(mpps_scu PRIVATE cxx_std_20)

# Install target
install(TARGETS mpps_scu
    RUNTIME DESTINATION bin
)
