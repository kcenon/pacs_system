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

# OpenJPH (for HTJ2K compression codec)
message(STATUS "")
message(STATUS "=== Finding OpenJPH (for HTJ2K compression) ===")

# Try to find system OpenJPH
find_package(openjph CONFIG QUIET)

if(openjph_FOUND)
    message(STATUS "Found system OpenJPH")
    set(PACS_HTJ2K_FOUND TRUE)
else()
    # Try pkg-config as fallback
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(OPENJPH QUIET openjph)
        if(OPENJPH_FOUND)
            message(STATUS "Found OpenJPH via pkg-config: ${OPENJPH_VERSION}")
            set(PACS_HTJ2K_FOUND TRUE)
        endif()
    endif()
endif()

option(PACS_FETCH_OPENJPH "Fetch OpenJPH via FetchContent when not found (disable for vcpkg builds)" ON)

if(NOT PACS_HTJ2K_FOUND AND PACS_FETCH_OPENJPH)
    # Fetch OpenJPH from GitHub as last resort
    message(STATUS "OpenJPH not found on system, fetching from GitHub...")
    include(FetchContent)
    set(OJPH_BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
    set(OJPH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(OJPH_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    # Preserve BUILD_SHARED_LIBS across FetchContent to prevent OpenJPH's
    # option(BUILD_SHARED_LIBS ...) from changing library type of our targets.
    # Must use CACHE FORCE because option() clears normal variables.
    set(_pacs_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        openjph
        GIT_REPOSITORY https://github.com/aous72/OpenJPH.git
        GIT_TAG        0.18.2
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(openjph)
    if(_pacs_saved_build_shared_libs)
        set(BUILD_SHARED_LIBS "${_pacs_saved_build_shared_libs}" CACHE BOOL "" FORCE)
    else()
        unset(BUILD_SHARED_LIBS CACHE)
    endif()
    if(TARGET openjphstatic)
        set(PACS_HTJ2K_FOUND TRUE)
        message(STATUS "OpenJPH fetched and built from source (static)")
    elseif(TARGET openjph)
        set(PACS_HTJ2K_FOUND TRUE)
        message(STATUS "OpenJPH fetched and built from source")
    else()
        message(WARNING
            "OpenJPH build failed!\n"
            "HTJ2K codec will be disabled.\n"
            "\n"
            "To enable HTJ2K support, install OpenJPH:\n"
            "  - Build from source: https://github.com/aous72/OpenJPH")
        set(PACS_HTJ2K_FOUND FALSE)
    endif()
    # FetchContent headers live at src/core/common/ojph_*.h (flat layout),
    # but installed OpenJPH uses openjph/ prefix (#include <openjph/ojph_*.h>).
    # Create a compatibility include directory with an openjph/ symlink so both
    # system-installed and FetchContent builds use the same #include paths.
    if(PACS_HTJ2K_FOUND)
        set(_openjph_compat_include "${CMAKE_BINARY_DIR}/_openjph_compat_include")
        file(MAKE_DIRECTORY "${_openjph_compat_include}")
        file(CREATE_LINK
            "${openjph_SOURCE_DIR}/src/core/common"
            "${_openjph_compat_include}/openjph"
            SYMBOLIC)
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

# ==========================================================================
# kcenon Ecosystem SOUP Version Registry (IEC 62304 §8.1.2)
# These SHA identifiers must match the pinned versions in:
#   .github/actions/checkout-kcenon-deps/action.yml
# ==========================================================================
set(PACS_SOUP_COMMON_SYSTEM_SHA     "47be5fd27650f0ebfa2d029ebed35a1c262e55c4")
set(PACS_SOUP_CONTAINER_SYSTEM_SHA  "0b95a0c10edf53315581b7f3fa458f0193e8c039")
set(PACS_SOUP_THREAD_SYSTEM_SHA     "3ea6fd55704f70b880b13da190064ddbce70dda8")
set(PACS_SOUP_LOGGER_SYSTEM_SHA     "66a00ccce9917a65179e1c42707af9f32d22361f")
set(PACS_SOUP_MONITORING_SYSTEM_SHA "0b562ea3c60a8a51da77a1847d87efba3d99ef64")
set(PACS_SOUP_NETWORK_SYSTEM_SHA    "c70026ab057e72a84a5031f24d2009f9ee641743")
set(PACS_SOUP_DATABASE_SYSTEM_SHA   "b90b0f3bb1e7c9719494724b4c900fd89199e513")

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

    # find_package() fallback for vcpkg/system installs
    if(NOT COMMON_SYSTEM_FOUND)
        find_package(common_system CONFIG QUIET)
        if(common_system_FOUND)
            message(STATUS "Found common_system via find_package(common_system)")
            set(COMMON_SYSTEM_FOUND TRUE)
            # Create compatibility target wrapping the installed package target
            if(NOT TARGET pacs_common_system_headers)
                add_library(pacs_common_system_headers INTERFACE)
                if(TARGET kcenon::common_system)
                    target_link_libraries(pacs_common_system_headers INTERFACE kcenon::common_system)
                elseif(TARGET common_system)
                    target_link_libraries(pacs_common_system_headers INTERFACE common_system)
                endif()
            endif()
            set(PACS_COMMON_SYSTEM_INCLUDE_DIR "FOUND_VIA_PACKAGE")
        endif()
    endif()

    if(NOT COMMON_SYSTEM_FOUND)
        message(FATAL_ERROR
            "common_system is REQUIRED (Tier 0).\n"
            "Could not find 'kcenon/common/patterns/result.h'.\n"
            "Please ensure common_system is available at:\n"
            "  - ../common_system/include\n"
            "  - Set COMMON_SYSTEM_ROOT environment variable\n"
            "  - Or install via vcpkg: vcpkg install kcenon-common-system")
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

    # find_package() fallback for vcpkg/system installs
    if(NOT CONTAINER_SYSTEM_FOUND AND NOT TARGET container_system)
        find_package(ContainerSystem CONFIG QUIET)
        if(ContainerSystem_FOUND)
            message(STATUS "Found container_system via find_package(ContainerSystem)")
            set(CONTAINER_SYSTEM_FOUND TRUE)
            if(NOT TARGET container_system)
                add_library(container_system INTERFACE)
                target_link_libraries(container_system INTERFACE ContainerSystem::container)
            endif()
        endif()
    endif()

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
            "  - Set CONTAINER_SYSTEM_ROOT environment variable\n"
            "  - Or install via vcpkg: vcpkg install kcenon-container-system")
    endif()
endif()

# Disable examples/samples for all dependencies fetched via FetchContent
# This prevents build errors from missing example source files in upstream repos
# (e.g., common_system's thread_integration_example.cpp doesn't exist but CMake
# tries to build it when thread_system target exists)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(COMMON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# thread_system (REQUIRED for thread_pool_adapter - Tier 1)
# NOTE: Must be added BEFORE logger_system to prevent FetchContent conflicts.
# logger_system depends on thread_system and will fetch it via UnifiedDependencies
# if targets don't exist. By adding thread_system first, we ensure the sibling
# directory version is used with correct include paths.
message(STATUS "")
message(STATUS "=== Finding thread_system (REQUIRED for thread_pool_adapter - Tier 1) ===")

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

# find_package() fallback for vcpkg/system installs
if(NOT THREAD_SYSTEM_FOUND AND NOT TARGET thread_base AND NOT TARGET ThreadSystem)
    find_package(ThreadSystem CONFIG QUIET)
    if(ThreadSystem_FOUND)
        message(STATUS "Found thread_system via find_package(ThreadSystem)")
        set(THREAD_SYSTEM_FOUND TRUE)
        set(PACS_THREAD_SYSTEM_INCLUDE_DIR "FOUND_VIA_PACKAGE")
        # Create non-namespaced target for compatibility with downstream checks
        if(NOT TARGET ThreadSystem AND TARGET ThreadSystem::thread_base)
            add_library(ThreadSystem INTERFACE)
            target_link_libraries(ThreadSystem INTERFACE ThreadSystem::thread_base)
        endif()
    endif()
endif()

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
        "thread_system is REQUIRED for thread_pool_adapter (Tier 1).\n"
        "thread_pool_adapter will be disabled.\n"
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

# find_package() fallback for vcpkg/system installs
if(NOT LOGGER_SYSTEM_FOUND AND NOT TARGET LoggerSystem)
    find_package(LoggerSystem CONFIG QUIET)
    if(LoggerSystem_FOUND)
        message(STATUS "Found logger_system via find_package(LoggerSystem)")
        set(LOGGER_SYSTEM_FOUND TRUE)
        set(PACS_LOGGER_SYSTEM_INCLUDE_DIR "FOUND_VIA_PACKAGE")
        # Create non-namespaced target for compatibility with downstream checks
        if(NOT TARGET LoggerSystem)
            if(TARGET LoggerSystem::LoggerSystem)
                add_library(LoggerSystem INTERFACE)
                target_link_libraries(LoggerSystem INTERFACE LoggerSystem::LoggerSystem)
            elseif(TARGET LoggerSystem::logger)
                add_library(LoggerSystem INTERFACE)
                target_link_libraries(LoggerSystem INTERFACE LoggerSystem::logger)
            endif()
        endif()
    endif()
endif()

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
        "  - Set LOGGER_SYSTEM_ROOT environment variable\n"
        "  - Or install via vcpkg: vcpkg install kcenon-logger-system")
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

    # find_package() fallback for vcpkg/system installs
    if(NOT DATABASE_SYSTEM_FOUND AND NOT TARGET database)
        find_package(DatabaseSystem CONFIG QUIET)
        if(DatabaseSystem_FOUND)
            message(STATUS "Found database_system via find_package(DatabaseSystem)")
            set(DATABASE_SYSTEM_FOUND TRUE)
            if(NOT TARGET database AND TARGET DatabaseSystem::database)
                add_library(database INTERFACE)
                target_link_libraries(database INTERFACE DatabaseSystem::database)
            endif()
        endif()
    endif()

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

# monitoring_system (OPTIONAL - Tier 3, for performance monitoring)
# NOTE: Must be added BEFORE pacs_storage so that TARGET monitoring_system exists
#       when pacs_storage links integrated_database + monitoring_system together.
if(PACS_BUILD_STORAGE)
    message(STATUS "")
    message(STATUS "=== Finding monitoring_system (OPTIONAL - Tier 3) ===")

    set(_PACS_MONITORING_PATHS
        "$ENV{MONITORING_SYSTEM_ROOT}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../monitoring_system"
        "${CMAKE_CURRENT_SOURCE_DIR}/monitoring_system"
    )
    set(MONITORING_SYSTEM_FOUND FALSE)
    foreach(_path ${_PACS_MONITORING_PATHS})
        if(EXISTS "${_path}/CMakeLists.txt" AND EXISTS "${_path}/include/kcenon/monitoring/core/performance_monitor.h")
            message(STATUS "Found monitoring_system source at: ${_path}")
            set(PACS_MONITORING_SYSTEM_DIR "${_path}")
            set(PACS_MONITORING_SYSTEM_INCLUDE_DIR "${_path}/include")
            set(MONITORING_SYSTEM_FOUND TRUE)
            break()
        endif()
    endforeach()

    # find_package() fallback for vcpkg/system installs
    if(NOT MONITORING_SYSTEM_FOUND AND NOT TARGET monitoring_system)
        find_package(monitoring_system CONFIG QUIET)
        if(monitoring_system_FOUND)
            message(STATUS "Found monitoring_system via find_package(monitoring_system)")
            set(MONITORING_SYSTEM_FOUND TRUE)
            set(PACS_MONITORING_SYSTEM_INCLUDE_DIR "FOUND_VIA_PACKAGE")
            # Create non-namespaced target for compatibility with downstream checks
            if(NOT TARGET monitoring_system AND TARGET monitoring_system::monitoring_system)
                add_library(monitoring_system INTERFACE)
                target_link_libraries(monitoring_system INTERFACE monitoring_system::monitoring_system)
            endif()
        endif()
    endif()

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
            elseif(PACS_COMMON_SYSTEM_INCLUDE_DIR AND NOT PACS_COMMON_SYSTEM_INCLUDE_DIR STREQUAL "FOUND_VIA_PACKAGE")
                add_library(kcenon_common_system_shim INTERFACE)
                target_include_directories(kcenon_common_system_shim INTERFACE ${PACS_COMMON_SYSTEM_INCLUDE_DIR})
                add_library(kcenon::common_system ALIAS kcenon_common_system_shim)
                message(STATUS "Created kcenon::common_system shim for monitoring_system compatibility")
            endif()
        endif()

        add_subdirectory("${PACS_MONITORING_SYSTEM_DIR}" "${CMAKE_BINARY_DIR}/monitoring_system_build")
        message(STATUS "  [OK] monitoring_system added as subdirectory")
    elseif(MONITORING_SYSTEM_FOUND)
        message(STATUS "  [OK] monitoring_system already available")
    else()
        message(STATUS "  [--] monitoring_system not found - performance monitoring disabled")
        message(STATUS "       Install monitoring_system at ../monitoring_system for database performance metrics")
    endif()

    message(STATUS "monitoring_system integration enabled")
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

    # find_package() fallback for vcpkg/system installs
    if(NOT NETWORK_SYSTEM_FOUND AND NOT TARGET NetworkSystem)
        find_package(NetworkSystem CONFIG QUIET)
        if(NetworkSystem_FOUND)
            message(STATUS "Found network_system via find_package(NetworkSystem)")
            set(NETWORK_SYSTEM_FOUND TRUE)
            # Create non-namespaced target for compatibility with downstream checks
            if(NOT TARGET NetworkSystem AND TARGET NetworkSystem::NetworkSystem)
                add_library(NetworkSystem INTERFACE)
                target_link_libraries(NetworkSystem INTERFACE NetworkSystem::NetworkSystem)
            endif()
        endif()
    endif()

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
            "  - Set NETWORK_SYSTEM_ROOT environment variable\n"
            "  - Or install via vcpkg: vcpkg install kcenon-network-system")
    endif()
endif()

# Crow Web Framework (for REST API - Issue #194)
message(STATUS "")
option(PACS_FETCH_CROW "Fetch Crow web framework via FetchContent (disable for vcpkg builds)" ON)

if(PACS_FETCH_CROW)
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
        GIT_TAG v1.3.1  # Pinned release; compatible with ASIO 1.30+ io_context (IEC 62304 §8.1.2)
        GIT_SHALLOW TRUE
    )
    set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CROW_ENABLE_SSL OFF CACHE BOOL "" FORCE)
    set(CROW_ENABLE_COMPRESSION OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(Crow)
    message(STATUS "  [OK] Crow (v1.3.1) fetched")
else()
    find_package(Crow CONFIG QUIET)
    if(Crow_FOUND)
        message(STATUS "  [OK] Crow found via find_package")
    else()
        message(STATUS "  [SKIP] Crow not found and PACS_FETCH_CROW=OFF")
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

if(PACS_COMMON_SYSTEM_INCLUDE_DIR AND NOT PACS_COMMON_SYSTEM_INCLUDE_DIR STREQUAL "FOUND_VIA_PACKAGE" AND NOT TARGET pacs_common_system_headers)
    add_library(pacs_common_system_headers INTERFACE)
    target_include_directories(pacs_common_system_headers INTERFACE
        "${PACS_COMMON_SYSTEM_INCLUDE_DIR}"
    )
endif()

if(PACS_THREAD_SYSTEM_INCLUDE_DIR AND NOT PACS_THREAD_SYSTEM_INCLUDE_DIR STREQUAL "FOUND_VIA_PACKAGE" AND NOT TARGET pacs_thread_system_headers)
    add_library(pacs_thread_system_headers INTERFACE)
    target_include_directories(pacs_thread_system_headers INTERFACE
        "${PACS_THREAD_SYSTEM_INCLUDE_DIR}"
    )
endif()

if(PACS_MONITORING_SYSTEM_INCLUDE_DIR AND NOT PACS_MONITORING_SYSTEM_INCLUDE_DIR STREQUAL "FOUND_VIA_PACKAGE" AND NOT TARGET pacs_monitoring_system_headers)
    add_library(pacs_monitoring_system_headers INTERFACE)
    target_include_directories(pacs_monitoring_system_headers INTERFACE
        "${PACS_MONITORING_SYSTEM_INCLUDE_DIR}"
    )
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
