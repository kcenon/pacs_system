cmake_minimum_required(VERSION 3.20)

project(pacs_system
    VERSION 0.1.0.0
    DESCRIPTION "Modern C++20 PACS implementation"
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
option(PACS_BUILD_SAMPLES "Build developer samples" OFF)

# Dependency integration options
option(PACS_WITH_COMMON_SYSTEM "Enable common_system integration (REQUIRED)" ON)
option(PACS_WITH_CONTAINER_SYSTEM "Enable container_system integration (REQUIRED)" ON)
option(PACS_WITH_NETWORK_SYSTEM "Enable network_system integration (REQUIRED)" ON)
option(PACS_BUILD_STORAGE "Build storage module (requires SQLite3)" ON)
option(PACS_BUILD_ITK_ADAPTER "Build ITK integration adapter (requires ITK 5.x)" OFF)
option(PACS_WARNINGS_AS_ERRORS "Treat warnings as errors (disable in CI if dependency warnings occur)" ON)
option(PACS_BUILD_MODULES "Build C++20 module version of pacs_system" OFF)

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

# libjpeg-turbo (for JPEG compression codecs)
message(STATUS "")
message(STATUS "=== Finding libjpeg-turbo (for JPEG compression) ===")

# Try to find system libjpeg/libjpeg-turbo
# NOTE: libjpeg-turbo cannot be built via FetchContent (their CMakeLists.txt
# explicitly prohibits integration into other build systems).
# System packages must be installed:
#   - Ubuntu: sudo apt install libjpeg-turbo8-dev
#   - macOS: brew install jpeg-turbo
#   - Windows: vcpkg install libjpeg-turbo:x64-windows
find_package(JPEG QUIET)

if(JPEG_FOUND)
    message(STATUS "Found system libjpeg: ${JPEG_VERSION}")
    set(PACS_JPEG_FOUND TRUE)
    set(PACS_JPEG_TARGET JPEG::JPEG)
else()
    message(WARNING
        "libjpeg/libjpeg-turbo not found!\n"
        "JPEG codec will be disabled.\n"
        "\n"
        "To enable JPEG support, install libjpeg-turbo:\n"
        "  - Ubuntu: sudo apt install libjpeg-turbo8-dev\n"
        "  - macOS: brew install jpeg-turbo\n"
        "  - Windows: vcpkg install libjpeg-turbo:x64-windows")
    set(PACS_JPEG_FOUND FALSE)
endif()

# libpng (for PNG image output)
message(STATUS "")
message(STATUS "=== Finding libpng (for PNG image output) ===")

# Try to find system libpng
find_package(PNG QUIET)

if(PNG_FOUND)
    message(STATUS "Found system libpng: ${PNG_VERSION_STRING}")
    set(PACS_PNG_FOUND TRUE)
    set(PACS_PNG_TARGET PNG::PNG)
else()
    message(WARNING
        "libpng not found!\n"
        "PNG output will be disabled.\n"
        "\n"
        "To enable PNG support, install libpng:\n"
        "  - Ubuntu: sudo apt install libpng-dev\n"
        "  - macOS: brew install libpng\n"
        "  - Windows: vcpkg install libpng:x64-windows")
    set(PACS_PNG_FOUND FALSE)
endif()

# OpenJPEG (for JPEG 2000 compression codec)
message(STATUS "")
message(STATUS "=== Finding OpenJPEG (for JPEG 2000 compression) ===")

# Try to find system OpenJPEG
find_package(OpenJPEG QUIET)

if(OpenJPEG_FOUND)
    message(STATUS "Found system OpenJPEG: ${OPENJPEG_VERSION}")
    set(PACS_JPEG2000_FOUND TRUE)
else()
    # Try pkg-config as fallback
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(OPENJPEG QUIET libopenjp2)
        if(OPENJPEG_FOUND)
            message(STATUS "Found OpenJPEG via pkg-config: ${OPENJPEG_VERSION}")
            set(PACS_JPEG2000_FOUND TRUE)
        endif()
    endif()
endif()

if(NOT PACS_JPEG2000_FOUND)
    message(WARNING
        "OpenJPEG not found!\n"
        "JPEG 2000 codec will be disabled.\n"
        "\n"
        "To enable JPEG 2000 support, install OpenJPEG:\n"
        "  - Ubuntu: sudo apt install libopenjp2-7-dev\n"
        "  - macOS: brew install openjpeg\n"
        "  - Windows: vcpkg install openjpeg:x64-windows")
    set(PACS_JPEG2000_FOUND FALSE)
endif()

# CharLS (for JPEG-LS compression codec)
message(STATUS "")
message(STATUS "=== Finding CharLS (for JPEG-LS compression) ===")

# Try to find system CharLS
find_package(charls CONFIG QUIET)

if(charls_FOUND)
    message(STATUS "Found system CharLS")
    set(PACS_JPEGLS_FOUND TRUE)
else()
    # Try pkg-config as fallback
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(CHARLS QUIET charls)
        if(CHARLS_FOUND)
            message(STATUS "Found CharLS via pkg-config: ${CHARLS_VERSION}")
            set(PACS_JPEGLS_FOUND TRUE)
        endif()
    endif()
endif()

if(NOT PACS_JPEGLS_FOUND)
    message(WARNING
        "CharLS not found!\n"
        "JPEG-LS codec will be disabled.\n"
        "\n"
        "To enable JPEG-LS support, install CharLS:\n"
        "  - Ubuntu: sudo apt install libcharls-dev\n"
        "  - macOS: brew install charls\n"
        "  - Windows: vcpkg install charls:x64-windows")
    set(PACS_JPEGLS_FOUND FALSE)
endif()

# ITK (for ITK integration adapter - Issue #463)
if(PACS_BUILD_ITK_ADAPTER)
    message(STATUS "")
    message(STATUS "=== Finding ITK (for ITK integration adapter) ===")

    find_package(ITK QUIET COMPONENTS
        ITKCommon
        ITKIOImageBase
    )

    if(ITK_FOUND)
        message(STATUS "Found ITK: ${ITK_VERSION}")
        set(PACS_ITK_FOUND TRUE)
        include(${ITK_USE_FILE})
    else()
        message(WARNING
            "ITK not found!\n"
            "ITK integration adapter will be disabled.\n"
            "\n"
            "To enable ITK support, install ITK 5.x:\n"
            "  - Ubuntu: sudo apt install libinsighttoolkit5-dev\n"
            "  - macOS: brew install itk\n"
            "  - Windows: vcpkg install itk:x64-windows\n"
            "  - Or build from source: https://itk.org/")
        set(PACS_ITK_FOUND FALSE)
        set(PACS_BUILD_ITK_ADAPTER OFF)
    endif()
endif()

# OpenSSL (for digital signatures - Issue #191)
message(STATUS "")
message(STATUS "=== Finding OpenSSL (for digital signatures) ===")

find_package(OpenSSL QUIET)

if(OpenSSL_FOUND)
    message(STATUS "Found system OpenSSL: ${OPENSSL_VERSION}")
    set(PACS_OPENSSL_FOUND TRUE)
else()
    message(WARNING
        "OpenSSL not found!\n"
        "Digital signature features will be disabled.\n"
        "\n"
        "To enable digital signatures, install OpenSSL:\n"
        "  - Ubuntu: sudo apt install libssl-dev\n"
        "  - macOS: brew install openssl@3\n"
        "  - Windows: vcpkg install openssl:x64-windows")
    set(PACS_OPENSSL_FOUND FALSE)
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

# database_system (OPTIONAL - Tier 3, for secure database operations)
if(PACS_BUILD_STORAGE)
    message(STATUS "")
    message(STATUS "=== Finding database_system (OPTIONAL - Tier 3) ===")

    set(_PACS_DATABASE_PATHS
        "$ENV{DATABASE_SYSTEM_ROOT}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../database_system"
        "${CMAKE_CURRENT_SOURCE_DIR}/database_system"
    )

    set(DATABASE_SYSTEM_FOUND FALSE)
    foreach(_path ${_PACS_DATABASE_PATHS})
        if(EXISTS "${_path}/CMakeLists.txt" AND EXISTS "${_path}/database/database_manager.h")
            message(STATUS "Found database_system at: ${_path}")
            set(PACS_DATABASE_SYSTEM_DIR "${_path}")
            set(PACS_DATABASE_SYSTEM_INCLUDE_DIR "${_path}")
            set(DATABASE_SYSTEM_FOUND TRUE)
            break()
        endif()
    endforeach()

    if(DATABASE_SYSTEM_FOUND AND NOT TARGET database)
        set(BUILD_DATABASE_SAMPLES OFF CACHE BOOL "" FORCE)
        set(USE_UNIT_TEST OFF CACHE BOOL "" FORCE)
        set(DATABASE_BUILD_INTEGRATION_TESTS OFF CACHE BOOL "" FORCE)
        set(DATABASE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
        set(USE_SQLITE ON CACHE BOOL "" FORCE)
        add_subdirectory("${PACS_DATABASE_SYSTEM_DIR}" "${CMAKE_BINARY_DIR}/database_system_build")
        message(STATUS "  [OK] database_system added as subdirectory")
    elseif(DATABASE_SYSTEM_FOUND)
        message(STATUS "  [OK] database_system already available")
    else()
        message(STATUS "  [--] database_system not found - SQL injection protection via query builder disabled")
        message(STATUS "       Install database_system at ../database_system for secure database operations")
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
        if(EXISTS "${_path}/CMakeLists.txt" AND EXISTS "${_path}/include/kcenon/network/network_system.h")
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

# Crow Web Framework (for REST API - Issue #194)
message(STATUS "")
message(STATUS "=== Fetching Crow Web Framework (for REST API) ===")

# Crow requires Asio. The network_system already fetched standalone ASIO.
# Point Crow to use the same ASIO headers.
if(DEFINED CACHE{network_system_asio_SOURCE_DIR})
    set(ASIO_INCLUDE_DIR "${network_system_asio_SOURCE_DIR}/asio/include" CACHE PATH "" FORCE)
    message(STATUS "  Using ASIO from network_system: ${ASIO_INCLUDE_DIR}")
elseif(EXISTS "${CMAKE_BINARY_DIR}/_deps/network_system_asio-src/asio/include")
    set(ASIO_INCLUDE_DIR "${CMAKE_BINARY_DIR}/_deps/network_system_asio-src/asio/include" CACHE PATH "" FORCE)
    message(STATUS "  Using ASIO from build deps: ${ASIO_INCLUDE_DIR}")
else()
    # Fallback: fetch standalone ASIO for Crow
    message(STATUS "  Fetching standalone ASIO for Crow...")
    FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-30-2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(asio)
    set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include" CACHE PATH "" FORCE)
endif()

FetchContent_Declare(
    Crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG master  # Use master for ASIO 1.30+ compatibility (io_context vs io_service)
)
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CROW_ENABLE_SSL OFF CACHE BOOL "" FORCE)
set(CROW_ENABLE_COMPRESSION OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Crow)
message(STATUS "  [OK] Crow (master) fetched")

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
    src/core/pool_manager.cpp
)
target_include_directories(pacs_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link common_system (Tier 0) - Result<T> pattern
if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(pacs_core PUBLIC ${PACS_COMMON_SYSTEM_INCLUDE_DIR})
    # Use KCENON_HAS_COMMON_SYSTEM for unified feature flag naming
    # Keep PACS_WITH_COMMON_SYSTEM as legacy alias for migration window
    target_compile_definitions(pacs_core PUBLIC
        KCENON_HAS_COMMON_SYSTEM=1
        PACS_WITH_COMMON_SYSTEM=1
    )
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
    src/encoding/compression/jpeg_baseline_codec.cpp
    src/encoding/compression/jpeg_lossless_codec.cpp
    src/encoding/compression/jpeg2000_codec.cpp
    src/encoding/compression/jpeg_ls_codec.cpp
    src/encoding/compression/rle_codec.cpp
    src/encoding/compression/codec_factory.cpp
)
target_include_directories(pacs_encoding
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(pacs_encoding
    PUBLIC pacs_core
)

# Link libjpeg-turbo for JPEG compression codecs
if(PACS_JPEG_FOUND)
    target_link_libraries(pacs_encoding PUBLIC ${PACS_JPEG_TARGET})
    if(PACS_JPEG_INCLUDE_DIR)
        target_include_directories(pacs_encoding PUBLIC ${PACS_JPEG_INCLUDE_DIR})
    endif()
    target_compile_definitions(pacs_encoding PUBLIC PACS_WITH_JPEG_CODEC)
    message(STATUS "  [OK] pacs_encoding: JPEG codec enabled")
endif()

# Link OpenJPEG for JPEG 2000 compression codec
if(PACS_JPEG2000_FOUND)
    if(TARGET openjp2)
        target_link_libraries(pacs_encoding PUBLIC openjp2)
    elseif(OPENJPEG_LIBRARIES)
        target_link_libraries(pacs_encoding PUBLIC ${OPENJPEG_LIBRARIES})
        if(OPENJPEG_INCLUDE_DIRS)
            target_include_directories(pacs_encoding PUBLIC ${OPENJPEG_INCLUDE_DIRS})
        endif()
    endif()
    target_compile_definitions(pacs_encoding PUBLIC PACS_WITH_JPEG2000_CODEC)
    message(STATUS "  [OK] pacs_encoding: JPEG 2000 codec enabled")
endif()

# Link CharLS for JPEG-LS compression codec
if(PACS_JPEGLS_FOUND)
    if(TARGET charls)
        target_link_libraries(pacs_encoding PUBLIC charls)
    elseif(TARGET charls::charls)
        target_link_libraries(pacs_encoding PUBLIC charls::charls)
    elseif(CHARLS_LIBRARIES)
        target_link_libraries(pacs_encoding PUBLIC ${CHARLS_LIBRARIES})
        if(CHARLS_INCLUDE_DIRS)
            target_include_directories(pacs_encoding PUBLIC ${CHARLS_INCLUDE_DIRS})
        endif()
    endif()
    target_compile_definitions(pacs_encoding PUBLIC PACS_WITH_JPEGLS_CODEC)
    message(STATUS "  [OK] pacs_encoding: JPEG-LS codec enabled")
endif()

# Network library
add_library(pacs_network
    src/network/pdu_encoder.cpp
    src/network/pdu_decoder.cpp
    src/network/pdu_buffer_pool.cpp
    src/network/association.cpp
    src/network/dicom_server.cpp
    src/network/dimse/dimse_message.cpp
    src/network/detail/accept_worker.cpp
    src/network/v2/dicom_association_handler.cpp
    src/network/v2/dicom_server_v2.cpp
    # Pipeline infrastructure (Issue #517)
    src/network/pipeline/pipeline_coordinator.cpp
    # Pipeline jobs (Issue #517, #519-#522)
    src/network/pipeline/jobs/receive_network_io_job.cpp
    src/network/pipeline/jobs/send_network_io_job.cpp
    src/network/pipeline/jobs/pdu_decode_job.cpp
    src/network/pipeline/jobs/dimse_process_job.cpp
    src/network/pipeline/jobs/storage_query_exec_job.cpp
    src/network/pipeline/jobs/response_encode_job.cpp
    # Pipeline adapter (Issue #523)
    src/network/pipeline/pipeline_adapter.cpp
)
target_include_directories(pacs_network
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
# Suppress deprecated warnings from common_system/logger_system headers (logger_interface is deprecated)
target_compile_options(pacs_network PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-deprecated-declarations>
    $<$<CXX_COMPILER_ID:MSVC>:/wd4996>
)
target_link_libraries(pacs_network
    PUBLIC pacs_core pacs_encoding pacs_security
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

# CRITICAL: Define KCENON_HAS_COMMON_EXECUTOR for thread_system ABI compatibility
# thread_system's thread_pool.h uses conditional compilation:
#   - KCENON_HAS_COMMON_EXECUTOR=1: is_running() declared with 'override' (virtual)
#   - KCENON_HAS_COMMON_EXECUTOR=0: is_running() declared as non-virtual
# When common_system is available, thread_system compiles with KCENON_HAS_COMMON_EXECUTOR=1.
# Consumer code must use the same definition to match the library's function signatures.
if(PACS_COMMON_SYSTEM_FOUND)
    message(STATUS "common_system found - enabling KCENON_HAS_COMMON_EXECUTOR for ABI compatibility")
    target_compile_definitions(pacs_network PUBLIC KCENON_HAS_COMMON_EXECUTOR=1)
endif()

# Client library (remote node management for SCU operations)
add_library(pacs_client
    src/client/remote_node_manager.cpp
    src/client/job_manager.cpp
    src/client/routing_manager.cpp
    src/client/prefetch_manager.cpp
    src/client/sync_manager.cpp
)
target_include_directories(pacs_client
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(pacs_client
    PUBLIC pacs_network
)
if(PACS_BUILD_STORAGE AND SQLITE3_FOUND)
    target_link_libraries(pacs_client PUBLIC pacs_storage)
    target_compile_definitions(pacs_client PUBLIC PACS_WITH_DATABASE_SYSTEM=1)
endif()

# Link common_system (Tier 0)
if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(pacs_client PUBLIC ${PACS_COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(pacs_client PUBLIC
        KCENON_HAS_COMMON_SYSTEM=1
        PACS_WITH_COMMON_SYSTEM=1
    )
endif()

# Services library
add_library(pacs_services
    src/services/verification_scp.cpp
    src/services/storage_scp.cpp
    src/services/storage_scu.cpp
    src/services/query_scu.cpp
    src/services/retrieve_scu.cpp
    src/services/query_scp.cpp
    src/services/retrieve_scp.cpp
    src/services/worklist_scp.cpp
    src/services/worklist_scu.cpp
    src/services/mpps_scp.cpp
    src/services/mpps_scu.cpp
    src/services/sop_class_registry.cpp
    src/services/sop_classes/us_storage.cpp
    src/services/sop_classes/dx_storage.cpp
    src/services/sop_classes/mg_storage.cpp
    src/services/sop_classes/pet_storage.cpp
    src/services/sop_classes/nm_storage.cpp
    src/services/sop_classes/rt_storage.cpp
    src/services/sop_classes/seg_storage.cpp
    src/services/sop_classes/sr_storage.cpp
    src/services/validation/us_iod_validator.cpp
    src/services/validation/dx_iod_validator.cpp
    src/services/validation/mg_iod_validator.cpp
    src/services/validation/pet_iod_validator.cpp
    src/services/validation/nm_iod_validator.cpp
    src/services/validation/rt_iod_validator.cpp
    src/services/validation/seg_iod_validator.cpp
    src/services/validation/sr_iod_validator.cpp
    src/services/cache/query_cache.cpp
    src/services/cache/database_cursor.cpp
    src/services/cache/query_result_stream.cpp
    src/services/cache/streaming_query_handler.cpp
    src/services/cache/parallel_query_executor.cpp
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
    # Use KCENON_HAS_COMMON_SYSTEM for unified feature flag naming
    # Keep PACS_WITH_COMMON_SYSTEM as legacy alias for migration window
    target_compile_definitions(pacs_services PUBLIC
        KCENON_HAS_COMMON_SYSTEM=1
        PACS_WITH_COMMON_SYSTEM=1
    )
endif()

# Link database_system for database_cursor query building (Issue #420)
if(TARGET database)
    target_link_libraries(pacs_services PUBLIC database)
    target_include_directories(pacs_services PUBLIC ${PACS_DATABASE_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(pacs_services PUBLIC PACS_WITH_DATABASE_SYSTEM=1)
    # Suppress deprecated warnings from database_system (database_base is deprecated)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(pacs_services PRIVATE -Wno-deprecated-declarations)
    endif()
    message(STATUS "    database_system: linked for pacs_services (Issue #420)")
endif()

message(STATUS "  [OK] pacs_services: ON")

# Link pacs_services to pacs_client (job_manager uses SCU classes)
# This must be done after pacs_services is defined
target_link_libraries(pacs_client PUBLIC pacs_services)

# Security library
# Base security sources (always included)
set(PACS_SECURITY_SOURCES
    src/security/access_control_manager.cpp
    src/security/tag_action.cpp
    src/security/uid_mapping.cpp
    src/security/anonymizer.cpp
)

# Digital signature sources (requires OpenSSL - Issue #191)
if(PACS_OPENSSL_FOUND)
    list(APPEND PACS_SECURITY_SOURCES
        src/security/certificate.cpp
        src/security/digital_signature.cpp
    )
endif()

add_library(pacs_security ${PACS_SECURITY_SOURCES})
target_include_directories(pacs_security
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link pacs_core for dicom_dataset
target_link_libraries(pacs_security PUBLIC pacs_core pacs_encoding)

# Link common_system (Tier 0)
if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(pacs_security PUBLIC ${PACS_COMMON_SYSTEM_INCLUDE_DIR})
    # Use KCENON_HAS_COMMON_SYSTEM for unified feature flag naming
    # Keep PACS_WITH_COMMON_SYSTEM as legacy alias for migration window
    target_compile_definitions(pacs_security PUBLIC
        KCENON_HAS_COMMON_SYSTEM=1
        PACS_WITH_COMMON_SYSTEM=1
    )
endif()

# Link OpenSSL for digital signatures (Issue #191)
if(PACS_OPENSSL_FOUND)
    target_link_libraries(pacs_security PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(pacs_security PUBLIC PACS_WITH_DIGITAL_SIGNATURES)
    message(STATUS "  [OK] pacs_security: ON (with digital signatures)")
else()
    message(STATUS "  [OK] pacs_security: ON (without digital signatures)")
endif()

# Storage library
if(PACS_BUILD_STORAGE AND SQLITE3_FOUND)
    add_library(pacs_storage
        src/storage/storage_interface.cpp
        src/storage/file_storage.cpp
        src/storage/s3_storage.cpp
        src/storage/azure_blob_storage.cpp
        src/storage/hsm_storage.cpp
        src/storage/hsm_migration_service.cpp
        src/storage/sqlite_security_storage.cpp
        src/storage/migration_runner.cpp
        src/storage/index_database.cpp
        src/storage/node_repository.cpp
        src/storage/job_repository.cpp
        src/storage/routing_repository.cpp
        src/storage/prefetch_repository.cpp
        src/storage/sync_repository.cpp
        src/storage/annotation_repository.cpp
        src/storage/measurement_repository.cpp
        src/storage/key_image_repository.cpp
        src/storage/viewer_state_repository.cpp
        src/storage/pacs_database_adapter.cpp
        src/storage/repository_factory.cpp
    )
    target_include_directories(pacs_storage
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    # Link common_system for Result<T>
    if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(pacs_storage PUBLIC ${PACS_COMMON_SYSTEM_INCLUDE_DIR})
        # Use KCENON_HAS_COMMON_SYSTEM for unified feature flag naming
        # Keep PACS_WITH_COMMON_SYSTEM as legacy alias for migration window
        target_compile_definitions(pacs_storage PUBLIC
            KCENON_HAS_COMMON_SYSTEM=1
            PACS_WITH_COMMON_SYSTEM=1
        )
    endif()

    # Link SQLite3
    if(SQLITE3_FROM_FETCH)
        target_link_libraries(pacs_storage PUBLIC sqlite3_lib)
    else()
        target_link_libraries(pacs_storage PUBLIC SQLite::SQLite3)
    endif()

    # Link pacs_core for dicom_dataset and pacs_encoding for transfer_syntax
    target_link_libraries(pacs_storage PUBLIC pacs_core pacs_encoding pacs_security)

    # Link database_system for secure query building (SQL injection prevention)
    if(TARGET database)
        target_link_libraries(pacs_storage PUBLIC database)
        # Link integrated_database for unified_database_system (Issue #606)
        if(TARGET integrated_database)
            target_link_libraries(pacs_storage PUBLIC integrated_database)
        endif()
        target_include_directories(pacs_storage PUBLIC ${PACS_DATABASE_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(pacs_storage PUBLIC PACS_WITH_DATABASE_SYSTEM=1)
        # Suppress deprecated warnings from database_system (database_base is deprecated)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(pacs_storage PRIVATE -Wno-deprecated-declarations)
        endif()
        message(STATUS "    database_system: linked (SQL injection protection enabled)")
    endif()

    # Link fmt library for format compatibility (if needed)
    if(PACS_FMT_NEEDED AND PACS_FMT_TARGET)
        target_link_libraries(pacs_storage PUBLIC ${PACS_FMT_TARGET})
    endif()

    message(STATUS "  [OK] pacs_storage: ON (SQLite3)")
else()
    message(STATUS "  [--] pacs_storage: OFF")
endif()

# AI library (AI result handler for SR/SEG/PR objects - Issue #204,
#             AI service connector for inference requests - Issue #205)
# Requires pacs_storage for storing AI-generated DICOM objects
# Requires pacs_integration for logger/monitoring adapters
if(TARGET pacs_storage)
    add_library(pacs_ai
        src/ai/ai_result_handler.cpp
        src/ai/ai_service_connector.cpp
    )
    target_include_directories(pacs_ai
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(pacs_ai
        PUBLIC pacs_core pacs_storage
    )

    # Link common_system for Result<T>
    if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(pacs_ai PUBLIC ${PACS_COMMON_SYSTEM_INCLUDE_DIR})
        # Use KCENON_HAS_COMMON_SYSTEM for unified feature flag naming
        # Keep PACS_WITH_COMMON_SYSTEM as legacy alias for migration window
        target_compile_definitions(pacs_ai PUBLIC
            KCENON_HAS_COMMON_SYSTEM=1
            PACS_WITH_COMMON_SYSTEM=1
        )
    endif()

    # Note: pacs_integration is linked later after it's defined (for ai_service_connector)
    # This is handled in the pacs_integration section below

    message(STATUS "  [OK] pacs_ai: ON (AI result handler, AI service connector)")
else()
    message(STATUS "  [--] pacs_ai: OFF (requires pacs_storage)")
endif()

# Monitoring library (health check endpoint - Issue #211, metrics - Issue #210)
# Requires pacs_storage for file_storage and index_database checks
if(TARGET pacs_storage)
    add_library(pacs_monitoring
        src/monitoring/health_checker.cpp
        src/monitoring/pacs_metrics.cpp
    )
    target_include_directories(pacs_monitoring
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(pacs_monitoring
        PUBLIC pacs_storage
    )

    message(STATUS "  [OK] pacs_monitoring: ON (health check)")
else()
    message(STATUS "  [--] pacs_monitoring: OFF (requires pacs_storage)")
endif()

# Workflow library (auto prefetch service - Issue #206, task scheduler - Issue #207)
# Requires pacs_storage for index_database and pacs_integration for adapters
if(TARGET pacs_storage)
    add_library(pacs_workflow
        src/workflow/auto_prefetch_service.cpp
        src/workflow/task_scheduler.cpp
        src/workflow/study_lock_manager.cpp
    )
    target_include_directories(pacs_workflow
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(pacs_workflow
        PUBLIC pacs_storage pacs_services
    )

    # Link common_system for Result<T>
    if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(pacs_workflow PUBLIC ${PACS_COMMON_SYSTEM_INCLUDE_DIR})
        # Use KCENON_HAS_COMMON_SYSTEM for unified feature flag naming
        # Keep PACS_WITH_COMMON_SYSTEM as legacy alias for migration window
        target_compile_definitions(pacs_workflow PUBLIC
            KCENON_HAS_COMMON_SYSTEM=1
            PACS_WITH_COMMON_SYSTEM=1
        )
    endif()

    message(STATUS "  [OK] pacs_workflow: ON (auto prefetch service)")
else()
    message(STATUS "  [--] pacs_workflow: OFF (requires pacs_storage)")
endif()

# Web library (REST API server - Issue #194)
# Requires Crow framework (fetched above)
if(TARGET Crow::Crow)
    add_library(pacs_web
        src/web/rest_server.cpp
        src/web/thumbnail_service.cpp
        src/web/metadata_service.cpp
        src/web/endpoints/system_endpoints.cpp
        src/web/endpoints/security_endpoints.cpp
        src/web/endpoints/patient_endpoints.cpp
        src/web/endpoints/study_endpoints.cpp
        src/web/endpoints/series_endpoints.cpp
        src/web/endpoints/worklist_endpoints.cpp
        src/web/endpoints/audit_endpoints.cpp
        src/web/endpoints/association_endpoints.cpp
        src/web/endpoints/dicomweb_endpoints.cpp
        src/web/endpoints/remote_nodes_endpoints.cpp
        src/web/endpoints/jobs_endpoints.cpp
        src/web/endpoints/routing_endpoints.cpp
        src/web/endpoints/thumbnail_endpoints.cpp
        src/web/endpoints/metadata_endpoints.cpp
        src/web/endpoints/annotation_endpoints.cpp
        src/web/endpoints/measurement_endpoints.cpp
        src/web/endpoints/key_image_endpoints.cpp
        src/web/endpoints/viewer_state_endpoints.cpp
    )
    target_include_directories(pacs_web
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(pacs_web
        PUBLIC pacs_core pacs_storage pacs_client Crow::Crow
    )

    # Link pacs_monitoring if available
    if(TARGET pacs_monitoring)
        target_link_libraries(pacs_web PUBLIC pacs_monitoring)
        target_compile_definitions(pacs_web PUBLIC PACS_WITH_MONITORING)
    endif()

    # Link fmt library for format compatibility (if needed)
    if(PACS_FMT_NEEDED AND PACS_FMT_TARGET)
        target_link_libraries(pacs_web PUBLIC ${PACS_FMT_TARGET})
    endif()

    message(STATUS "  [OK] pacs_web: ON (REST API server)")
else()
    message(STATUS "  [--] pacs_web: OFF (requires Crow)")
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
        src/integration/thread_pool_adapter.cpp
        src/integration/executor_adapter.cpp
        src/integration/network_adapter.cpp
        src/integration/dicom_session.cpp
    )

    # Only include monitoring_adapter.cpp if monitoring_system is available
    if(MONITORING_SYSTEM_AVAILABLE)
        list(APPEND PACS_INTEGRATION_SOURCES src/integration/monitoring_adapter.cpp)
    endif()

    # Only include itk_adapter.cpp if ITK is available (Issue #463)
    if(PACS_ITK_FOUND)
        list(APPEND PACS_INTEGRATION_SOURCES src/integration/itk_adapter.cpp)
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
        # Use KCENON_HAS_COMMON_SYSTEM for unified feature flag naming
        # Keep PACS_WITH_COMMON_SYSTEM as legacy alias for migration window
        # KCENON_HAS_COMMON_EXECUTOR is required for thread_system ABI compatibility
        target_compile_definitions(pacs_integration PUBLIC
            KCENON_HAS_COMMON_SYSTEM=1
            PACS_WITH_COMMON_SYSTEM=1
            KCENON_HAS_COMMON_EXECUTOR=1
        )
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
            elseif(PACS_COMMON_SYSTEM_INCLUDE_DIR)
                add_library(kcenon_common_system_shim INTERFACE)
                target_include_directories(kcenon_common_system_shim INTERFACE ${PACS_COMMON_SYSTEM_INCLUDE_DIR})
                add_library(kcenon::common_system ALIAS kcenon_common_system_shim)
                message(STATUS "Created kcenon::common_system shim for monitoring_system compatibility")
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

    # Link ITK for itk_adapter (Issue #463)
    if(PACS_ITK_FOUND)
        target_link_libraries(pacs_integration PUBLIC ${ITK_LIBRARIES})
        target_compile_definitions(pacs_integration PUBLIC PACS_WITH_ITK)
        message(STATUS "    - itk_adapter: ON (ITK ${ITK_VERSION})")
    else()
        message(STATUS "    - itk_adapter: OFF (ITK not found or disabled)")
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

    # Link pacs_integration to pacs_ai for ai_service_connector (Issue #205)
    # ai_service_connector uses logger_adapter and monitoring_adapter
    if(TARGET pacs_ai)
        target_link_libraries(pacs_ai PUBLIC pacs_integration)
        message(STATUS "    - pacs_ai: linked pacs_integration for ai_service_connector")
    endif()

    # Link pacs_integration to pacs_workflow for auto_prefetch_service (Issue #206)
    # auto_prefetch_service uses logger_adapter, monitoring_adapter, and thread_adapter
    if(TARGET pacs_workflow)
        target_link_libraries(pacs_workflow PUBLIC pacs_integration)
        message(STATUS "    - pacs_workflow: linked pacs_integration for auto_prefetch_service")
    endif()
else()
    message(STATUS "  [--] pacs_integration: OFF (requires container_system and network_system)")
endif()

##################################################
# C++20 Module Support
##################################################
# Enable C++20 modules with CMake 3.28+ when building with module-capable compilers.
# This provides an alternative to the header-based interface.
#
# Usage:
#   cmake -DPACS_BUILD_MODULES=ON ..
#
# Requirements:
#   - CMake 3.28 or later
#   - Clang 16+, GCC 14+, or MSVC 2022 17.4+
##################################################

if(PACS_BUILD_MODULES)
    # Check CMake version
    if(CMAKE_VERSION VERSION_LESS "3.28")
        message(WARNING "C++20 modules require CMake 3.28+. Disabling module build.")
        set(PACS_BUILD_MODULES OFF)
    else()
        message(STATUS "")
        message(STATUS "=== C++20 Module Build ===")

        # Create module library target
        add_library(pacs_system_modules)
        add_library(kcenon::pacs_modules ALIAS pacs_system_modules)

        # Set C++20 standard for module target
        target_compile_features(pacs_system_modules PUBLIC cxx_std_20)

        # Enable module scanning
        set_target_properties(pacs_system_modules PROPERTIES
            CXX_SCAN_FOR_MODULES ON
        )

        # Add module source files
        target_sources(pacs_system_modules
            PUBLIC FILE_SET CXX_MODULES
            FILES
                # Primary module
                src/modules/pacs.cppm
                # Tier 1: Core partition
                src/modules/pacs-core.cppm
                # Tier 2: Infrastructure partitions
                src/modules/pacs-encoding.cppm
                src/modules/pacs-storage.cppm
                src/modules/pacs-security.cppm
                # Tier 3: Protocol partition
                src/modules/pacs-network.cppm
                # Tier 4: Services partition
                src/modules/pacs-services.cppm
                # Tier 5: Application partitions
                src/modules/pacs-workflow.cppm
                src/modules/pacs-web.cppm
                src/modules/pacs-integration.cppm
                # Tier 6: Optional partitions
                src/modules/pacs-ai.cppm
                src/modules/pacs-monitoring.cppm
                src/modules/pacs-di.cppm
        )

        # Include directories for module target
        target_include_directories(pacs_system_modules PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )

        # Link required libraries
        target_link_libraries(pacs_system_modules PUBLIC
            pacs_core
            pacs_encoding
            pacs_network
            pacs_services
            pacs_security
        )

        # Conditionally link optional libraries
        if(TARGET pacs_storage)
            target_link_libraries(pacs_system_modules PUBLIC pacs_storage)
        endif()
        if(TARGET pacs_workflow)
            target_link_libraries(pacs_system_modules PUBLIC pacs_workflow)
        endif()
        if(TARGET pacs_web)
            target_link_libraries(pacs_system_modules PUBLIC pacs_web)
        endif()
        if(TARGET pacs_integration)
            target_link_libraries(pacs_system_modules PUBLIC pacs_integration)
        endif()
        if(TARGET pacs_ai)
            target_link_libraries(pacs_system_modules PUBLIC pacs_ai)
        endif()
        if(TARGET pacs_monitoring)
            target_link_libraries(pacs_system_modules PUBLIC pacs_monitoring)
        endif()

        # Compile definitions
        target_compile_definitions(pacs_system_modules PUBLIC
            KCENON_USE_MODULES=1
        )

        message(STATUS "  C++20 module target: pacs_system_modules")
        message(STATUS "  Module partitions: 12")
        message(STATUS "==============================")
    endif()
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
        tests/core/events_test.cpp
    )
    target_link_libraries(core_tests
        PRIVATE
            pacs_core
            pacs_encoding
            Catch2::Catch2WithMain
    )
    target_include_directories(core_tests PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})

    # Utilities tests (dcm_modify, etc.)
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/utilities)
    add_executable(utilities_tests
        tests/utilities/dcm_modify_test.cpp
    )
    target_link_libraries(utilities_tests
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
        tests/encoding/compression/jpeg_baseline_codec_test.cpp
        tests/encoding/compression/jpeg_lossless_codec_test.cpp
        tests/encoding/compression/jpeg2000_codec_test.cpp
        tests/encoding/compression/jpeg_ls_codec_test.cpp
        tests/encoding/compression/rle_codec_test.cpp
        tests/encoding/simd/simd_rle_test.cpp
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
        tests/network/v2/dicom_server_v2_test.cpp
        tests/network/v2/pdu_framing_test.cpp
        tests/network/v2/state_machine_test.cpp
        tests/network/v2/service_dispatching_test.cpp
        tests/network/v2/network_system_integration_test.cpp
        tests/network/v2/stress_test.cpp
        # Pipeline tests (Issue #524)
        tests/network/pipeline/pipeline_job_types_test.cpp
        tests/network/pipeline/pipeline_coordinator_test.cpp
        tests/network/pipeline/pipeline_integration_test.cpp
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
        tests/services/retrieve_scu_test.cpp
        tests/services/query_scp_test.cpp
        tests/services/retrieve_scp_test.cpp
        tests/services/worklist_scp_test.cpp
        tests/services/worklist_scu_test.cpp
        tests/services/mpps_scp_test.cpp
        tests/services/mpps_scu_test.cpp
        tests/services/us_storage_test.cpp
        tests/services/dx_storage_test.cpp
        tests/services/mg_storage_test.cpp
        tests/services/pet_storage_test.cpp
        tests/services/nm_storage_test.cpp
        tests/services/mg_iod_validator_test.cpp
        tests/services/pet_iod_validator_test.cpp
        tests/services/nm_iod_validator_test.cpp
        tests/services/rt_storage_test.cpp
        tests/services/rt_iod_validator_test.cpp
        tests/services/seg_storage_test.cpp
        tests/services/sr_storage_test.cpp
        tests/services/cache/simple_lru_cache_test.cpp
        tests/services/cache/query_cache_test.cpp
        tests/services/cache/streaming_query_test.cpp
        tests/services/cache/parallel_query_executor_test.cpp
    )
    target_link_libraries(services_tests
        PRIVATE
            pacs_services
            pacs_storage
            Catch2::Catch2WithMain
    )
    # Link pacs_integration for thread_adapter (used by parallel_query_executor tests)
    if(TARGET pacs_integration)
        target_link_libraries(services_tests PRIVATE pacs_integration)
    endif()

    # Security tests (Issue #193 - RBAC, Issue #191 - Digital Signatures, Issue #192 - Anonymization)
    set(SECURITY_TEST_SOURCES
        tests/security/access_control_manager_test.cpp
        tests/security/sqlite_security_storage_test.cpp
        tests/security/uid_mapping_test.cpp
        tests/security/anonymizer_test.cpp
    )

    # Add digital signature tests if OpenSSL is available (Issue #191)
    if(PACS_OPENSSL_FOUND)
        list(APPEND SECURITY_TEST_SOURCES
            tests/security/digital_signature_test.cpp
        )
    endif()

    add_executable(security_tests ${SECURITY_TEST_SOURCES})
    target_link_libraries(security_tests
        PRIVATE
            pacs_security
            pacs_storage
            Catch2::Catch2WithMain
    )

    # Storage tests
    if(TARGET pacs_storage)
        add_executable(storage_tests
            tests/storage/storage_interface_test.cpp
            tests/storage/file_storage_test.cpp
            tests/storage/s3_storage_test.cpp
            tests/storage/azure_blob_storage_test.cpp
            tests/storage/hsm_storage_test.cpp
            tests/storage/migration_runner_test.cpp
            tests/storage/index_database_test.cpp
            tests/storage/mpps_test.cpp
            tests/storage/worklist_test.cpp
            tests/storage/pacs_database_adapter_test.cpp
            tests/storage/base_repository_test.cpp
            tests/storage/repository_factory_test.cpp
            # Annotation & Measurement API integration tests (Issue #545, #584)
            tests/integration/annotation_api_test.cpp
            tests/integration/measurement_api_test.cpp
        )
        target_link_libraries(storage_tests
            PRIVATE
                pacs_storage
                Catch2::Catch2WithMain
                $<$<BOOL:${fmt_FOUND}>:fmt::fmt>
                $<$<TARGET_EXISTS:database>:database>
        )
    endif()

    # AI tests (Issue #204 - AI result handler for SR/SEG/PR objects,
    #           Issue #205 - AI service connector for inference requests)
    if(TARGET pacs_ai)
        add_executable(ai_tests
            tests/ai/ai_result_handler_test.cpp
            tests/ai/ai_service_connector_test.cpp
        )
        target_link_libraries(ai_tests
            PRIVATE
                pacs_ai
                Catch2::Catch2WithMain
        )
        # Link pacs_integration for logger/monitoring adapters (required by ai_service_connector)
        if(TARGET pacs_integration)
            target_link_libraries(ai_tests PRIVATE pacs_integration)
        endif()
    endif()

    # Workflow tests (Issue #206 - auto prefetch service, Issue #207 - task scheduler)
    if(TARGET pacs_workflow)
        add_executable(workflow_tests
            tests/workflow/auto_prefetch_service_test.cpp
            tests/workflow/task_scheduler_test.cpp
            tests/workflow/study_lock_manager_test.cpp
        )
        target_link_libraries(workflow_tests
            PRIVATE
                pacs_workflow
                Catch2::Catch2WithMain
        )
        # Link pacs_integration for logger/monitoring/thread adapters
        if(TARGET pacs_integration)
            target_link_libraries(workflow_tests PRIVATE pacs_integration)
        endif()
    endif()

    # Monitoring tests (Issue #211 - health check endpoint, Issue #210 - metrics)
    if(TARGET pacs_monitoring)
        add_executable(monitoring_tests
            tests/monitoring/health_status_test.cpp
            tests/monitoring/health_checker_test.cpp
            tests/monitoring/health_json_test.cpp
            tests/monitoring/pacs_metrics_test.cpp
            tests/monitoring/collectors_test.cpp
        )
        target_link_libraries(monitoring_tests
            PRIVATE
                pacs_monitoring
                Catch2::Catch2WithMain
        )
    endif()

    # Web tests (Issue #194 - REST API server)
    if(TARGET pacs_web)
        add_executable(web_tests
            tests/web/rest_server_test.cpp
            tests/web/system_endpoints_test.cpp
            tests/web/patient_study_endpoints_test.cpp
            tests/web/worklist_audit_endpoints_test.cpp
            tests/web/dicomweb_endpoints_test.cpp
            tests/web/jobs_endpoints_test.cpp
            tests/web/thumbnail_service_test.cpp
            tests/web/metadata_service_test.cpp
            tests/web/annotation_endpoints_test.cpp
            tests/web/measurement_endpoints_test.cpp
            tests/web/key_image_endpoints_test.cpp
            tests/web/viewer_state_endpoints_test.cpp
        )
        target_link_libraries(web_tests
            PRIVATE
                pacs_web
                pacs_services
                pacs_client
                Catch2::Catch2WithMain
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
            # Cross-system integration tests (Issue #390)
            tests/integration/dicom_workflow_integration_test.cpp
            tests/integration/error_propagation_integration_test.cpp
            tests/integration/load_integration_test.cpp
            tests/integration/shutdown_integration_test.cpp
            tests/integration/config_reload_integration_test.cpp
            # ITK/VTK Integration Adapter tests (Issue #463)
            tests/integration/itk_adapter_test.cpp
        )
        target_link_libraries(pacs_integration_tests
            PRIVATE
                pacs_integration
                pacs_network
                Catch2::Catch2WithMain
        )
        # Link ITK libraries for ITK adapter tests (Issue #463)
        if(PACS_ITK_FOUND)
            target_link_libraries(pacs_integration_tests PRIVATE ${ITK_LIBRARIES})
        endif()
    endif()

    # DI tests (Issue #312 - ServiceContainer based DI Integration)
    # (Issue #309 - Full Adoption of ILogger Interface)
    if(TARGET pacs_integration AND TARGET pacs_storage)
        add_executable(di_tests
            tests/di/service_registration_test.cpp
            tests/di/ilogger_test.cpp
        )
        target_link_libraries(di_tests
            PRIVATE
                pacs_core
                pacs_storage
                pacs_network
                pacs_integration
                pacs_encoding
                pacs_services
                Catch2::Catch2WithMain
        )
        if(TARGET thread_system_lib)
            target_link_libraries(di_tests PRIVATE thread_system_lib)
        endif()
        if(TARGET common_system_lib)
            target_link_libraries(di_tests PRIVATE common_system_lib)
        endif()
    endif()

    # Client tests (Issue #554 - Job Manager, Issue #539 - Routing Manager, Issue #541 - Prefetch Manager, Issue #542 - Sync Manager)
    if(TARGET pacs_client AND TARGET pacs_storage)
        add_executable(client_tests
            tests/client/job_manager_test.cpp
            tests/client/routing_manager_test.cpp
            tests/client/prefetch_manager_test.cpp
            tests/client/sync_manager_test.cpp
        )
        target_link_libraries(client_tests
            PRIVATE
                pacs_client
                pacs_core
                pacs_storage
                pacs_network
                pacs_services
                Catch2::Catch2WithMain
        )
        if(TARGET pacs_integration)
            target_link_libraries(client_tests PRIVATE pacs_integration)
        endif()
        if(TARGET thread_system_lib)
            target_link_libraries(client_tests PRIVATE thread_system_lib)
        endif()
        if(TARGET common_system_lib)
            target_link_libraries(client_tests PRIVATE common_system_lib)
        endif()
    endif()

    # Concurrency tests (Issue #337 - Lock-free structure stress tests)
    if(TARGET thread_base OR TARGET ThreadSystem)
        add_executable(concurrency_tests
            tests/concurrency/lockfree_stress_test.cpp
        )
        target_link_libraries(concurrency_tests
            PRIVATE
                Catch2::Catch2WithMain
        )
        # Link thread_system for lockfree_queue
        if(TARGET ThreadSystem)
            target_link_libraries(concurrency_tests PRIVATE ThreadSystem)
        elseif(TARGET thread_base)
            target_link_libraries(concurrency_tests PRIVATE thread_base)
        endif()
        # Add thread_system include directory explicitly
        if(PACS_THREAD_SYSTEM_INCLUDE_DIR)
            target_include_directories(concurrency_tests PRIVATE ${PACS_THREAD_SYSTEM_INCLUDE_DIR})
        endif()
        # Suppress deprecated warnings
        target_compile_options(concurrency_tests PRIVATE
            $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-deprecated-declarations>
            $<$<CXX_COMPILER_ID:MSVC>:/wd4996>
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
    catch_discover_tests(utilities_tests
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
    if(TARGET ai_tests)
        catch_discover_tests(ai_tests
            PROPERTIES LABELS "unit"
        )
    endif()
    if(TARGET workflow_tests)
        catch_discover_tests(workflow_tests
            PROPERTIES LABELS "unit"
        )
    endif()
    if(TARGET monitoring_tests)
        catch_discover_tests(monitoring_tests
            PROPERTIES LABELS "unit"
        )
    endif()
    if(TARGET web_tests)
        catch_discover_tests(web_tests
            PROPERTIES LABELS "unit"
        )
    endif()
    
    if(TARGET security_tests)
        catch_discover_tests(security_tests
            PROPERTIES LABELS "unit"
        )
    endif()

    if(TARGET di_tests)
        catch_discover_tests(di_tests
            PROPERTIES LABELS "unit"
        )
    endif()

    if(TARGET client_tests)
        catch_discover_tests(client_tests
            PROPERTIES LABELS "unit"
        )
    endif()

    # Concurrency stress tests (Issue #337 - Lock-free structure stress tests)
    # TEST_PREFIX ensures these tests are excluded from unit test runs
    # Label "stress" allows separate CI runs for long-running tests
    if(TARGET concurrency_tests)
        catch_discover_tests(concurrency_tests
            TEST_PREFIX "stress::"
            PROPERTIES LABELS "stress"
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

    ##################################################
    # C++20 Module Tests (Issue #507)
    # Tests module import functionality when PACS_BUILD_MODULES=ON
    ##################################################
    if(PACS_BUILD_MODULES AND TARGET pacs_system_modules)
        file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/modules)
        add_executable(module_tests
            tests/modules/module_import_test.cpp
        )
        target_link_libraries(module_tests
            PRIVATE
                pacs_system_modules
                Catch2::Catch2WithMain
        )

        # Enable module scanning for test target
        set_target_properties(module_tests PROPERTIES
            CXX_SCAN_FOR_MODULES ON
        )

        # Register module tests with CTest
        catch_discover_tests(module_tests
            TEST_PREFIX "modules::"
            PROPERTIES LABELS "modules"
        )
        message(STATUS "  [OK] module_tests: C++20 module import tests")
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

    # DCM Info - DICOM file summary utility (no network dependencies)
    add_subdirectory(examples/dcm_info)
    message(STATUS "  [OK] dcm_info: DICOM file summary utility")

    # DCM Conv - Transfer Syntax conversion utility (no network dependencies)
    add_subdirectory(examples/dcm_conv)
    message(STATUS "  [OK] dcm_conv: Transfer Syntax conversion utility")

    # DCM Modify - DICOM tag modification utility (no network dependencies)
    add_subdirectory(examples/dcm_modify)
    message(STATUS "  [OK] dcm_modify: DICOM tag modification utility")

    # DCM to JSON - DICOM to JSON conversion utility (DICOM PS3.18)
    add_subdirectory(examples/dcm_to_json)
    message(STATUS "  [OK] dcm_to_json: DICOM to JSON conversion utility")

    # JSON to DCM - JSON to DICOM conversion utility (DICOM PS3.18)
    add_subdirectory(examples/json_to_dcm)
    message(STATUS "  [OK] json_to_dcm: JSON to DICOM conversion utility")

    # DCM to XML - DICOM to XML conversion utility (DICOM Native XML PS3.19)
    add_subdirectory(examples/dcm_to_xml)
    message(STATUS "  [OK] dcm_to_xml: DICOM to XML conversion utility")

    # XML to DCM - XML to DICOM conversion utility (DICOM Native XML PS3.19)
    add_subdirectory(examples/xml_to_dcm)
    message(STATUS "  [OK] xml_to_dcm: XML to DICOM conversion utility")

    # DCM Anonymize - DICOM de-identification utility (PS3.15)
    add_subdirectory(examples/dcm_anonymize)
    message(STATUS "  [OK] dcm_anonymize: DICOM de-identification utility")

    # DCM Dir - DICOMDIR creation/management utility (PS3.10)
    add_subdirectory(examples/dcm_dir)
    message(STATUS "  [OK] dcm_dir: DICOMDIR creation/management utility")

    # Img to DCM - Image to DICOM conversion utility
    add_subdirectory(examples/img_to_dcm)
    message(STATUS "  [OK] img_to_dcm: Image to DICOM conversion utility")

    # DCM Extract - DICOM pixel data extraction utility
    add_subdirectory(examples/dcm_extract)
    message(STATUS "  [OK] dcm_extract: DICOM pixel data extraction utility")

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

    # find_scu - dcmtk-compatible C-FIND SCU utility
    if(TARGET pacs_integration)
        add_subdirectory(examples/find_scu)
        message(STATUS "  [OK] find_scu: dcmtk-compatible C-FIND SCU utility")
    else()
        message(STATUS "  [--] find_scu: OFF (requires pacs_integration)")
    endif()

    # Retrieve SCU sample - DICOM C-MOVE/C-GET client
    if(TARGET pacs_integration)
        add_subdirectory(examples/retrieve_scu)
        message(STATUS "  [OK] retrieve_scu: DICOM C-MOVE/C-GET client")
    else()
        message(STATUS "  [--] retrieve_scu: OFF (requires pacs_integration)")
    endif()

    # move_scu - dcmtk-compatible C-MOVE SCU utility
    if(TARGET pacs_integration)
        add_subdirectory(examples/move_scu)
        message(STATUS "  [OK] move_scu: dcmtk-compatible C-MOVE SCU utility")
    else()
        message(STATUS "  [--] move_scu: OFF (requires pacs_integration)")
    endif()

    # get_scu - dcmtk-compatible C-GET SCU utility
    if(TARGET pacs_integration)
        add_subdirectory(examples/get_scu)
        message(STATUS "  [OK] get_scu: dcmtk-compatible C-GET SCU utility")
    else()
        message(STATUS "  [--] get_scu: OFF (requires pacs_integration)")
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

    # Query/Retrieve SCP - DICOM Q/R server (C-FIND, C-MOVE, C-GET)
    if(TARGET pacs_storage AND TARGET pacs_integration)
        add_subdirectory(examples/qr_scp)
        message(STATUS "  [OK] qr_scp: DICOM Query/Retrieve server")
    else()
        message(STATUS "  [--] qr_scp: OFF (requires pacs_storage and pacs_integration)")
    endif()

    # Modality Worklist SCP - DICOM MWL server
    if(TARGET pacs_integration)
        add_subdirectory(examples/worklist_scp)
        message(STATUS "  [OK] worklist_scp: Modality Worklist server")
    else()
        message(STATUS "  [--] worklist_scp: OFF (requires pacs_integration)")
    endif()

    # MPPS SCP - DICOM Modality Performed Procedure Step server
    if(TARGET pacs_integration)
        add_subdirectory(examples/mpps_scp)
        message(STATUS "  [OK] mpps_scp: MPPS N-CREATE/N-SET server")
    else()
        message(STATUS "  [--] mpps_scp: OFF (requires pacs_integration)")
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

    # C++20 Module Example (Issue #507)
    add_subdirectory(examples/module_example)
    if(PACS_BUILD_MODULES AND TARGET pacs_system_modules)
        message(STATUS "  [OK] module_example: C++20 module usage demonstration")
    else()
        message(STATUS "  [OK] module_example: C++20 module demo (fallback to headers)")
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

    # SIMD Performance Benchmarks
    # @see Issue #332 - Add SIMD optimization benchmarks
    # @see Issue #313 - Apply SIMD Optimization (Image Processing)
    if(TARGET pacs_encoding AND TARGET Catch2::Catch2WithMain)
        add_subdirectory(benchmarks/simd_performance)
        message(STATUS "  [OK] simd_performance_benchmarks: SIMD optimization measurement")
    else()
        message(STATUS "  [--] simd_performance_benchmarks: OFF (requires pacs_encoding and Catch2)")
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

# Suppress deprecated warnings from database_system for pacs_services (Issue #420)
if(TARGET database)
    target_compile_options(pacs_services PRIVATE
        $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-deprecated-declarations>
        $<$<CXX_COMPILER_ID:MSVC>:/wd4996>
    )
endif()

# Optional PACS libraries
if(TARGET pacs_storage)
    pacs_apply_warnings(pacs_storage)
endif()

if(TARGET pacs_client)
    pacs_apply_warnings(pacs_client)
endif()

if(TARGET pacs_ai)
    pacs_apply_warnings(pacs_ai)
endif()

if(TARGET pacs_monitoring)
    pacs_apply_warnings(pacs_monitoring)
endif()

if(TARGET pacs_workflow)
    pacs_apply_warnings(pacs_workflow)
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
        # Suppress deprecated warnings from database_system headers
        target_compile_options(storage_tests PRIVATE
            $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-deprecated-declarations>
            $<$<CXX_COMPILER_ID:MSVC>:/wd4996>
        )
    endif()
    if(TARGET ai_tests)
        pacs_apply_warnings(ai_tests)
    endif()
    if(TARGET workflow_tests)
        pacs_apply_warnings(workflow_tests)
    endif()
    if(TARGET monitoring_tests)
        pacs_apply_warnings(monitoring_tests)
    endif()
    if(TARGET pacs_integration_tests)
        pacs_apply_warnings(pacs_integration_tests)
    endif()
    if(TARGET di_tests)
        pacs_apply_warnings(di_tests)
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
# Developer Samples
##################################################

if(PACS_BUILD_SAMPLES)
    message(STATUS "")
    message(STATUS "=== Building Developer Samples ===")
    add_subdirectory(samples)
    pacs_apply_warnings(pacs_samples_common)
    message(STATUS "Developer samples: ON")
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
if(PACS_BUILD_EXAMPLES AND TARGET pacs_server)
    message(STATUS "  Examples: ON (pacs_server)")
else()
    message(STATUS "  Examples: ${PACS_BUILD_EXAMPLES}")
endif()
message(STATUS "  Benchmarks: ${PACS_BUILD_BENCHMARKS}")
message(STATUS "  Samples: ${PACS_BUILD_SAMPLES}")
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
