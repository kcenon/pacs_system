# kcenon-pacs-system portfile
# Modern C++20 PACS implementation for DICOM medical imaging

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/pacs_system
    REF v0.1.0
    SHA512 0  # TODO: Update with actual SHA512 hash after release
    HEAD_REF main
)

# Feature-based option selection
set(PACS_STORAGE OFF)
if("storage" IN_LIST FEATURES)
    set(PACS_STORAGE ON)
endif()

set(PACS_AWS OFF)
if("aws" IN_LIST FEATURES)
    set(PACS_AWS ON)
endif()

set(PACS_AZURE OFF)
if("azure" IN_LIST FEATURES)
    set(PACS_AZURE ON)
endif()

set(PACS_TESTS OFF)
if("testing" IN_LIST FEATURES)
    set(PACS_TESTS ON)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DPACS_BUILD_STORAGE=${PACS_STORAGE}
        -DPACS_WITH_AWS_SDK=${PACS_AWS}
        -DPACS_WITH_AZURE_SDK=${PACS_AZURE}
        -DPACS_BUILD_TESTS=${PACS_TESTS}
        -DPACS_BUILD_EXAMPLES=OFF
        -DPACS_BUILD_BENCHMARKS=OFF
        -DPACS_BUILD_SAMPLES=OFF
        -DPACS_WARNINGS_AS_ERRORS=OFF
        -DPACS_BUILD_MODULES=OFF
        -DBUILD_SHARED_LIBS=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME pacs_system
    CONFIG_PATH lib/cmake/pacs_system
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
