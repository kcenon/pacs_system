# samples/03_storage_server/CMakeLists.txt
# Level 3: Storage Server - DICOM image reception and archiving

add_executable(sample_03_storage_server
    main.cpp
)

target_link_libraries(sample_03_storage_server PRIVATE
    pacs_network
    pacs_services
    pacs_storage
    pacs_samples_common
)

# Require C++20
target_compile_features(sample_03_storage_server PRIVATE cxx_std_20)

# Set output properties
set_target_properties(sample_03_storage_server PROPERTIES
    OUTPUT_NAME "storage_server"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/samples"
)

# Add alias for consistent naming
add_executable(pacs::samples::storage_server ALIAS sample_03_storage_server)
