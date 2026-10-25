#!/bin/bash
# scripts/check-source-install.sh
#
# Perform the supported source install (docs/PACKAGE_STATE.md, "Source install")
# and check that an external project consumes it with find_package only:
#
#   1. install each kcenon ecosystem dependency standalone into PREFIX,
#   2. configure, build and install pacs_system against PREFIX,
#   3. configure, build and run tests/cmake_consumer against PREFIX.
#
# Usage:
#   ./scripts/check-source-install.sh --deps DIR --prefix DIR --build DIR \
#       [--third-party DIR]...
#
# Environment: BUILD_TYPE (default Release); CMAKE_GENERATOR (Ninja is used
# when it is unset and ninja is on PATH).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

DEPENDENCIES=(common_system thread_system container_system logger_system
              monitoring_system network_system database_system)

DEPS_DIR=""
PREFIX=""
BUILD_DIR=""
THIRD_PARTY=()
BUILD_TYPE="${BUILD_TYPE:-Release}"

usage() {
    cat <<EOF
Usage: $0 --deps DIR --prefix DIR --build DIR [--third-party DIR]...

  --deps         Directory holding the dependency checkouts side by side
                 (${DEPENDENCIES[*]}),
                 as .github/actions/checkout-kcenon-deps lays them out
  --prefix       Install prefix for the dependencies and pacs_system
  --build        Directory for the build trees
  --third-party  Prefix that provides the asio, simdutf and unofficial-sqlite3
                 CMake packages, for example vcpkg_installed/<triplet> from
                 tests/source_install/vcpkg.json (repeatable)
EOF
    exit 2
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --deps) DEPS_DIR="$2"; shift 2 ;;
        --prefix) PREFIX="$2"; shift 2 ;;
        --build) BUILD_DIR="$2"; shift 2 ;;
        --third-party) THIRD_PARTY+=("$2"); shift 2 ;;
        -h|--help) usage ;;
        *) echo "Unknown option: $1" >&2; usage ;;
    esac
done

if [[ -z "${DEPS_DIR}" || -z "${PREFIX}" || -z "${BUILD_DIR}" ]]; then
    usage
fi

DEPS_DIR="$(cd "${DEPS_DIR}" && pwd)"
mkdir -p "${PREFIX}" "${BUILD_DIR}"
PREFIX="$(cd "${PREFIX}" && pwd)"
BUILD_DIR="$(cd "${BUILD_DIR}" && pwd)"

for dep in "${DEPENDENCIES[@]}"; do
    if [[ ! -f "${DEPS_DIR}/${dep}/CMakeLists.txt" ]]; then
        echo "error: ${DEPS_DIR}/${dep} is not a ${dep} checkout" >&2
        exit 1
    fi
    # cmake/dependencies.cmake uses a checkout named by <NAME>_ROOT (for example
    # COMMON_SYSTEM_ROOT), ../<name> or ./<name> before it looks for an
    # installed package.
    root_var="$(echo "${dep}" | tr '[:lower:]' '[:upper:]')_ROOT"
    if [[ -n "${!root_var:-}" ]]; then
        echo "error: unset ${root_var}; pacs_system would use it instead of the installed ${dep}" >&2
        exit 1
    fi
    for tree in "${PROJECT_ROOT}/../${dep}" "${PROJECT_ROOT}/${dep}"; do
        if [[ -d "${tree}" ]]; then
            echo "error: pacs_system would use ${tree} instead of the installed ${dep};" \
                 "keep dependency checkouts out of the pacs_system directory and its parent" >&2
            exit 1
        fi
    done
done

PREFIX_PATH="${PREFIX}"
for dir in "${THIRD_PARTY[@]+"${THIRD_PARTY[@]}"}"; do
    PREFIX_PATH="${PREFIX_PATH};$(cd "${dir}" && pwd)"
done

# Every package is found through CMAKE_PREFIX_PATH. A toolchain file from the
# environment (vcpkg's, for example) would run vcpkg manifest mode for each
# dependency's vcpkg.json and pull registry ports instead.
if [[ -n "${CMAKE_TOOLCHAIN_FILE:-}" ]]; then
    echo "note: ignoring CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} from the environment"
    unset CMAKE_TOOLCHAIN_FILE
fi

GENERATOR_ARGS=()
if [[ -z "${CMAKE_GENERATOR:-}" ]] && command -v ninja > /dev/null; then
    GENERATOR_ARGS=(-G Ninja)
fi

# configure_build_install <source dir> <build dir> [cmake options...]
configure_build_install() {
    local source_dir=$1 build_dir=$2
    shift 2
    echo "=== $(basename "${source_dir}") ==="
    cmake -S "${source_dir}" -B "${build_dir}" "${GENERATOR_ARGS[@]+"${GENERATOR_ARGS[@]}"}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
        -DCMAKE_PREFIX_PATH="${PREFIX_PATH}" \
        "$@"
    cmake --build "${build_dir}" --parallel
    cmake --install "${build_dir}"
}

install_dependency() {
    local dep=$1
    shift
    configure_build_install "${DEPS_DIR}/${dep}" "${BUILD_DIR}/${dep}" "$@"
}

# The pinned network_system installs public headers that include src/internal/
# headers it does not install (fixed upstream by kcenon/network_system#945,
# #949 and #1177). Copy them the way the kcenon-network-system registry port
# does; the step is skipped once the installed headers no longer need it.
complete_network_system_headers() {
    local source_dir="${DEPS_DIR}/network_system"
    local session_header="${PREFIX}/include/kcenon/network/detail/session/messaging_session.h"
    if [[ -f "${PREFIX}/include/internal/tcp/tcp_socket.h" ]] ||
       ! grep -q '"internal/tcp/tcp_socket.h"' "${session_header}"; then
        return
    fi
    echo "=== network_system: installing src/internal and network-core headers ==="
    (cd "${source_dir}/src" &&
        find internal -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.inl' \)) |
    while IFS= read -r header; do
        mkdir -p "${PREFIX}/include/$(dirname "${header}")"
        cp "${source_dir}/src/${header}" "${PREFIX}/include/${header}"
    done
    cp -R "${source_dir}/network-core/include/." "${PREFIX}/include/"
}

install_dependency common_system \
    -DCOMMON_BUILD_TESTS=OFF -DCOMMON_BUILD_INTEGRATION_TESTS=OFF -DCOMMON_BUILD_EXAMPLES=OFF
install_dependency thread_system \
    -DBUILD_TESTING=OFF -DBUILD_SAMPLES=OFF -DTHREAD_BUILD_INTEGRATION_TESTS=OFF \
    -DBUILD_DOCUMENTATION=OFF
install_dependency container_system \
    -DBUILD_TESTS=OFF -DBUILD_CONTAINER_SAMPLES=OFF -DBUILD_CONTAINER_EXAMPLES=OFF \
    -DCONTAINER_BUILD_INTEGRATION_TESTS=OFF -DBUILD_DOCUMENTATION=OFF \
    -DUNIFIED_ALLOW_FETCHCONTENT=OFF
install_dependency logger_system \
    -DBUILD_TESTS=OFF -DBUILD_SAMPLES=OFF -DLOGGER_BUILD_INTEGRATION_TESTS=OFF \
    -DUNIFIED_ALLOW_FETCHCONTENT_FALLBACK=OFF
install_dependency monitoring_system \
    -DMONITORING_BUILD_TESTS=OFF -DMONITORING_BUILD_INTEGRATION_TESTS=OFF \
    -DMONITORING_BUILD_EXAMPLES=OFF
install_dependency network_system \
    -DBUILD_TESTS=OFF -DBUILD_SAMPLES=OFF -DNETWORK_BUILD_INTEGRATION_TESTS=OFF
complete_network_system_headers
install_dependency database_system \
    -DUSE_SQLITE=ON -DUSE_POSTGRESQL=OFF -DUSE_UNIT_TEST=OFF -DBUILD_DATABASE_SAMPLES=OFF \
    -DDATABASE_BUILD_INTEGRATION_TESTS=OFF

# Warnings stay non-fatal, as in every other workflow of this repository.
configure_build_install "${PROJECT_ROOT}" "${BUILD_DIR}/pacs_system" \
    -DPACS_BUILD_TESTS=OFF -DPACS_BUILD_EXAMPLES=OFF -DPACS_WARNINGS_AS_ERRORS=OFF

echo "=== tests/cmake_consumer ==="
cmake -S "${PROJECT_ROOT}/tests/cmake_consumer" -B "${BUILD_DIR}/cmake_consumer" \
    "${GENERATOR_ARGS[@]+"${GENERATOR_ARGS[@]}"}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_PREFIX_PATH="${PREFIX_PATH}"
cmake --build "${BUILD_DIR}/cmake_consumer"
output="$("${BUILD_DIR}/cmake_consumer/pacs_consumer")"
echo "${output}"
if [[ "${output}" != "pacs_system v1.0 consumer OK"* ]]; then
    echo "error: unexpected consumer output" >&2
    exit 1
fi
