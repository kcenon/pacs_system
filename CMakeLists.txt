cmake_minimum_required(VERSION 3.20)

project(pacs_system
    VERSION 0.2.0
    DESCRIPTION "Production-ready C++20 PACS implementation"
    LANGUAGES CXX
)

##################################################
# C++ Standard and Build Configuration
##################################################

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build settings
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

##################################################
# Dependency Chain Documentation (Option A)
#
# Tier 5: pacs_system
#   Required (Direct):
#     - common_system (Tier 0) - Result<T> pattern
#     - container_system (Tier 1) - DICOM serialization
#     - network_system (Tier 4) - DICOM PDU/Association
#
#   Auto-included via network_system:
#     - thread_system (Tier 1)
#     - logger_system (Tier 2)
#
#   Optional:
#     - monitoring_system (Tier 3) - via network_system config
##################################################

##################################################
# Options
##################################################

option(PACS_BUILD_TESTS "Build unit tests" ON)
option(PACS_BUILD_EXAMPLES "Build examples" OFF)
option(PACS_BUILD_BENCHMARKS "Build benchmarks" OFF)

# Dependency integration options
option(PACS_WITH_COMMON_SYSTEM "Enable common_system integration (REQUIRED)" ON)
option(PACS_WITH_CONTAINER_SYSTEM "Enable container_system integration (REQUIRED)" ON)
option(PACS_WITH_NETWORK_SYSTEM "Enable network_system integration (REQUIRED)" ON)
option(PACS_BUILD_STORAGE "Build storage module (requires SQLite3)" ON)

##################################################
# Compiler Warnings
##################################################

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W4 /WX)
endif()

##################################################
# Required Dependencies (Tier 0-4)
##################################################

include(FetchContent)
find_package(Threads REQUIRED)

# SQLite3 (for storage module)
if(PACS_BUILD_STORAGE)
    message(STATUS "")
    message(STATUS "=== Finding SQLite3 (for storage module) ===")

    # Try system SQLite3 first
    find_package(SQLite3 QUIET)

    if(SQLite3_FOUND)
        message(STATUS "Found system SQLite3: ${SQLite3_VERSION}")
        set(SQLITE3_FOUND TRUE)
    else()
        # Fallback to FetchContent
        message(STATUS "System SQLite3 not found, fetching from source...")

        FetchContent_Declare(
            sqlite3
            URL https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip
            URL_HASH SHA3_256=72887d57a1d5c9ff937e59efc8db186c5c871fae9e2e5a9b20a1f2c7b5f1e8f7
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(sqlite3)

        # Create SQLite3 library target
        add_library(sqlite3_lib STATIC
            ${sqlite3_SOURCE_DIR}/sqlite3.c
        )
        target_include_directories(sqlite3_lib PUBLIC ${sqlite3_SOURCE_DIR})
        target_compile_definitions(sqlite3_lib PRIVATE
            SQLITE_THREADSAFE=1
            SQLITE_ENABLE_FTS5
            SQLITE_ENABLE_JSON1
        )
        set(SQLITE3_FOUND TRUE)
        set(SQLITE3_FROM_FETCH TRUE)
        message(STATUS "SQLite3 fetched and configured")
    endif()
endif()

# common_system (REQUIRED - Tier 0)
if(PACS_WITH_COMMON_SYSTEM)
    message(STATUS "")
    message(STATUS "=== Finding common_system (REQUIRED - Tier 0) ===")

    set(_PACS_COMMON_PATHS
        "$ENV{COMMON_SYSTEM_ROOT}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common_system/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/common_system/include"
    )

    set(COMMON_SYSTEM_FOUND FALSE)
    foreach(_path ${_PACS_COMMON_PATHS})
        if(EXISTS "${_path}/kcenon/common/patterns/result.h")
            message(STATUS "Found common_system at: ${_path}")
            set(PACS_COMMON_SYSTEM_INCLUDE_DIR "${_path}")
            set(COMMON_SYSTEM_FOUND TRUE)
            break()
        endif()
    endforeach()

    if(NOT COMMON_SYSTEM_FOUND)
        message(FATAL_ERROR
            "common_system is REQUIRED (Tier 0).\n"
            "Could not find 'kcenon/common/patterns/result.h'.\n"
            "Please ensure common_system is available at:\n"
            "  - ../common_system/include\n"
            "  - Set COMMON_SYSTEM_ROOT environment variable")
    endif()
endif()

# container_system (REQUIRED - Tier 1)
if(PACS_WITH_CONTAINER_SYSTEM)
    message(STATUS "")
    message(STATUS "=== Finding container_system (REQUIRED - Tier 1) ===")

    set(_PACS_CONTAINER_PATHS
        "$ENV{CONTAINER_SYSTEM_ROOT}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../container_system"
        "${CMAKE_CURRENT_SOURCE_DIR}/container_system"
    )

    set(CONTAINER_SYSTEM_FOUND FALSE)
    foreach(_path ${_PACS_CONTAINER_PATHS})
        if(EXISTS "${_path}/CMakeLists.txt" AND EXISTS "${_path}/container.h")
            message(STATUS "Found container_system at: ${_path}")
            set(PACS_CONTAINER_SYSTEM_DIR "${_path}")
            set(CONTAINER_SYSTEM_FOUND TRUE)
            break()
        endif()
    endforeach()

    if(CONTAINER_SYSTEM_FOUND AND NOT TARGET container_system)
        set(BUILD_CONTAINERSYSTEM_AS_SUBMODULE ON CACHE BOOL "" FORCE)
        set(BUILD_CONTAINER_SAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(CONTAINER_BUILD_INTEGRATION_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory("${PACS_CONTAINER_SYSTEM_DIR}" "${CMAKE_BINARY_DIR}/container_system_build")
    elseif(NOT CONTAINER_SYSTEM_FOUND AND NOT TARGET container_system)
        message(FATAL_ERROR
            "container_system is REQUIRED (Tier 1) for DICOM serialization.\n"
            "Please ensure container_system is available at:\n"
            "  - ../container_system\n"
            "  - Set CONTAINER_SYSTEM_ROOT environment variable")
    endif()
endif()

# network_system (REQUIRED - Tier 4)
if(PACS_WITH_NETWORK_SYSTEM)
    message(STATUS "")
    message(STATUS "=== Finding network_system (REQUIRED - Tier 4) ===")

    set(_PACS_NETWORK_PATHS
        "$ENV{NETWORK_SYSTEM_ROOT}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../network_system"
        "${CMAKE_CURRENT_SOURCE_DIR}/network_system"
    )

    set(NETWORK_SYSTEM_FOUND FALSE)
    foreach(_path ${_PACS_NETWORK_PATHS})
        if(EXISTS "${_path}/CMakeLists.txt" AND EXISTS "${_path}/include/network_system/network_system.h")
            message(STATUS "Found network_system at: ${_path}")
            set(PACS_NETWORK_SYSTEM_DIR "${_path}")
            set(NETWORK_SYSTEM_FOUND TRUE)
            break()
        endif()
    endforeach()

    if(NETWORK_SYSTEM_FOUND AND NOT TARGET NetworkSystem)
        set(BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(NETWORK_BUILD_INTEGRATION_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory("${PACS_NETWORK_SYSTEM_DIR}" "${CMAKE_BINARY_DIR}/network_system_build")
    elseif(NOT NETWORK_SYSTEM_FOUND AND NOT TARGET NetworkSystem)
        message(FATAL_ERROR
            "network_system is REQUIRED (Tier 4) for DICOM PDU.\n"
            "\n"
            "Dependency chain:\n"
            "  pacs_system (Tier 5)\n"
            "    └── network_system (Tier 4) <- MISSING\n"
            "          └── logger_system (Tier 2)\n"
            "                └── thread_system (Tier 1)\n"
            "                      └── common_system (Tier 0)\n"
            "\n"
            "Please ensure network_system is available at:\n"
            "  - ../network_system\n"
            "  - Set NETWORK_SYSTEM_ROOT environment variable")
    endif()
endif()

##################################################
# Dependency Chain Validation
##################################################

message(STATUS "")
message(STATUS "=== PACS System Dependency Chain Validation ===")
message(STATUS "")
if(COMMON_SYSTEM_FOUND)
    message(STATUS "  [OK] Tier 0: common_system [REQUIRED]")
else()
    message(STATUS "  [--] Tier 0: common_system [SKIPPED]")
endif()
if(TARGET container_system)
    message(STATUS "  [OK] Tier 1: container_system [REQUIRED]")
else()
    message(STATUS "  [--] Tier 1: container_system [SKIPPED]")
endif()
message(STATUS "  [AUTO] Tier 1: thread_system [via network_system]")
message(STATUS "  [AUTO] Tier 2: logger_system [via network_system]")
message(STATUS "  [OPT] Tier 3: monitoring_system [via network_system config]")
if(TARGET NetworkSystem)
    message(STATUS "  [OK] Tier 4: network_system [REQUIRED]")
else()
    message(STATUS "  [--] Tier 4: network_system [SKIPPED]")
endif()
message(STATUS "  [OK] Tier 5: pacs_system")
message(STATUS "")
message(STATUS "=== Dependency Chain: VALID ===")
message(STATUS "")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

##################################################
# PACS Libraries
##################################################

# Core library
add_library(pacs_core
    src/core/dicom_tag.cpp
    src/core/dicom_element.cpp
    src/core/dicom_dataset.cpp
    src/core/dicom_file.cpp
    src/core/tag_info.cpp
    src/core/dicom_dictionary.cpp
    src/core/standard_tags_data.cpp
)
target_include_directories(pacs_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link common_system (Tier 0) - Result<T> pattern
if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(pacs_core PUBLIC ${PACS_COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(pacs_core PUBLIC PACS_WITH_COMMON_SYSTEM)
endif()

# Link container_system (Tier 1) - DICOM serialization
if(TARGET container_system)
    target_link_libraries(pacs_core PUBLIC container_system)
    target_compile_definitions(pacs_core PUBLIC PACS_WITH_CONTAINER_SYSTEM)
endif()

# Encoding library
add_library(pacs_encoding
    src/encoding/transfer_syntax.cpp
    src/encoding/vr_info.cpp
    src/encoding/implicit_vr_codec.cpp
    src/encoding/explicit_vr_codec.cpp
)
target_include_directories(pacs_encoding
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(pacs_encoding
    PUBLIC pacs_core
)

# Network library
add_library(pacs_network
    src/network/pdu_encoder.cpp
    src/network/pdu_decoder.cpp
    src/network/dimse/dimse_message.cpp
)
target_include_directories(pacs_network
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(pacs_network
    PUBLIC pacs_core pacs_encoding
)

# Link network_system (Tier 4) - DICOM PDU/Association
if(TARGET NetworkSystem)
    target_link_libraries(pacs_network PUBLIC NetworkSystem)
    target_compile_definitions(pacs_network PUBLIC PACS_WITH_NETWORK_SYSTEM)
endif()

# Storage library
if(PACS_BUILD_STORAGE AND SQLITE3_FOUND)
    add_library(pacs_storage
        src/storage/migration_runner.cpp
        src/storage/index_database.cpp
    )
    target_include_directories(pacs_storage
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    # Link common_system for Result<T>
    if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(pacs_storage PUBLIC ${PACS_COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(pacs_storage PUBLIC PACS_WITH_COMMON_SYSTEM)
    endif()

    # Link SQLite3
    if(SQLITE3_FROM_FETCH)
        target_link_libraries(pacs_storage PUBLIC sqlite3_lib)
    else()
        target_link_libraries(pacs_storage PUBLIC SQLite::SQLite3)
    endif()

    message(STATUS "  [OK] pacs_storage: ON (SQLite3)")
else()
    message(STATUS "  [--] pacs_storage: OFF")
endif()

# Testing
if(PACS_BUILD_TESTS)
    enable_testing()

    # Fetch Catch2 for testing
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)

    # Create tests directory if it doesn't exist
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/core)
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/encoding)
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/network)
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/storage)

    # Core tests
    add_executable(core_tests
        tests/core/dicom_tag_test.cpp
        tests/core/dicom_element_test.cpp
        tests/core/dicom_dataset_test.cpp
        tests/core/dicom_file_test.cpp
        tests/core/tag_info_test.cpp
        tests/core/dicom_dictionary_test.cpp
    )
    target_link_libraries(core_tests
        PRIVATE
            pacs_core
            pacs_encoding
            Catch2::Catch2WithMain
    )

    # Encoding tests
    add_executable(encoding_tests
        tests/encoding/transfer_syntax_test.cpp
        tests/encoding/vr_type_test.cpp
        tests/encoding/vr_info_test.cpp
        tests/encoding/implicit_vr_codec_test.cpp
        tests/encoding/explicit_vr_codec_test.cpp
    )
    target_link_libraries(encoding_tests
        PRIVATE
            pacs_encoding
            Catch2::Catch2WithMain
    )

    # Network tests
    add_executable(network_tests
        tests/network/pdu_encoder_test.cpp
        tests/network/pdu_decoder_test.cpp
        tests/network/dimse/dimse_message_test.cpp
    )
    target_link_libraries(network_tests
        PRIVATE
            pacs_network
            Catch2::Catch2WithMain
    )

    # Storage tests
    if(TARGET pacs_storage)
        add_executable(storage_tests
            tests/storage/migration_runner_test.cpp
            tests/storage/index_database_test.cpp
        )
        target_link_libraries(storage_tests
            PRIVATE
                pacs_storage
                Catch2::Catch2WithMain
        )
    endif()

    include(CTest)
    include(Catch)
    catch_discover_tests(core_tests)
    catch_discover_tests(encoding_tests)
    catch_discover_tests(network_tests)
    if(TARGET storage_tests)
        catch_discover_tests(storage_tests)
    endif()
endif()

##################################################
# Build Summary
##################################################

message(STATUS "")
message(STATUS "========================================")
message(STATUS "PACS System v${PROJECT_VERSION}")
message(STATUS "========================================")
message(STATUS "")
message(STATUS "Dependency Chain (Tier 5):")
message(STATUS "  pacs_system")
if(COMMON_SYSTEM_FOUND)
    message(STATUS "    ├── common_system (Tier 0): ON")
else()
    message(STATUS "    ├── common_system (Tier 0): OFF")
endif()
if(TARGET container_system)
    message(STATUS "    ├── container_system (Tier 1): ON")
else()
    message(STATUS "    ├── container_system (Tier 1): OFF")
endif()
if(TARGET NetworkSystem)
    message(STATUS "    └── network_system (Tier 4): ON")
    message(STATUS "          ├── logger_system (Tier 2): [AUTO]")
    message(STATUS "          │     └── thread_system (Tier 1): [AUTO]")
    message(STATUS "          └── monitoring_system (Tier 3): [OPTIONAL]")
else()
    message(STATUS "    └── network_system (Tier 4): OFF")
endif()
message(STATUS "")
message(STATUS "Build Options:")
message(STATUS "  Tests: ${PACS_BUILD_TESTS}")
message(STATUS "  Examples: ${PACS_BUILD_EXAMPLES}")
message(STATUS "  Benchmarks: ${PACS_BUILD_BENCHMARKS}")
if(TARGET pacs_storage)
    message(STATUS "  Storage: ON (SQLite3)")
else()
    message(STATUS "  Storage: OFF")
endif()
message(STATUS "========================================")
