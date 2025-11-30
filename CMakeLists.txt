cmake_minimum_required(VERSION 3.20)

project(pacs_system
    VERSION 0.1.0
    DESCRIPTION "Production-ready C++20 PACS implementation"
    LANGUAGES CXX
)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build settings
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Options
option(PACS_BUILD_TESTS "Build unit tests" ON)
option(PACS_BUILD_EXAMPLES "Build examples" OFF)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W4 /WX)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Core library
add_library(pacs_core
    src/core/dicom_tag.cpp
)
target_include_directories(pacs_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Encoding library
add_library(pacs_encoding
    src/encoding/transfer_syntax.cpp
)
target_include_directories(pacs_encoding
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(pacs_encoding
    PUBLIC pacs_core
)

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

    # Core tests
    add_executable(core_tests
        tests/core/dicom_tag_test.cpp
    )
    target_link_libraries(core_tests
        PRIVATE
            pacs_core
            Catch2::Catch2WithMain
    )

    # Encoding tests
    add_executable(encoding_tests
        tests/encoding/transfer_syntax_test.cpp
        tests/encoding/vr_type_test.cpp
    )
    target_link_libraries(encoding_tests
        PRIVATE
            pacs_encoding
            Catch2::Catch2WithMain
    )

    include(CTest)
    include(Catch)
    catch_discover_tests(core_tests)
    catch_discover_tests(encoding_tests)
endif()
