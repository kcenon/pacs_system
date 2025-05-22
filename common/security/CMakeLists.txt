cmake_minimum_required(VERSION 3.16)

# Define source files
set(SECURITY_SOURCE_FILES
    tls_config.cpp
    security_manager.cpp
    dcmtk_tls_adapter.cpp
)

add_library(pacs_security ${SECURITY_SOURCE_FILES})

target_include_directories(pacs_security PUBLIC 
    ${CMAKE_SOURCE_DIR}
)

# Find required dependencies
find_package(nlohmann_json CONFIG REQUIRED)

# Find CryptoPP (required)
find_package(cryptopp CONFIG REQUIRED)
target_compile_definitions(pacs_security PUBLIC HAVE_CRYPTOPP)

# Basic dependencies
target_link_libraries(pacs_security PUBLIC 
    pacs_common
    pacs_config
    pacs_logger
    nlohmann_json::nlohmann_json
)

# Link CryptoPP
target_link_libraries(pacs_security PUBLIC cryptopp::cryptopp)

# DCMTK dependency (only when available)
if(NOT USE_DCMTK_PLACEHOLDER)
    find_package(DCMTK REQUIRED)
    target_link_libraries(pacs_security PUBLIC ${DCMTK_LIBRARIES})
    target_include_directories(pacs_security PUBLIC ${DCMTK_INCLUDE_DIRS})
    
    # Check if DCMTK has OpenSSL support
    include(CheckCXXSymbolExists)
    set(CMAKE_REQUIRED_INCLUDES ${DCMTK_INCLUDE_DIRS})
    set(CMAKE_REQUIRED_LIBRARIES ${DCMTK_LIBRARIES})
    check_cxx_symbol_exists("DCMTK_WITH_OPENSSL" "dcmtk/config/osconfig.h" DCMTK_HAS_OPENSSL)
    
    if(DCMTK_HAS_OPENSSL)
        target_compile_definitions(pacs_security PUBLIC DCMTK_WITH_OPENSSL)
        message(STATUS "DCMTK has OpenSSL support: YES")
    else()
        message(STATUS "DCMTK has OpenSSL support: NO")
    endif()
endif()