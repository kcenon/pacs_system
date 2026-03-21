# kcenon-pacs-system portfile
# Modern C++20 PACS implementation built on the kcenon ecosystem

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/pacs_system
    REF "v${VERSION}"
    SHA512 242dd7bc82f56e0267e4978dd406aebeebd1bb50e70f93a8c809d3474ae6de1d0e692cc8fbbbaa81dbb68e2642b48c14100b8d5daa5f99097287af2d9465904c
    HEAD_REF main
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        storage  PACS_BUILD_STORAGE
        aws      PACS_WITH_AWS_SDK
        azure    PACS_WITH_AZURE_SDK
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DPACS_BUILD_TESTS=OFF
        -DPACS_BUILD_EXAMPLES=OFF
        -DPACS_BUILD_BENCHMARKS=OFF
        -DPACS_BUILD_SAMPLES=OFF
        -DPACS_WITH_COMMON_SYSTEM=ON
        -DPACS_WITH_CONTAINER_SYSTEM=ON
        -DPACS_WITH_NETWORK_SYSTEM=ON
        -DPACS_BUILD_MODULES=OFF
        -DPACS_WARNINGS_AS_ERRORS=OFF
        -DBUILD_SHARED_LIBS=OFF
        # Disable all FetchContent fallbacks; all deps must be resolved via vcpkg
        -DPACS_FETCH_OPENJPH=OFF
        -DPACS_FETCH_CROW=OFF
        -DFETCHCONTENT_FULLY_DISCONNECTED=ON
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME pacs_system
    CONFIG_PATH lib/cmake/pacs_system
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
