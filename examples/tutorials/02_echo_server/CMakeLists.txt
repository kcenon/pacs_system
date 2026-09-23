# samples/02_echo_server/CMakeLists.txt
# Level 2: Echo Server - DICOM network communication fundamentals

add_executable(sample_02_echo_server
    main.cpp
)

target_link_libraries(sample_02_echo_server PRIVATE
    pacs_network
    pacs_services
    pacs_samples_common
)

# Require C++20
target_compile_features(sample_02_echo_server PRIVATE cxx_std_20)

# Set output properties
set_target_properties(sample_02_echo_server PROPERTIES
    OUTPUT_NAME "echo_server"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/samples"
)

# Add alias for consistent naming
add_executable(pacs::samples::echo_server ALIAS sample_02_echo_server)
