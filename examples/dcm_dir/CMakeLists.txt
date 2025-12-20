# DCM Dir Sample
# DICOMDIR creation and management utility

add_executable(dcm_dir
    main.cpp
)

target_include_directories(dcm_dir
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
# Note: dcm_dir only needs core and encoding - no network dependencies
target_link_libraries(dcm_dir
    PRIVATE
        pacs_core
        pacs_encoding
)

# Set C++20 standard
target_compile_features(dcm_dir PRIVATE cxx_std_20)

# Install target
install(TARGETS dcm_dir
    RUNTIME DESTINATION bin
)
