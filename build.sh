#!/bin/bash

# Build script for pacs_system
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory (resolve symlinks)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Configuration defaults
PROJECT_NAME="pacs_system"
BUILD_DIR="build"
BUILD_TYPE="Release"
BUILD_TESTS="ON"
BUILD_EXAMPLES="OFF"
BUILD_BENCHMARKS="OFF"
BUILD_SAMPLES="OFF"
BUILD_STORAGE="ON"
BUILD_ITK_ADAPTER="OFF"
WARNINGS_AS_ERRORS="ON"
VERBOSE="OFF"
CLEAN_BUILD="OFF"
RUN_TESTS="OFF"
SANITIZER=""
JOBS=""

# Print usage
usage() {
    cat <<EOF
Usage: $0 [options]

Build Types:
  --debug              Build in Debug mode
  --release            Build in Release mode (default)
  --relwithdebinfo     Build in RelWithDebInfo mode

Features:
  --tests              Build and run tests (build only by default)
  --no-tests           Don't build tests
  --examples           Build examples
  --benchmarks         Build benchmarks
  --samples            Build developer samples
  --storage            Build storage module (default: ON)
  --no-storage         Disable storage module
  --itk                Build ITK integration adapter

Sanitizers:
  --asan               Build with AddressSanitizer
  --tsan               Build with ThreadSanitizer
  --ubsan              Build with UndefinedBehaviorSanitizer

Build Control:
  --clean              Clean build directory before building
  --verbose            Enable verbose build output
  --no-werror          Disable warnings-as-errors
  --jobs N             Set parallel build jobs (default: auto)
  --build-dir DIR      Set build directory (default: build)

Actions:
  --configure-only     Only run CMake configure, skip build
  --test-only          Only run tests (assumes already built)

  --help               Show this help message
EOF
    exit 0
}

# Action flags
CONFIGURE_ONLY="OFF"
TEST_ONLY="OFF"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            BUILD_DIR="build"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --relwithdebinfo)
            BUILD_TYPE="RelWithDebInfo"
            shift
            ;;
        --tests)
            BUILD_TESTS="ON"
            RUN_TESTS="ON"
            shift
            ;;
        --no-tests)
            BUILD_TESTS="OFF"
            RUN_TESTS="OFF"
            shift
            ;;
        --examples)
            BUILD_EXAMPLES="ON"
            shift
            ;;
        --benchmarks)
            BUILD_BENCHMARKS="ON"
            shift
            ;;
        --samples)
            BUILD_SAMPLES="ON"
            shift
            ;;
        --storage)
            BUILD_STORAGE="ON"
            shift
            ;;
        --no-storage)
            BUILD_STORAGE="OFF"
            shift
            ;;
        --itk)
            BUILD_ITK_ADAPTER="ON"
            shift
            ;;
        --asan)
            SANITIZER="address"
            BUILD_TYPE="Debug"
            shift
            ;;
        --tsan)
            SANITIZER="thread"
            BUILD_TYPE="Debug"
            shift
            ;;
        --ubsan)
            SANITIZER="undefined"
            BUILD_TYPE="Debug"
            shift
            ;;
        --clean)
            CLEAN_BUILD="ON"
            shift
            ;;
        --verbose)
            VERBOSE="ON"
            shift
            ;;
        --no-werror)
            WARNINGS_AS_ERRORS="OFF"
            shift
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --configure-only)
            CONFIGURE_ONLY="ON"
            shift
            ;;
        --test-only)
            TEST_ONLY="ON"
            shift
            ;;
        --help|-h)
            usage
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Auto-detect parallel jobs
if [ -z "${JOBS}" ]; then
    if command -v nproc &> /dev/null; then
        JOBS=$(nproc)
    elif command -v sysctl &> /dev/null; then
        JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    else
        JOBS=4
    fi
fi

# Test-only mode: skip configure and build
if [ "${TEST_ONLY}" = "ON" ]; then
    echo -e "${YELLOW}Running tests only...${NC}"
    if [ ! -d "${SCRIPT_DIR}/${BUILD_DIR}" ]; then
        echo -e "${RED}Error: Build directory '${BUILD_DIR}' not found. Build first.${NC}"
        exit 1
    fi
    cd "${SCRIPT_DIR}/${BUILD_DIR}"
    if ctest --output-on-failure --timeout 120 --parallel "${JOBS}"; then
        echo -e "${GREEN}All tests passed.${NC}"
    else
        echo -e "${RED}Some tests failed (see output above)${NC}"
        exit 1
    fi
    exit 0
fi

# Print configuration
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Building ${PROJECT_NAME}${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}Build Configuration:${NC}"
echo "  Build Type:        ${BUILD_TYPE}"
echo "  Build Directory:   ${BUILD_DIR}"
echo "  Parallel Jobs:     ${JOBS}"
echo "  Build Tests:       ${BUILD_TESTS}"
echo "  Build Examples:    ${BUILD_EXAMPLES}"
echo "  Build Storage:     ${BUILD_STORAGE}"
echo "  Warnings as Errors:${WARNINGS_AS_ERRORS}"
if [ -n "${SANITIZER}" ]; then
echo "  Sanitizer:         ${SANITIZER}"
fi
echo -e "${BLUE}========================================${NC}"

# Check for required tools
echo -e "${YELLOW}Checking prerequisites...${NC}"

if ! command -v cmake &> /dev/null; then
    echo -e "${RED}Error: cmake is not installed${NC}"
    echo "  Install: brew install cmake (macOS) / apt install cmake (Ubuntu)"
    exit 1
fi

# Detect generator: reuse existing cache if present, otherwise prefer Ninja
CACHE_FILE="${SCRIPT_DIR}/${BUILD_DIR}/CMakeCache.txt"
if [ -f "${CACHE_FILE}" ] && [ "${CLEAN_BUILD}" != "ON" ]; then
    CACHED_GENERATOR=$(grep 'CMAKE_GENERATOR:INTERNAL=' "${CACHE_FILE}" 2>/dev/null | cut -d= -f2)
    if [ -n "${CACHED_GENERATOR}" ]; then
        GENERATOR="${CACHED_GENERATOR}"
        echo -e "${GREEN}  Generator: ${GENERATOR} (from existing cache)${NC}"
    fi
fi

if [ -z "${GENERATOR:-}" ]; then
    if command -v ninja &> /dev/null; then
        GENERATOR="Ninja"
        echo -e "${GREEN}  Generator: Ninja${NC}"
    else
        GENERATOR="Unix Makefiles"
        echo -e "${YELLOW}  Generator: Make (install ninja for faster builds)${NC}"
    fi
fi

# Check for sibling dependencies
MISSING_DEPS=()
for dep in common_system thread_system logger_system container_system network_system; do
    if [ ! -d "${SCRIPT_DIR}/../${dep}" ]; then
        MISSING_DEPS+=("${dep}")
    fi
done

if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    echo -e "${YELLOW}Warning: Missing sibling dependencies:${NC}"
    for dep in "${MISSING_DEPS[@]}"; do
        echo -e "  ${RED}../$(basename "${dep}")${NC}"
    done
    echo ""
    echo "Clone them as siblings to this project:"
    echo "  cd $(dirname "${SCRIPT_DIR}")"
    for dep in "${MISSING_DEPS[@]}"; do
        echo "  git clone https://github.com/kcenon/${dep}.git"
    done
    echo ""
    echo -e "${YELLOW}CMake FetchContent will attempt to download them automatically.${NC}"
fi

# Clean build directory if requested
if [ "${CLEAN_BUILD}" = "ON" ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "${SCRIPT_DIR}/${BUILD_DIR}"
fi

# Create build directory
mkdir -p "${SCRIPT_DIR}/${BUILD_DIR}"

# Configure with CMake
echo -e "${YELLOW}Configuring with CMake...${NC}"

CMAKE_ARGS=(
    "-G" "${GENERATOR}"
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    "-DPACS_BUILD_TESTS=${BUILD_TESTS}"
    "-DPACS_BUILD_EXAMPLES=${BUILD_EXAMPLES}"
    "-DPACS_BUILD_BENCHMARKS=${BUILD_BENCHMARKS}"
    "-DPACS_BUILD_SAMPLES=${BUILD_SAMPLES}"
    "-DPACS_BUILD_STORAGE=${BUILD_STORAGE}"
    "-DPACS_BUILD_ITK_ADAPTER=${BUILD_ITK_ADAPTER}"
    "-DPACS_WARNINGS_AS_ERRORS=${WARNINGS_AS_ERRORS}"
)

# Sanitizer flags
if [ -n "${SANITIZER}" ]; then
    SANITIZER_FLAGS="-fsanitize=${SANITIZER} -fno-omit-frame-pointer -g"
    CMAKE_ARGS+=(
        "-DCMAKE_C_FLAGS=${SANITIZER_FLAGS}"
        "-DCMAKE_CXX_FLAGS=${SANITIZER_FLAGS}"
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=${SANITIZER}"
        "-DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=${SANITIZER}"
        "-DPACS_WARNINGS_AS_ERRORS=OFF"
    )
fi

if [ "${VERBOSE}" = "ON" ]; then
    CMAKE_ARGS+=("-DCMAKE_VERBOSE_MAKEFILE=ON")
fi

cmake -B "${SCRIPT_DIR}/${BUILD_DIR}" -S "${SCRIPT_DIR}" "${CMAKE_ARGS[@]}"

if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed${NC}"
    exit 1
fi

echo -e "${GREEN}Configuration successful.${NC}"

# Stop here if configure-only
if [ "${CONFIGURE_ONLY}" = "ON" ]; then
    echo -e "${GREEN}Configure-only mode: skipping build.${NC}"
    exit 0
fi

# Build
echo -e "${YELLOW}Building (${JOBS} parallel jobs)...${NC}"

cmake --build "${SCRIPT_DIR}/${BUILD_DIR}" --parallel "${JOBS}"

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed${NC}"
    exit 1
fi

echo -e "${GREEN}Build successful.${NC}"

# Link compile_commands.json for IDE support
if [ -f "${SCRIPT_DIR}/${BUILD_DIR}/compile_commands.json" ]; then
    ln -sf "${BUILD_DIR}/compile_commands.json" "${SCRIPT_DIR}/compile_commands.json" 2>/dev/null || true
fi

# Run tests if requested
if [ "${RUN_TESTS}" = "ON" ] && [ "${BUILD_TESTS}" = "ON" ]; then
    echo -e "${YELLOW}Running tests...${NC}"

    CTEST_ARGS=(
        "--test-dir" "${SCRIPT_DIR}/${BUILD_DIR}"
        "--output-on-failure"
        "--timeout" "120"
        "--parallel" "${JOBS}"
    )

    # Sanitizer runtime options
    if [ "${SANITIZER}" = "address" ]; then
        export ASAN_OPTIONS="halt_on_error=0 detect_leaks=1 detect_stack_use_after_return=1"
    elif [ "${SANITIZER}" = "thread" ]; then
        export TSAN_OPTIONS="halt_on_error=0 second_deadlock_stack=1 history_size=4"
    elif [ "${SANITIZER}" = "undefined" ]; then
        export UBSAN_OPTIONS="halt_on_error=0 print_stacktrace=1"
    fi

    if ctest "${CTEST_ARGS[@]}"; then
        echo -e "${GREEN}All tests passed.${NC}"
    else
        echo -e "${RED}Some tests failed (see output above)${NC}"
        exit 1
    fi
fi

# Summary
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Binaries:  ${BUILD_DIR}/bin/"
echo "Libraries: ${BUILD_DIR}/lib/"
if [ "${BUILD_TESTS}" = "ON" ] && [ "${RUN_TESTS}" != "ON" ]; then
    echo ""
    echo "Run tests with:"
    echo "  $0 --test-only"
    echo "  ctest --test-dir ${BUILD_DIR} --output-on-failure"
fi
