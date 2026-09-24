# kcenon-pacs-system portfile
# Modern C++20 PACS implementation built on the kcenon ecosystem

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/pacs_system
    REF "v${VERSION}"
    SHA512 0db1f80c979f726efc6db5836e626d717297e6e4ead74005973d655ab63e67d093ed89afa402579d53e823a7c1bf5ac350ee199faf2f5b5e2cb70976ed4c282a
    HEAD_REF main
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        storage   PACS_BUILD_STORAGE
        codecs    PACS_BUILD_CODECS
        ssl       PACS_WITH_OPENSSL
        rest-api  PACS_WITH_REST_API
        aws       PACS_WITH_AWS_SDK
        azure     PACS_WITH_AZURE_SDK
        testing   PACS_BUILD_TESTS
        testing   PACS_BUILD_BENCHMARKS
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DPACS_BUILD_EXAMPLES=OFF
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

configure_file("${CMAKE_CURRENT_LIST_DIR}/usage" "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" COPYONLY)
