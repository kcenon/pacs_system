# Secure DICOM Sample
# TLS-secured DICOM communication demonstrating C-ECHO over TLS 1.2/1.3
#
# See Issue #110 - TLS DICOM Connection Sample

# ============================================================================
# Secure Echo SCP - TLS Server
# ============================================================================

add_executable(secure_echo_scp
    secure_echo_scp.cpp
)

target_include_directories(secure_echo_scp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(secure_echo_scp
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_integration
)

# Set C++20 standard
target_compile_features(secure_echo_scp PRIVATE cxx_std_20)

# Install target
install(TARGETS secure_echo_scp
    RUNTIME DESTINATION bin
)

# ============================================================================
# Secure Echo SCU - TLS Client
# ============================================================================

add_executable(secure_echo_scu
    secure_echo_scu.cpp
)

target_include_directories(secure_echo_scu
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(secure_echo_scu
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_integration
)

# Set C++20 standard
target_compile_features(secure_echo_scu PRIVATE cxx_std_20)

# Install target
install(TARGETS secure_echo_scu
    RUNTIME DESTINATION bin
)

# ============================================================================
# Install certificate generation script
# ============================================================================

install(PROGRAMS generate_certs.sh
    DESTINATION bin
    RENAME generate_dicom_certs.sh
)
