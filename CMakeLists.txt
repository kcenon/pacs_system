cmake_minimum_required(VERSION 3.20)

project(pacs_system
    VERSION 1.0.0
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
option(PACS_WARNINGS_AS_ERRORS "Treat warnings as errors (disable in CI if dependency warnings occur)" ON)

##################################################
# Compiler Warnings (Target-Scoped)
#
# IMPORTANT: Use target_compile_options() instead of add_compile_options()
# to prevent warning flags from propagating to dependencies (container_system,
# network_system, etc.) via add_subdirectory().
#
# The pacs_apply_warnings() function is called after all pacs_* targets
# are defined to apply warnings ONLY to pacs_system's own targets.
##################################################

# Define warning flags for later application to pacs targets only
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(PACS_WARNING_FLAGS -Wall -Wextra -Wpedantic)
    if(PACS_WARNINGS_AS_ERRORS)
        list(APPEND PACS_WARNING_FLAGS -Werror)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(PACS_WARNING_FLAGS /W4)
    if(PACS_WARNINGS_AS_ERRORS)
        list(APPEND PACS_WARNING_FLAGS /WX)
    endif()
endif()

# Function to apply warnings to pacs targets only
function(pacs_apply_warnings target_name)
    if(TARGET ${target_name} AND PACS_WARNING_FLAGS)
        target_compile_options(${target_name} PRIVATE ${PACS_WARNING_FLAGS})
    endif()
endfunction()

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

# fmt library (for std::format compatibility on older compilers)
message(STATUS "")
message(STATUS "=== Finding fmt library (for format compatibility) ===")

# Check if std::format is available
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-std=c++20")
check_cxx_source_compiles("
    #include <format>
    int main() {
        auto s = std::format(\"Hello {}\", \"World\");
        return 0;
    }
" PACS_HAS_STD_FORMAT)

if(PACS_HAS_STD_FORMAT)
    message(STATUS "std::format is available - no fmt fallback needed")
    set(PACS_FMT_NEEDED FALSE)
else()
    message(STATUS "std::format not available - using fmt library as fallback")
    set(PACS_FMT_NEEDED TRUE)

    # Try to find fmt library
    find_package(fmt CONFIG QUIET)
    if(NOT fmt_FOUND)
        find_package(fmt QUIET)
    endif()

    if(fmt_FOUND)
        message(STATUS "Found fmt library")
        set(PACS_FMT_TARGET fmt::fmt)
    else()
        # Fetch fmt if not found
        message(STATUS "fmt not found, fetching from source...")
        FetchContent_Declare(
            fmt
            GIT_REPOSITORY https://github.com/fmtlib/fmt.git
            GIT_TAG 10.2.1
        )
        FetchContent_MakeAvailable(fmt)
        set(PACS_FMT_TARGET fmt::fmt)
        message(STATUS "fmt fetched and configured")
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

# Disable examples/samples for all dependencies fetched via FetchContent
# This prevents build errors from missing example source files in upstream repos
# (e.g., common_system's thread_integration_example.cpp doesn't exist but CMake
# tries to build it when thread_system target exists)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(COMMON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# thread_system (REQUIRED for thread_adapter - Tier 1)
# NOTE: Must be added BEFORE logger_system to prevent FetchContent conflicts.
# logger_system depends on thread_system and will fetch it via UnifiedDependencies
# if targets don't exist. By adding thread_system first, we ensure the sibling
# directory version is used with correct include paths.
message(STATUS "")
message(STATUS "=== Finding thread_system (REQUIRED for thread_adapter - Tier 1) ===")

set(_PACS_THREAD_PATHS
    "$ENV{THREAD_SYSTEM_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../thread_system"
    "${CMAKE_CURRENT_SOURCE_DIR}/thread_system"
)

set(THREAD_SYSTEM_FOUND FALSE)
foreach(_path ${_PACS_THREAD_PATHS})
    if(EXISTS "${_path}/CMakeLists.txt" AND EXISTS "${_path}/include/kcenon/thread/core/thread_pool.h")
        message(STATUS "Found thread_system at: ${_path}")
        set(PACS_THREAD_SYSTEM_DIR "${_path}")
        set(PACS_THREAD_SYSTEM_INCLUDE_DIR "${_path}/include")
        set(THREAD_SYSTEM_FOUND TRUE)
        break()
    endif()
endforeach()

# Avoid re-adding thread_system when the unified build already provided the targets
if(TARGET thread_base OR TARGET ThreadSystem OR TARGET interfaces)
    message(STATUS "thread_system already available in parent build - skipping add_subdirectory")
elseif(THREAD_SYSTEM_FOUND)
    set(BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(THREAD_BUILD_INTEGRATION_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${PACS_THREAD_SYSTEM_DIR}" "${CMAKE_BINARY_DIR}/thread_system_build")
else()
    message(WARNING
        "thread_system is REQUIRED for thread_adapter (Tier 1).\n"
        "thread_adapter will be disabled.\n"
        "Please ensure thread_system is available at:\n"
        "  - ../thread_system\n"
        "  - Set THREAD_SYSTEM_ROOT environment variable")
endif()

# logger_system (REQUIRED for logger_adapter - Tier 2)
message(STATUS "")
message(STATUS "=== Finding logger_system (REQUIRED for logger_adapter - Tier 2) ===")

set(_PACS_LOGGER_PATHS
    "$ENV{LOGGER_SYSTEM_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../logger_system"
    "${CMAKE_CURRENT_SOURCE_DIR}/logger_system"
)

set(LOGGER_SYSTEM_FOUND FALSE)
foreach(_path ${_PACS_LOGGER_PATHS})
    if(EXISTS "${_path}/CMakeLists.txt" AND EXISTS "${_path}/include/kcenon/logger/core/logger.h")
        message(STATUS "Found logger_system at: ${_path}")
        set(PACS_LOGGER_SYSTEM_DIR "${_path}")
        set(PACS_LOGGER_SYSTEM_INCLUDE_DIR "${_path}/include")
        set(LOGGER_SYSTEM_FOUND TRUE)
        break()
    endif()
endforeach()

if(LOGGER_SYSTEM_FOUND AND NOT TARGET LoggerSystem)
    set(BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LOGGER_BUILD_INTEGRATION_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${PACS_LOGGER_SYSTEM_DIR}" "${CMAKE_BINARY_DIR}/logger_system_build")
elseif(NOT LOGGER_SYSTEM_FOUND AND NOT TARGET LoggerSystem)
    message(WARNING
        "logger_system is REQUIRED for logger_adapter (Tier 2).\n"
        "logger_adapter will be disabled.\n"
        "Please ensure logger_system is available at:\n"
        "  - ../logger_system\n"
        "  - Set LOGGER_SYSTEM_ROOT environment variable")
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

        # Find logger_system include directory (included via network_system)
        set(_PACS_LOGGER_PATHS
            "$ENV{LOGGER_SYSTEM_ROOT}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/../logger_system/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/logger_system/include"
        )
        foreach(_path ${_PACS_LOGGER_PATHS})
            if(EXISTS "${_path}/kcenon/logger/core/logger.h")
                message(STATUS "Found logger_system at: ${_path}")
                set(PACS_LOGGER_SYSTEM_INCLUDE_DIR "${_path}")
                break()
            endif()
        endforeach()
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
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
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
    src/encoding/explicit_vr_big_endian_codec.cpp
)
target_include_directories(pacs_encoding
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(pacs_encoding
    PUBLIC pacs_core
)

# Network library
add_library(pacs_network
    src/network/pdu_encoder.cpp
    src/network/pdu_decoder.cpp
    src/network/association.cpp
    src/network/dicom_server.cpp
    src/network/dimse/dimse_message.cpp
    src/network/detail/accept_worker.cpp
    src/network/v2/dicom_association_handler.cpp
)
target_include_directories(pacs_network
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
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

# Link thread_system (Tier 1) - for accept_worker
if(TARGET ThreadSystem)
    target_link_libraries(pacs_network PUBLIC ThreadSystem)
    target_compile_definitions(pacs_network PUBLIC PACS_NETWORK_WITH_THREAD_SYSTEM)
elseif(TARGET thread_base)
    target_link_libraries(pacs_network PUBLIC thread_base)
    target_compile_definitions(pacs_network PUBLIC PACS_NETWORK_WITH_THREAD_SYSTEM)
    # CRITICAL: Explicitly add thread_system include directories
    # When thread_base is fetched via FetchContent (by logger_system), its INTERFACE
    # include directories may not be set up correctly. Ensure we can find thread_base.h
    # by explicitly adding the include directory from the sibling checkout.
    if(PACS_THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(pacs_network PUBLIC ${PACS_THREAD_SYSTEM_INCLUDE_DIR})
    endif()
endif()

# CRITICAL: Inherit thread_system compile definitions for ABI compatibility
# thread_system uses add_definitions(-DUSE_STD_JTHREAD) which doesn't propagate
# as interface compile definitions. We must manually sync these flags to ensure
# thread_base class layout matches between library and consumer code.
# Without this, derived classes have incorrect member offsets causing crashes.
#
# NOTE: We must detect jthread support ourselves because SET_STD_JTHREAD from
# thread_system doesn't propagate when using FetchContent. AppleClang doesn't
# support std::jthread even in C++20 mode, so we need proper detection.
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
    #include <stop_token>
    #include <thread>
    int main() {
        std::jthread t([](){});
        return 0;
    }
" PACS_HAS_STD_JTHREAD)

if(PACS_HAS_STD_JTHREAD)
    message(STATUS "std::jthread detected - enabling USE_STD_JTHREAD for ABI compatibility")
    target_compile_definitions(pacs_network PUBLIC USE_STD_JTHREAD)
else()
    message(STATUS "std::jthread not available - using std::thread fallback")
endif()

# Services library
add_library(pacs_services
    src/services/verification_scp.cpp
    src/services/storage_scp.cpp
    src/services/storage_scu.cpp
    src/services/query_scp.cpp
    src/services/retrieve_scp.cpp
    src/services/worklist_scp.cpp
    src/services/mpps_scp.cpp
    src/services/sop_class_registry.cpp
    src/services/sop_classes/us_storage.cpp
    src/services/validation/us_iod_validator.cpp
)
target_include_directories(pacs_services
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(pacs_services
    PUBLIC pacs_network
)

# Link common_system for Result<T>
if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(pacs_services PUBLIC ${PACS_COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(pacs_services PUBLIC PACS_WITH_COMMON_SYSTEM)
endif()

message(STATUS "  [OK] pacs_services: ON")

# Storage library
if(PACS_BUILD_STORAGE AND SQLITE3_FOUND)
    add_library(pacs_storage
        src/storage/storage_interface.cpp
        src/storage/file_storage.cpp
        src/storage/migration_runner.cpp
        src/storage/index_database.cpp
    )
    target_include_directories(pacs_storage
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
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

    # Link pacs_core for dicom_dataset and pacs_encoding for transfer_syntax
    target_link_libraries(pacs_storage PUBLIC pacs_core pacs_encoding)

    # Link fmt library for format compatibility (if needed)
    if(PACS_FMT_NEEDED AND PACS_FMT_TARGET)
        target_link_libraries(pacs_storage PUBLIC ${PACS_FMT_TARGET})
    endif()

    message(STATUS "  [OK] pacs_storage: ON (SQLite3)")
else()
    message(STATUS "  [--] pacs_storage: OFF")
endif()

# Integration library (requires container_system and logger_system via network_system)
if(TARGET container_system AND COMMON_SYSTEM_FOUND AND TARGET NetworkSystem)
    # Check for monitoring_system availability BEFORE defining pacs_integration
    # This allows conditional inclusion of monitoring_adapter.cpp
    set(_PACS_MONITORING_PATHS_EARLY
        "$ENV{MONITORING_SYSTEM_ROOT}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../monitoring_system"
        "${CMAKE_CURRENT_SOURCE_DIR}/monitoring_system"
    )
    set(MONITORING_SYSTEM_AVAILABLE FALSE)
    foreach(_path ${_PACS_MONITORING_PATHS_EARLY})
        if(EXISTS "${_path}/CMakeLists.txt" AND EXISTS "${_path}/include/kcenon/monitoring/core/performance_monitor.h")
            set(MONITORING_SYSTEM_AVAILABLE TRUE)
            break()
        endif()
    endforeach()

    # Define integration library sources
    set(PACS_INTEGRATION_SOURCES
        src/integration/container_adapter.cpp
        src/integration/logger_adapter.cpp
        src/integration/thread_adapter.cpp
        src/integration/network_adapter.cpp
        src/integration/dicom_session.cpp
    )

    # Only include monitoring_adapter.cpp if monitoring_system is available
    if(MONITORING_SYSTEM_AVAILABLE)
        list(APPEND PACS_INTEGRATION_SOURCES src/integration/monitoring_adapter.cpp)
    endif()

    add_library(pacs_integration ${PACS_INTEGRATION_SOURCES})
    target_include_directories(pacs_integration
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    # Suppress deprecated warnings from thread_system headers (logger_interface is deprecated)
    target_compile_options(pacs_integration PRIVATE
        $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-deprecated-declarations>
        $<$<CXX_COMPILER_ID:MSVC>:/wd4996>
    )
    target_link_libraries(pacs_integration
        PUBLIC pacs_core pacs_encoding pacs_network container_system
    )

    # Link fmt library for format compatibility (if needed)
    if(PACS_FMT_NEEDED AND PACS_FMT_TARGET)
        target_link_libraries(pacs_integration PUBLIC ${PACS_FMT_TARGET})
    endif()

    # Link logger_system (REQUIRED for logger_adapter)
    if(TARGET LoggerSystem)
        target_link_libraries(pacs_integration PUBLIC LoggerSystem)
        target_compile_definitions(pacs_integration PUBLIC PACS_WITH_LOGGER_SYSTEM)
        message(STATUS "    - logger_adapter: ON (LoggerSystem)")
    else()
        message(WARNING "    - logger_adapter: OFF (LoggerSystem target not found)")
    endif()

    # Link thread_system (REQUIRED for thread_adapter)
    # thread_system creates 'thread_base' as main target (legacy build) or 'ThreadSystem' (new build)
    if(TARGET ThreadSystem)
        target_link_libraries(pacs_integration PUBLIC ThreadSystem)
        target_compile_definitions(pacs_integration PUBLIC PACS_WITH_THREAD_SYSTEM)
        message(STATUS "    - thread_adapter: ON (ThreadSystem)")
    elseif(TARGET thread_base)
        target_link_libraries(pacs_integration PUBLIC thread_base)
        target_compile_definitions(pacs_integration PUBLIC PACS_WITH_THREAD_SYSTEM)
        message(STATUS "    - thread_adapter: ON (thread_base)")
    elseif(PACS_THREAD_SYSTEM_INCLUDE_DIR)
        # Fallback: include headers only (will fail at link time)
        target_include_directories(pacs_integration PUBLIC ${PACS_THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(pacs_integration PUBLIC PACS_WITH_THREAD_SYSTEM)
        message(WARNING "    - thread_adapter: ON (headers only - linking may fail)")
    else()
        message(WARNING "    - thread_adapter: OFF (ThreadSystem/thread_base target not found)")
    endif()

    # CRITICAL: Inherit thread_system compile definitions for ABI compatibility
    # Use the same PACS_HAS_STD_JTHREAD detection result from pacs_network
    if(PACS_HAS_STD_JTHREAD)
        target_compile_definitions(pacs_integration PUBLIC USE_STD_JTHREAD)
    endif()

    # Link common_system for Result<T>
    if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(pacs_integration PUBLIC ${PACS_COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(pacs_integration PUBLIC PACS_WITH_COMMON_SYSTEM)
    endif()

    # Link network_system (REQUIRED for network_adapter)
    if(TARGET NetworkSystem)
        target_link_libraries(pacs_integration PUBLIC NetworkSystem)
        target_compile_definitions(pacs_integration PUBLIC PACS_WITH_NETWORK_SYSTEM)
        message(STATUS "    - network_adapter: ON (NetworkSystem)")
    else()
        message(WARNING "    - network_adapter: OFF (NetworkSystem target not found)")
    endif()

    # Link monitoring_system (REQUIRED for monitoring_adapter)
    set(_PACS_MONITORING_PATHS
        "$ENV{MONITORING_SYSTEM_ROOT}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../monitoring_system"
        "${CMAKE_CURRENT_SOURCE_DIR}/monitoring_system"
    )
    set(MONITORING_SYSTEM_FOUND FALSE)
    foreach(_path ${_PACS_MONITORING_PATHS})
        if(EXISTS "${_path}/CMakeLists.txt" AND EXISTS "${_path}/include/kcenon/monitoring/core/performance_monitor.h")
            set(PACS_MONITORING_SYSTEM_DIR "${_path}")
            set(PACS_MONITORING_SYSTEM_INCLUDE_DIR "${_path}/include")
            set(MONITORING_SYSTEM_FOUND TRUE)
            break()
        endif()
    endforeach()

    if(MONITORING_SYSTEM_FOUND AND NOT TARGET monitoring_system)
        set(BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(MONITORING_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(MONITORING_BUILD_INTEGRATION_TESTS OFF CACHE BOOL "" FORCE)
        set(MONITORING_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

        # Workaround for monitoring_system alias mismatch:
        # monitoring_system expects kcenon::common_system but common_system provides kcenon::common
        # Create the missing alias if common_system target exists
        if(NOT TARGET kcenon::common_system)
            if(TARGET common_system)
                add_library(kcenon::common_system ALIAS common_system)
                message(STATUS "Created kcenon::common_system alias for monitoring_system compatibility")
            endif()
        endif()

        add_subdirectory("${PACS_MONITORING_SYSTEM_DIR}" "${CMAKE_BINARY_DIR}/monitoring_system_build")
    endif()

    if(TARGET monitoring_system)
        target_link_libraries(pacs_integration PUBLIC monitoring_system)
        target_compile_definitions(pacs_integration PUBLIC PACS_WITH_MONITORING_SYSTEM)
        message(STATUS "    - monitoring_adapter: ON (monitoring_system)")
    elseif(PACS_MONITORING_SYSTEM_INCLUDE_DIR)
        # Fallback: include headers only
        target_include_directories(pacs_integration PUBLIC ${PACS_MONITORING_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(pacs_integration PUBLIC PACS_WITH_MONITORING_SYSTEM)
        message(STATUS "    - monitoring_adapter: ON (headers only)")
    else()
        message(WARNING "    - monitoring_adapter: OFF (monitoring_system target not found)")
    endif()

    message(STATUS "  [OK] pacs_integration: ON (container_system, logger_system, thread_system, network_system, monitoring_system)")

    # CRITICAL: Link pacs_integration to pacs_network for thread_adapter usage in dicom_server.
    # dicom_server.cpp uses thread_adapter::submit_fire_and_forget() for association worker threads.
    # This creates a circular dependency (pacs_integration -> pacs_network -> pacs_integration)
    # but CMake handles this correctly for static libraries by passing libraries multiple times
    # to the linker. Using PUBLIC so that targets linking pacs_network also get pacs_integration
    # (required because static libraries don't actually link - symbols are resolved at final link).
    target_link_libraries(pacs_network PUBLIC pacs_integration)
    message(STATUS "    - pacs_network: linked pacs_integration for thread_adapter")
else()
    message(STATUS "  [--] pacs_integration: OFF (requires container_system and network_system)")
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
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/services)
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/storage)
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/integration)

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
        tests/encoding/byte_swap_test.cpp
        tests/encoding/explicit_vr_big_endian_codec_test.cpp
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
        tests/network/association_test.cpp
        tests/network/dicom_server_test.cpp
        tests/network/dimse/dimse_message_test.cpp
        tests/network/dimse/n_service_test.cpp
        tests/network/detail/accept_worker_test.cpp
        tests/network/v2/dicom_association_handler_test.cpp
    )
    target_link_libraries(network_tests
        PRIVATE
            pacs_network
            pacs_services
            Catch2::Catch2WithMain
    )
    # Link pacs_integration for thread_adapter (used by dicom_server)
    if(TARGET pacs_integration)
        target_link_libraries(network_tests PRIVATE pacs_integration)
    endif()

    # Services tests
    add_executable(services_tests
        tests/services/verification_scp_test.cpp
        tests/services/storage_scp_test.cpp
        tests/services/storage_scu_test.cpp
        tests/services/query_scp_test.cpp
        tests/services/retrieve_scp_test.cpp
        tests/services/worklist_scp_test.cpp
        tests/services/mpps_scp_test.cpp
        tests/services/us_storage_test.cpp
    )
    target_link_libraries(services_tests
        PRIVATE
            pacs_services
            Catch2::Catch2WithMain
    )

    # Storage tests
    if(TARGET pacs_storage)
        add_executable(storage_tests
            tests/storage/storage_interface_test.cpp
            tests/storage/file_storage_test.cpp
            tests/storage/migration_runner_test.cpp
            tests/storage/index_database_test.cpp
            tests/storage/mpps_test.cpp
            tests/storage/worklist_test.cpp
        )
        target_link_libraries(storage_tests
            PRIVATE
                pacs_storage
                Catch2::Catch2WithMain
                $<$<BOOL:${fmt_FOUND}>:fmt::fmt>
        )
    endif()

    # Integration tests
    if(TARGET pacs_integration)
        add_executable(pacs_integration_tests
            tests/integration/container_adapter_test.cpp
            tests/integration/logger_adapter_test.cpp
            tests/integration/thread_adapter_test.cpp
            tests/integration/thread_system_direct_test.cpp
            tests/integration/network_adapter_test.cpp
            tests/integration/monitoring_adapter_test.cpp
        )
        target_link_libraries(pacs_integration_tests
            PRIVATE
                pacs_integration
                pacs_network
                Catch2::Catch2WithMain
        )
    endif()

    include(CTest)
    include(Catch)

    ##################################################
    # CTest Labels for CI/CD Integration
    # @see Issue #139 - CI/CD Integration
    #
    # Labels:
    #   - unit: Fast unit tests (timeout: 60s)
    #   - integration: Integration tests (timeout: 300s)
    #   - stress: Stress/performance tests (timeout: 600s)
    #   - slow: Long-running tests (manual trigger only)
    ##################################################

    # Unit tests - fast, isolated component tests
    catch_discover_tests(core_tests
        PROPERTIES LABELS "unit"
    )
    catch_discover_tests(encoding_tests
        PROPERTIES LABELS "unit"
    )
    catch_discover_tests(network_tests
        PROPERTIES LABELS "unit"
    )
    catch_discover_tests(services_tests
        PROPERTIES LABELS "unit"
    )
    if(TARGET storage_tests)
        catch_discover_tests(storage_tests
            PROPERTIES LABELS "unit"
        )
    endif()

    # Integration tests - cross-module adapter tests
    # TEST_PREFIX ensures these tests are excluded from unit test runs
    # (CI uses --exclude-regex "integration::" for unit tests)
    if(TARGET pacs_integration_tests)
        catch_discover_tests(pacs_integration_tests
            TEST_PREFIX "integration::"
            PROPERTIES LABELS "integration"
        )
    endif()
endif()

##################################################
# Examples
##################################################

if(PACS_BUILD_EXAMPLES)
    message(STATUS "")
    message(STATUS "=== Building Examples ===")

    # DCM Dump - DICOM file inspection utility (no network dependencies)
    add_subdirectory(examples/dcm_dump)
    message(STATUS "  [OK] dcm_dump: DICOM file inspection utility")

    # DCM Modify - DICOM tag modification utility (no network dependencies)
    add_subdirectory(examples/dcm_modify)
    message(STATUS "  [OK] dcm_modify: DICOM tag modification utility")

    # Echo SCU/SCP samples - basic connectivity test
    if(TARGET pacs_integration)
        add_subdirectory(examples/echo_scu)
        add_subdirectory(examples/echo_scp)
        message(STATUS "  [OK] echo_scu: DICOM connectivity test client")
        message(STATUS "  [OK] echo_scp: DICOM connectivity test server")
    else()
        message(STATUS "  [--] echo_scu/scp: OFF (requires pacs_integration)")
    endif()

    # Secure Echo SCU/SCP samples - TLS-secured connectivity test
    if(TARGET pacs_integration)
        add_subdirectory(examples/secure_dicom)
        message(STATUS "  [OK] secure_echo_scu: TLS-secured DICOM client")
        message(STATUS "  [OK] secure_echo_scp: TLS-secured DICOM server")
    else()
        message(STATUS "  [--] secure_dicom: OFF (requires pacs_integration)")
    endif()

    # Store SCU sample - DICOM image sender
    if(TARGET pacs_integration)
        add_subdirectory(examples/store_scu)
        message(STATUS "  [OK] store_scu: DICOM image sender")
    else()
        message(STATUS "  [--] store_scu: OFF (requires pacs_integration)")
    endif()

    # Query SCU sample - DICOM C-FIND client
    if(TARGET pacs_integration)
        add_subdirectory(examples/query_scu)
        message(STATUS "  [OK] query_scu: DICOM C-FIND client")
    else()
        message(STATUS "  [--] query_scu: OFF (requires pacs_integration)")
    endif()

    # Retrieve SCU sample - DICOM C-MOVE/C-GET client
    if(TARGET pacs_integration)
        add_subdirectory(examples/retrieve_scu)
        message(STATUS "  [OK] retrieve_scu: DICOM C-MOVE/C-GET client")
    else()
        message(STATUS "  [--] retrieve_scu: OFF (requires pacs_integration)")
    endif()

    # Worklist SCU sample - Modality Worklist Query client
    if(TARGET pacs_integration)
        add_subdirectory(examples/worklist_scu)
        message(STATUS "  [OK] worklist_scu: Modality Worklist query client")
    else()
        message(STATUS "  [--] worklist_scu: OFF (requires pacs_integration)")
    endif()

    # MPPS SCU sample - Modality Performed Procedure Step client
    if(TARGET pacs_integration)
        add_subdirectory(examples/mpps_scu)
        message(STATUS "  [OK] mpps_scu: MPPS N-CREATE/N-SET client")
    else()
        message(STATUS "  [--] mpps_scu: OFF (requires pacs_integration)")
    endif()

    # Store SCP sample - DICOM image receiver
    if(TARGET pacs_storage AND TARGET pacs_integration)
        add_subdirectory(examples/store_scp)
        message(STATUS "  [OK] store_scp: DICOM image receiver")
    else()
        message(STATUS "  [--] store_scp: OFF (requires pacs_storage and pacs_integration)")
    endif()

    # Database Browser - PACS index database viewer
    if(TARGET pacs_storage)
        add_subdirectory(examples/db_browser)
        message(STATUS "  [OK] db_browser: PACS index database viewer")
    else()
        message(STATUS "  [--] db_browser: OFF (requires pacs_storage)")
    endif()

    # PACS Server example requires all modules
    if(TARGET pacs_storage AND TARGET pacs_integration)
        add_subdirectory(examples/pacs_server)
        message(STATUS "  [OK] pacs_server: Complete DICOM archive")
    else()
        message(STATUS "  [--] pacs_server: OFF (requires pacs_storage and pacs_integration)")
    endif()

    # Integration Test Suite - End-to-end workflow tests
    if(TARGET pacs_storage AND TARGET pacs_integration AND TARGET Catch2::Catch2WithMain)
        add_subdirectory(examples/integration_tests)
        message(STATUS "  [OK] pacs_integration_e2e: End-to-end workflow tests")
    else()
        message(STATUS "  [--] pacs_integration_e2e: OFF (requires pacs_storage, pacs_integration, and Catch2)")
    endif()
endif()

##################################################
# Benchmarks
##################################################

if(PACS_BUILD_BENCHMARKS)
    message(STATUS "")
    message(STATUS "=== Building Benchmarks ===")

    # Thread Performance Benchmarks
    # @see Issue #154 - Establish performance baseline benchmarks for thread migration
    if(TARGET pacs_integration AND TARGET Catch2::Catch2WithMain)
        add_subdirectory(benchmarks/thread_performance)
        message(STATUS "  [OK] thread_performance_benchmarks: Thread migration baseline")
    else()
        message(STATUS "  [--] thread_performance_benchmarks: OFF (requires pacs_integration and Catch2)")
    endif()
endif()

##################################################
# Apply Compiler Warnings to PACS Targets Only
#
# This must be done AFTER all pacs_* targets are defined
# and AFTER dependencies are added via add_subdirectory().
# This ensures warning flags are NOT inherited by dependencies.
##################################################

message(STATUS "")
message(STATUS "=== Applying Compiler Warnings to PACS Targets ===")

# Core PACS libraries
pacs_apply_warnings(pacs_core)
pacs_apply_warnings(pacs_encoding)
pacs_apply_warnings(pacs_network)
pacs_apply_warnings(pacs_services)

# Optional PACS libraries
if(TARGET pacs_storage)
    pacs_apply_warnings(pacs_storage)
endif()

if(TARGET pacs_integration)
    pacs_apply_warnings(pacs_integration)
endif()

# Test executables
if(PACS_BUILD_TESTS)
    pacs_apply_warnings(core_tests)
    pacs_apply_warnings(encoding_tests)
    pacs_apply_warnings(network_tests)
    pacs_apply_warnings(services_tests)
    if(TARGET storage_tests)
        pacs_apply_warnings(storage_tests)
    endif()
    if(TARGET pacs_integration_tests)
        pacs_apply_warnings(pacs_integration_tests)
    endif()
endif()

# Benchmark executables
if(PACS_BUILD_BENCHMARKS)
    if(TARGET thread_performance_benchmarks)
        pacs_apply_warnings(thread_performance_benchmarks)
    endif()
endif()

if(PACS_WARNINGS_AS_ERRORS)
    message(STATUS "  Warning flags: ${PACS_WARNING_FLAGS} (APPLIED TO PACS TARGETS ONLY)")
else()
    message(STATUS "  Warning flags: ${PACS_WARNING_FLAGS} (warnings-as-errors disabled)")
endif()
message(STATUS "")

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
if(PACS_BUILD_EXAMPLES AND TARGET pacs_server)
    message(STATUS "  Examples: ON (pacs_server)")
else()
    message(STATUS "  Examples: ${PACS_BUILD_EXAMPLES}")
endif()
message(STATUS "  Benchmarks: ${PACS_BUILD_BENCHMARKS}")
message(STATUS "  Services: ON")
if(TARGET pacs_storage)
    message(STATUS "  Storage: ON (SQLite3)")
else()
    message(STATUS "  Storage: OFF")
endif()
if(TARGET pacs_integration)
    message(STATUS "  Integration: ON (container_system)")
else()
    message(STATUS "  Integration: OFF")
endif()
message(STATUS "========================================")
