cmake_minimum_required(VERSION 3.16)
project(PACSSystem VERSION 0.1.0 LANGUAGES CXX)

# Work around CMake policy issues
cmake_policy(SET CMP0076 NEW)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

# Allow include directories during build
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_EXPORT_NO_PACKAGE_REGISTRY ON)
set(CMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY ON)

# Disable include directory checks for build
set(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION OFF)

# Set C++20 as the default standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Global linker flags to handle macOS-specific features
if(APPLE)
    # The -dead_strip flag removes unreferenced code and data, helping with library deduplication
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-dead_strip")
endif()

# Check for vcpkg toolchain
# if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
#   message(FATAL_ERROR "Please provide the path to vcpkg toolchain file using -DCMAKE_TOOLCHAIN_FILE=<path_to_vcpkg_toolchain>")
# endif()

# Set CMake module path
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Find fmt package explicitly before adding thread_system
find_package(fmt CONFIG QUIET)
if(fmt_FOUND)
    message(STATUS "Found fmt version ${fmt_VERSION}")
else()
    message(STATUS "fmt not found, using std::format")
endif()

# Force USE_STD_FORMAT to avoid fmt dependency issues
add_definitions(-DUSE_STD_FORMAT)
message(STATUS "Forcing USE_STD_FORMAT to avoid fmt dependency issues")

# Find thread_system from Sources/ directory
# Priority: Local Sources/ directory first, then embedded subfolder (for backwards compatibility)
set(_PACS_THREAD_SEARCH_PATHS
    "/Users/dongcheolshin/Sources/thread_system"                # macOS development (HIGHEST PRIORITY)
    "/home/${USER}/Sources/thread_system"                        # Linux development
    "${CMAKE_CURRENT_SOURCE_DIR}/thread_system"                  # Embedded subfolder (legacy, fallback)
)

set(_PACS_THREAD_SYSTEM_PATH "")
foreach(_path ${_PACS_THREAD_SEARCH_PATHS})
    if(EXISTS "${_path}/CMakeLists.txt")
        message(STATUS "Found thread_system at: ${_path}")
        set(_PACS_THREAD_SYSTEM_PATH "${_path}")
        break()
    endif()
endforeach()

if(NOT _PACS_THREAD_SYSTEM_PATH)
    message(FATAL_ERROR "thread_system not found in any search path")
endif()

message(STATUS "Using thread_system from: ${_PACS_THREAD_SYSTEM_PATH}")

# Use thread_system from the project instead of internal thread_pool
# Note: thread_system now uses target-based include directory propagation
# via target_link_libraries, so no manual include_directories needed
add_subdirectory(${_PACS_THREAD_SYSTEM_PATH} ${CMAKE_BINARY_DIR}/thread_system_build)

# Find DCMTK package
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
find_package(DCMTK CONFIG REQUIRED)
if(DCMTK_FOUND)
    message(STATUS "Found DCMTK version ${DCMTK_VERSION}")
    message(STATUS "DCMTK include dirs: ${DCMTK_INCLUDE_DIRS}")
    message(STATUS "DCMTK libraries: ${DCMTK_LIBRARIES}")
else()
    message(STATUS "DCMTK not found, trying manual search")
    # Try to find DCMTK manually
    find_path(DCMTK_INCLUDE_DIR dcmtk/config/osconfig.h
        PATHS
        /opt/homebrew/opt/dcmtk/include
        /opt/homebrew/include
        /usr/local/include
        /usr/include
        NO_DEFAULT_PATH
    )
    if(DCMTK_INCLUDE_DIR)
        message(STATUS "Found DCMTK includes at: ${DCMTK_INCLUDE_DIR}")
        set(DCMTK_FOUND TRUE)
        set(DCMTK_INCLUDE_DIRS ${DCMTK_INCLUDE_DIR})
        # Try to find libraries
        find_library(DCMTK_dcmdata_LIBRARY NAMES dcmdata
            PATHS
            /opt/homebrew/opt/dcmtk/lib
            /opt/homebrew/lib
            /usr/local/lib
            /usr/lib
        )
        if(DCMTK_dcmdata_LIBRARY)
            message(STATUS "Found DCMTK dcmdata library at: ${DCMTK_dcmdata_LIBRARY}")
        endif()
    else()
        message(STATUS "DCMTK not found, using placeholder")
    endif()
endif()

# Set DCMTK variables
if(DCMTK_FOUND)
    set(USE_DCMTK_PLACEHOLDER FALSE)
    # Check if we have the proper CMake targets
    if(NOT TARGET DCMTK::DCMTK)
        # We found headers but not the CMake config, so set DCMTK_FOUND to FALSE
        set(DCMTK_FOUND FALSE)
        set(USE_DCMTK_PLACEHOLDER TRUE)
        message(STATUS "DCMTK headers found but CMake targets not available, using placeholder")
    endif()
else()
    set(USE_DCMTK_PLACEHOLDER TRUE)
endif()

# Add DCMTK definitions
# add_definitions(-DHAVE_CONFIG_H)  # Commented out as osconfig.h is not found

# Find additional required packages
find_package(ZLIB QUIET)
find_package(OpenSSL QUIET)
find_package(LibXml2 QUIET)

# Add subdirectories for each module
add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(modules)
add_subdirectory(apps)

# Add tests if enabled
option(BUILD_TESTS "Build test programs" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Package configuration
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install targets
set(PACS_INSTALL_TARGETS
    pacs_core
    pacs_common
    pacs_config
    pacs_logger
    pacs_security
    pacs_audit
    pacs_storage
    pacs_network
    pacs_database
    pacs_core_result
    pacs_interfaces
    pacs_thread
    pacs_api
    pacs_version
    pacs_monitoring
    # Note: thread_system targets (logger, thread_base, utilities) are external dependencies
    # and should not be installed as part of pacs_system
)

if(TARGET pacs_dicom)
    list(APPEND PACS_INSTALL_TARGETS pacs_dicom)
endif()

install(TARGETS ${PACS_INSTALL_TARGETS}
    EXPORT PACSSystemTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY 
    ${CMAKE_CURRENT_SOURCE_DIR}/common/
    ${CMAKE_CURRENT_SOURCE_DIR}/core/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pacs
    FILES_MATCHING PATTERN "*.h"
)

# Export targets
install(EXPORT PACSSystemTargets
    FILE PACSSystemTargets.cmake
    NAMESPACE pacs::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PACSSystem
)

# Create version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/PACSSystemConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Configure config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PACSSystemConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/PACSSystemConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PACSSystem
)

# Install config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/PACSSystemConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/PACSSystemConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PACSSystem
)

# Export for build tree
export(EXPORT PACSSystemTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/PACSSystemTargets.cmake"
    NAMESPACE pacs::
)

# Register package
export(PACKAGE PACSSystem)