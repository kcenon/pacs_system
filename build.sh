#!/bin/bash

# PACS System Build Script
# Enhanced version based on thread_system submodule patterns

# Color definitions for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Display banner
echo -e "${BOLD}${BLUE}============================================${NC}"
echo -e "${BOLD}${BLUE}        PACS System Build Script           ${NC}"
echo -e "${BOLD}${BLUE}============================================${NC}"

# Function to print status messages
print_status() {
    echo -e "${BOLD}${BLUE}[STATUS]${NC} $1"
}

# Function to print success messages
print_success() {
    echo -e "${BOLD}${GREEN}[SUCCESS]${NC} $1"
}

# Function to print error messages
print_error() {
    echo -e "${BOLD}${RED}[ERROR]${NC} $1"
}

# Function to print warning messages
print_warning() {
    echo -e "${BOLD}${YELLOW}[WARNING]${NC} $1"
}

# Function to check if a command exists
command_exists() {
    command -v "$1" &> /dev/null
}

# Display help information
show_help() {
    echo -e "${BOLD}Usage:${NC} $0 [options]"
    echo ""
    echo -e "${BOLD}Build Options:${NC}"
    echo "  --clean           Perform a clean rebuild by removing the build directory"
    echo "  --debug           Build in debug mode (default is release)"
    echo "  --tests           Build and run tests"
    echo "  --benchmark       Build with performance benchmarks enabled"
    echo ""
    echo -e "${BOLD}Target Options:${NC}"
    echo "  --all             Build all targets (default)"
    echo "  --modules-only    Build only PACS modules (storage, query_retrieve, etc.)"
    echo "  --samples         Build only sample applications"
    echo "  --core-only       Build only core libraries"
    echo ""
    echo -e "${BOLD}Documentation Options:${NC}"
    echo "  --docs            Generate Doxygen documentation"
    echo "  --clean-docs      Clean and regenerate Doxygen documentation"
    echo ""
    echo -e "${BOLD}General Options:${NC}"
    echo "  --cores N         Use N cores for compilation (default: auto-detect)"
    echo "  --verbose         Show detailed build output"
    echo "  --help            Display this help and exit"
    echo ""
    echo -e "${BOLD}Compiler Options:${NC}"
    echo "  --compiler NAME   Use specific compiler (e.g., g++, clang++, g++-12)"
    echo "  --list-compilers  List all available compilers and exit"
    echo ""
}

# Function to detect available compilers
detect_compilers() {
    print_status "Detecting available compilers..."
    
    local available_compilers=()
    local compiler_details=()
    
    # Check for various C++ compilers
    if command_exists g++; then
        local gcc_version=$(g++ --version 2>/dev/null | head -n1)
        available_compilers+=("g++")
        compiler_details+=("GCC: $gcc_version")
    fi
    
    if command_exists clang++; then
        local clang_version=$(clang++ --version 2>/dev/null | head -n1)
        available_compilers+=("clang++")
        compiler_details+=("Clang: $clang_version")
    fi
    
    # Check for additional GCC versions
    for version in 13 12 11 10 9; do
        if command_exists "g++-$version"; then
            local gcc_ver=$(g++-$version --version 2>/dev/null | head -n1)
            available_compilers+=("g++-$version")
            compiler_details+=("GCC-$version: $gcc_ver")
        fi
    done
    
    # Check for additional Clang versions  
    for version in 17 16 15 14 13 12 11 10; do
        if command_exists "clang++-$version"; then
            local clang_ver=$(clang++-$version --version 2>/dev/null | head -n1)
            available_compilers+=("clang++-$version")
            compiler_details+=("Clang-$version: $clang_ver")
        fi
    done
    
    # Check for platform-specific compilers
    if [ "$(uname)" == "Darwin" ]; then
        # macOS specific compilers
        if command_exists /usr/bin/clang++; then
            local apple_clang_version=$(/usr/bin/clang++ --version 2>/dev/null | head -n1)
            available_compilers+=("/usr/bin/clang++")
            compiler_details+=("Apple Clang: $apple_clang_version")
        fi
    fi
    
    if [ ${#available_compilers[@]} -eq 0 ]; then
        print_error "No C++ compilers found!"
        print_warning "Please install a C++ compiler (GCC or Clang)"
        return 1
    fi
    
    # Export arrays for use in other functions
    AVAILABLE_COMPILERS=("${available_compilers[@]}")
    COMPILER_DETAILS=("${compiler_details[@]}")
    
    return 0
}

# Function to display available compilers
show_compilers() {
    echo -e "${BOLD}${CYAN}Available Compilers:${NC}"
    for i in "${!AVAILABLE_COMPILERS[@]}"; do
        printf "  %d) %s\n" $((i + 1)) "${COMPILER_DETAILS[i]}"
    done
    echo ""
}

# Function to check dependencies
check_dependencies() {
    print_status "Checking build dependencies..."
    
    local missing_deps=()
    
    # Check for essential build tools
    for cmd in cmake git; do
        if ! command_exists "$cmd"; then
            missing_deps+=("$cmd")
        fi
    done
    
    # Check for at least one build system (make or ninja)
    if ! command_exists "make" && ! command_exists "ninja"; then
        missing_deps+=("make or ninja")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing required dependencies: ${missing_deps[*]}"
        print_warning "Please install missing dependencies before building."
        print_warning "You can use './dependency.sh' to install all required dependencies."
        return 1
    fi
    
    # Check for thread_system submodule
    if [ ! -d "thread_system" ] || [ -z "$(ls -A thread_system)" ]; then
        print_warning "thread_system submodule not found or empty."
        print_warning "Running dependency script to set up thread_system..."
        
        if [ -f "./dependency.sh" ]; then
            bash ./dependency.sh
            if [ $? -ne 0 ]; then
                print_error "Failed to run dependency.sh"
                return 1
            fi
        else
            print_error "dependency.sh script not found"
            return 1
        fi
    fi
    
    # Check for vcpkg
    if [ ! -d "../vcpkg" ]; then
        print_warning "vcpkg not found in parent directory."
        print_warning "Running dependency script to set up vcpkg..."
        
        if [ -f "./dependency.sh" ]; then
            bash ./dependency.sh
            if [ $? -ne 0 ]; then
                print_error "Failed to run dependency.sh"
                return 1
            fi
        else
            print_error "dependency.sh script not found"
            return 1
        fi
    fi
    
    # Check for doxygen if building docs
    if [ $BUILD_DOCS -eq 1 ] && ! command_exists doxygen; then
        print_warning "Doxygen not found but documentation was requested."
        print_warning "Documentation will not be generated."
        BUILD_DOCS=0
    fi
    
    print_success "All dependencies are satisfied"
    return 0
}

# Store original directory
ORIGINAL_DIR=$(pwd)

# Parse command line arguments
CLEAN_BUILD=0
RUN_TESTS=0
BUILD_DOCS=0
CLEAN_DOCS=0
BUILD_TYPE="Release"
BUILD_BENCHMARKS=0
TARGET="all"
BUILD_CORES=0
VERBOSE=0
SELECTED_COMPILER=""
LIST_COMPILERS_ONLY=0

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN_BUILD=1
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --tests)
            RUN_TESTS=1
            shift
            ;;
        --benchmark)
            BUILD_BENCHMARKS=1
            shift
            ;;
        --all)
            TARGET="all"
            shift
            ;;
        --modules-only)
            TARGET="modules-only"
            shift
            ;;
        --samples)
            TARGET="samples"
            shift
            ;;
        --core-only)
            TARGET="core-only"
            shift
            ;;
        --docs)
            BUILD_DOCS=1
            shift
            ;;
        --clean-docs)
            BUILD_DOCS=1
            CLEAN_DOCS=1
            shift
            ;;
        --cores)
            if [[ $2 =~ ^[0-9]+$ ]]; then
                BUILD_CORES=$2
                shift 2
            else
                print_error "Option --cores requires a numeric argument"
                exit 1
            fi
            ;;
        --verbose)
            VERBOSE=1
            shift
            ;;
        --compiler)
            if [ -n "$2" ]; then
                SELECTED_COMPILER="$2"
                shift 2
            else
                print_error "Option --compiler requires a compiler name"
                exit 1
            fi
            ;;
        --list-compilers)
            LIST_COMPILERS_ONLY=1
            shift
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Set number of cores to use for building
if [ $BUILD_CORES -eq 0 ]; then
    if command_exists nproc; then
        TOTAL_CORES=$(nproc)
    elif [ "$(uname)" == "Darwin" ]; then
        TOTAL_CORES=$(sysctl -n hw.ncpu)
    else
        # Default to 4 if we can't detect
        TOTAL_CORES=4
    fi
    
    # Use single core to avoid pipe issues on macOS
    BUILD_CORES=1
fi

print_status "Using $BUILD_CORES cores for compilation"

# Check for platform-specific settings
if [ "$(uname)" == "Linux" ]; then
    if [ $(uname -m) == "aarch64" ]; then
        export VCPKG_FORCE_SYSTEM_BINARIES=arm
        print_status "Detected ARM64 platform, setting VCPKG_FORCE_SYSTEM_BINARIES=arm"
    fi
fi

# Detect available compilers
detect_compilers
if [ $? -ne 0 ]; then
    exit 1
fi

# Handle compiler-related options
if [ $LIST_COMPILERS_ONLY -eq 1 ]; then
    show_compilers
    exit 0
fi

if [ -n "$SELECTED_COMPILER" ]; then
    # Check if selected compiler is available
    compiler_found=0
    for compiler in "${AVAILABLE_COMPILERS[@]}"; do
        if [ "$compiler" = "$SELECTED_COMPILER" ]; then
            compiler_found=1
            break
        fi
    done
    
    if [ $compiler_found -eq 0 ]; then
        print_error "Compiler '$SELECTED_COMPILER' not found in available compilers"
        show_compilers
        exit 1
    fi
    
    print_status "Using selected compiler: $SELECTED_COMPILER"
else
    # Use the first available compiler as default
    SELECTED_COMPILER="${AVAILABLE_COMPILERS[0]}"
    print_status "Using default compiler: ${COMPILER_DETAILS[0]}"
fi

# Check dependencies before proceeding
check_dependencies
if [ $? -ne 0 ]; then
    print_error "Failed dependency check. Exiting."
    exit 1
fi

# Clean build if requested
if [ $CLEAN_BUILD -eq 1 ]; then
    print_status "Performing clean build..."
    rm -rf build
fi

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    print_status "Creating build directory..."
    mkdir -p build
fi

# Prepare CMake arguments
CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake"
CMAKE_ARGS+=" -DCMAKE_BUILD_TYPE=$BUILD_TYPE"
CMAKE_ARGS+=" -DCMAKE_CXX_COMPILER=$SELECTED_COMPILER"

# Add feature flags based on options
if [ $BUILD_BENCHMARKS -eq 1 ]; then
    CMAKE_ARGS+=" -DBUILD_BENCHMARKS=ON"
fi

# Set target-specific options
if [ "$TARGET" == "modules-only" ]; then
    CMAKE_ARGS+=" -DBUILD_MODULES_ONLY=ON"
elif [ "$TARGET" == "core-only" ]; then
    CMAKE_ARGS+=" -DBUILD_CORE_ONLY=ON"
elif [ "$TARGET" == "samples" ]; then
    CMAKE_ARGS+=" -DBUILD_SAMPLES_ONLY=ON"
fi

# Enter build directory
cd build || { print_error "Failed to enter build directory"; exit 1; }

# Run CMake configuration
print_status "Configuring project with CMake..."
if [ "$(uname)" == "Darwin" ]; then
    # On macOS, ensure proper paths
    cmake .. $CMAKE_ARGS -DCMAKE_PREFIX_PATH="$(pwd)"
else
    cmake .. $CMAKE_ARGS
fi

# Check if CMake configuration was successful
if [ $? -ne 0 ]; then
    print_error "CMake configuration failed. See the output above for details."
    cd "$ORIGINAL_DIR"
    exit 1
fi

# Build the project
print_status "Building project in $BUILD_TYPE mode..."

# Determine build target based on option
BUILD_TARGET=""
if [ "$TARGET" == "all" ]; then
    BUILD_TARGET=""
elif [ "$TARGET" == "modules-only" ]; then
    BUILD_TARGET="pacs_storage_scp pacs_storage_scu pacs_query_retrieve_scp pacs_query_retrieve_scu pacs_worklist_scp pacs_worklist_scu pacs_mpps_scp pacs_mpps_scu"
elif [ "$TARGET" == "core-only" ]; then
    BUILD_TARGET="pacs_common pacs_dicom pacs_security thread_base thread_pool logger utilities"
elif [ "$TARGET" == "samples" ]; then
    BUILD_TARGET="storage_scp_sample storage_scu_sample query_retrieve_scp_sample query_retrieve_scu_sample"
fi

# Detect build system (Ninja or Make)
if [ -f "build.ninja" ]; then
    BUILD_COMMAND="ninja"
    if [ $VERBOSE -eq 1 ]; then
        BUILD_ARGS="-v"
    else
        BUILD_ARGS=""
    fi
elif [ -f "Makefile" ]; then
    BUILD_COMMAND="make"
    if [ $VERBOSE -eq 1 ]; then
        BUILD_ARGS="VERBOSE=1"
    else
        BUILD_ARGS=""
    fi
else
    print_error "No build system files found (neither build.ninja nor Makefile)"
    cd "$ORIGINAL_DIR"
    exit 1
fi

print_status "Using build system: $BUILD_COMMAND"

# Run build with appropriate target and cores
if [ "$BUILD_COMMAND" == "ninja" ]; then
    if [ -n "$BUILD_TARGET" ]; then
        $BUILD_COMMAND -j$BUILD_CORES $BUILD_ARGS $BUILD_TARGET
    else
        $BUILD_COMMAND -j$BUILD_CORES $BUILD_ARGS
    fi
elif [ "$BUILD_COMMAND" == "make" ]; then
    if [ -n "$BUILD_TARGET" ]; then
        $BUILD_COMMAND -j$BUILD_CORES $BUILD_ARGS $BUILD_TARGET
    else
        $BUILD_COMMAND -j$BUILD_CORES $BUILD_ARGS
    fi
fi

# Check if build was successful
if [ $? -ne 0 ]; then
    print_error "Build failed. See the output above for details."
    cd "$ORIGINAL_DIR"
    exit 1
fi

# Run tests if requested
if [ $RUN_TESTS -eq 1 ]; then
    print_status "Running tests..."
    
    export LC_ALL=C
    unset LANGUAGE
    
    # Run CTest
    ctest --output-on-failure --build-config $BUILD_TYPE
    TEST_RESULT=$?
    
    # Also run integration tests if available
    if [ -f "./bin/test_dcmtk_integration" ]; then
        print_status "Running DCMTK integration tests..."
        ./bin/test_dcmtk_integration
        INTEGRATION_TEST_RESULT=$?
        
        if [ $INTEGRATION_TEST_RESULT -ne 0 ]; then
            print_warning "Integration tests failed with exit code $INTEGRATION_TEST_RESULT"
        else
            print_success "Integration tests passed!"
        fi
    fi
    
    if [ $TEST_RESULT -ne 0 ]; then
        print_error "Tests failed with exit code $TEST_RESULT"
        cd "$ORIGINAL_DIR"
        exit $TEST_RESULT
    else
        print_success "All tests passed!"
    fi
fi

# Return to original directory
cd "$ORIGINAL_DIR"

# Generate documentation if requested
if [ $BUILD_DOCS -eq 1 ]; then
    print_status "Generating Doxygen documentation..."
    
    # Create docs directory if it doesn't exist
    if [ ! -d "documents" ]; then
        mkdir -p documents
    fi
    
    # Clean docs if requested
    if [ $CLEAN_DOCS -eq 1 ]; then
        print_status "Cleaning documentation directory..."
        rm -rf documents/html/*
    fi
    
    # Check if doxygen is installed
    if ! command_exists doxygen; then
        print_error "Doxygen is not installed. Please install it to generate documentation."
        exit 1
    fi
    
    # Run doxygen
    doxygen Doxyfile
    
    # Check if documentation generation was successful
    if [ $? -ne 0 ]; then
        print_error "Documentation generation failed. See the output above for details."
    else
        print_success "Documentation generated successfully in the documents directory!"
    fi
fi

# Show success message
print_success "Build completed successfully!"

# Final success message
echo -e "${BOLD}${GREEN}============================================${NC}"
echo -e "${BOLD}${GREEN}        PACS System Build Complete         ${NC}"
echo -e "${BOLD}${GREEN}============================================${NC}"

if [ -d "build/bin" ]; then
    echo -e "${CYAN}Available executables:${NC}"
    ls -la build/bin/
fi

echo -e "${CYAN}Build type:${NC} $BUILD_TYPE"
echo -e "${CYAN}Target:${NC} $TARGET"
echo -e "${CYAN}Compiler:${NC} $SELECTED_COMPILER"

if [ $BUILD_BENCHMARKS -eq 1 ]; then
    echo -e "${CYAN}Benchmarks:${NC} Enabled"
fi

if [ $BUILD_DOCS -eq 1 ]; then
    echo -e "${CYAN}Documentation:${NC} Generated"
fi

exit 0