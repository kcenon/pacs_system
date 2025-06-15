# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create security library
add_library(pacs_security
    crypto_manager.cpp
    dcmtk_tls_adapter.cpp
    security_manager.cpp
    tls_config.cpp
)

# Add alias
add_library(pacs::security ALIAS pacs_security)

# Configure include directories
target_include_directories(pacs_security
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find packages
find_package(OpenSSL QUIET)
find_package(DCMTK QUIET)
find_package(cryptopp QUIET)
find_package(nlohmann_json QUIET)

# Link libraries
target_link_libraries(pacs_security
    PUBLIC
        pacs::logger
        pacs::common
        pacs::config
)

if(OpenSSL_FOUND)
    target_link_libraries(pacs_security PUBLIC OpenSSL::SSL OpenSSL::Crypto)
else()
    target_compile_definitions(pacs_security PUBLIC NO_OPENSSL)
endif()

if(DCMTK_FOUND)
    target_link_libraries(pacs_security PUBLIC DCMTK::DCMTK)
else()
    target_compile_definitions(pacs_security PUBLIC USE_DCMTK_PLACEHOLDER)
endif()

if(cryptopp_FOUND)
    target_link_libraries(pacs_security PUBLIC cryptopp::cryptopp)
    target_compile_definitions(pacs_security PUBLIC HAVE_CRYPTOPP)
    message(STATUS "CryptoPP found - secure password hashing enabled")
else()
    target_compile_definitions(pacs_security PUBLIC NO_CRYPTOPP)
    message(STATUS "CryptoPP not found - using insecure password hashing for testing only")
endif()

if(nlohmann_json_FOUND)
    target_link_libraries(pacs_security PUBLIC nlohmann_json::nlohmann_json)
    target_compile_definitions(pacs_security PUBLIC HAS_NLOHMANN_JSON)
else()
    target_compile_definitions(pacs_security PUBLIC NO_NLOHMANN_JSON)
endif()

# Set compile features
target_compile_features(pacs_security PUBLIC cxx_std_20)

# Install library
install(TARGETS pacs_security
    EXPORT pacs_targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(FILES
    crypto_manager.h
    dcmtk_tls_adapter.h
    security_manager.h
    tls_config.h
    DESTINATION include/pacs/common/security
)