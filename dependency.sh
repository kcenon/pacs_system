#!/bin/bash

# PACS System Dependency Setup Script  
# Enhanced version based on thread_system submodule patterns

set -e  # Exit on error

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
echo -e "${BOLD}${BLUE}    PACS System Dependency Setup          ${NC}"
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

# Display how to use this script
show_usage() {
    echo -e "${BOLD}Usage:${NC} $0 [OPTIONS]"
    echo ""
    echo -e "${BOLD}Options:${NC}"
    echo "  --submodule         Update thread_system submodule to latest version"
    echo "  --clone-fresh       Clone thread_system repository directly instead of using submodule"
    echo "  --force-vcpkg       Force reinstall vcpkg even if it exists"
    echo "  --minimal           Install only essential dependencies"
    echo "  --development       Install additional development tools"
    echo "  --help              Show this help message"
    echo ""
    echo -e "${BOLD}Examples:${NC}"
    echo "  $0                  # Standard setup with submodule"
    echo "  $0 --clone-fresh    # Clone thread_system directly"
    echo "  $0 --development    # Install with dev tools"
}

# Process command line arguments
CLONE_FRESH=false
UPDATE_SUBMODULE=false
FORCE_VCPKG=false
MINIMAL_INSTALL=false
DEVELOPMENT_INSTALL=false

for arg in "$@"; do
    case $arg in
        --help)
            show_usage
            exit 0
            ;;
        --submodule)
            UPDATE_SUBMODULE=true
            ;;
        --clone-fresh)
            CLONE_FRESH=true
            ;;
        --force-vcpkg)
            FORCE_VCPKG=true
            ;;
        --minimal)
            MINIMAL_INSTALL=true
            ;;
        --development)
            DEVELOPMENT_INSTALL=true
            ;;
        *)
            print_error "Unknown option: $arg"
            show_usage
            exit 1
            ;;
    esac
done

# Save current directory to return to later
ORIGINAL_DIR=$(pwd)
print_status "Starting dependency setup from directory: $ORIGINAL_DIR"

# Function to install system dependencies
install_system_dependencies() {
    print_status "Installing system dependencies..."
    
    if [ "$(uname)" == "Darwin" ]; then
        print_status "Detected macOS - using Homebrew"
        
        # Check if Homebrew is installed
        if ! command_exists brew; then
            print_status "Installing Homebrew..."
            /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        fi
        
        # Update Homebrew
        print_status "Updating Homebrew..."
        brew update
        brew upgrade
        
        # Install essential packages
        ESSENTIAL_PACKAGES="pkg-config cmake doxygen ninja git curl"
        BUILD_PACKAGES="autoconf automake autoconf-archive python3"
        
        print_status "Installing essential packages..."
        for package in $ESSENTIAL_PACKAGES; do
            if ! brew list $package &>/dev/null; then
                print_status "Installing $package..."
                brew install $package
            else
                print_status "$package is already installed"
            fi
        done
        
        if [ "$MINIMAL_INSTALL" != true ]; then
            print_status "Installing build packages..."
            for package in $BUILD_PACKAGES; do
                if ! brew list $package &>/dev/null; then
                    print_status "Installing $package..."
                    brew install $package
                else
                    print_status "$package is already installed"
                fi
            done
        fi
        
        if [ "$DEVELOPMENT_INSTALL" == true ]; then
            DEV_PACKAGES="llvm gdb valgrind ccache"
            print_status "Installing development packages..."
            for package in $DEV_PACKAGES; do
                if ! brew list $package &>/dev/null; then
                    print_status "Installing $package..."
                    brew install $package
                else
                    print_status "$package is already installed"
                fi
            done
        fi
        
    elif [ "$(uname)" == "Linux" ]; then
        print_status "Detected Linux - using apt"
        
        # Update package lists
        print_status "Updating package lists..."
        sudo apt update
        sudo apt upgrade -y
        
        # Install essential packages
        ESSENTIAL_PACKAGES="cmake build-essential gdb doxygen pkg-config ninja-build git curl zip unzip tar python3"
        BUILD_PACKAGES="autoconf automake autoconf-archive libtool"
        
        print_status "Installing essential packages..."
        sudo apt install -y $ESSENTIAL_PACKAGES
        
        if [ "$MINIMAL_INSTALL" != true ]; then
            print_status "Installing build packages..."
            sudo apt install -y $BUILD_PACKAGES
        fi
        
        if [ "$DEVELOPMENT_INSTALL" == true ]; then
            DEV_PACKAGES="clang clang-tools valgrind ccache cppcheck"
            print_status "Installing development packages..."
            sudo apt install -y $DEV_PACKAGES
        fi
        
        # Handle ARM64 specific settings
        if [ $(uname -m) == "aarch64" ]; then
            export VCPKG_FORCE_SYSTEM_BINARIES=arm
            print_status "Detected ARM64 platform, setting VCPKG_FORCE_SYSTEM_BINARIES=arm"
        fi
    else
        print_warning "Unsupported operating system: $(uname)"
        print_warning "Please install dependencies manually:"
        print_warning "- cmake (>= 3.16)"
        print_warning "- C++ compiler (GCC >= 9 or Clang >= 10)"
        print_warning "- git"
        print_warning "- pkg-config"
        print_warning "- python3"
    fi
    
    print_success "System dependencies installed successfully"
}

# Function to manually clone the thread_system repo directly
clone_fresh_thread_system() {
    print_status "Cloning thread_system repository directly..."
    
    # Create a backup of existing thread_system if it exists
    if [ -d "./thread_system" ]; then
        print_warning "Backing up existing thread_system directory..."
        timestamp=$(date +%Y%m%d%H%M%S)
        mv "./thread_system" "./thread_system_backup_$timestamp"
        print_status "Backup created: thread_system_backup_$timestamp"
    fi
    
    # Clone the repository directly
    if ! git clone https://github.com/kcenon/thread_system.git; then
        print_error "Failed to clone thread_system repository."
        print_error "Please check your network connection and that the repository exists."
        return 1
    fi
    
    print_success "Successfully cloned thread_system repository"
    return 0
}

# Function to initialize and update thread_system as a submodule
init_and_update_submodule() {
    print_status "Setting up thread_system as a submodule..."
    
    # First, check if we're in a git repository
    if ! git rev-parse --is-inside-work-tree &>/dev/null; then
        print_error "Not inside a git repository. Cannot use submodule."
        print_warning "Try using '--clone-fresh' option instead."
        return 1
    fi
    
    # Clean up existing .git in thread_system if it exists
    if [ -d "./thread_system/.git" ]; then
        print_status "Removing existing git repository..."
        rm -rf thread_system/.git
    fi

    # Try to add as submodule if not already registered
    if ! git submodule status ./thread_system &>/dev/null; then
        print_status "Adding thread_system as a submodule..."
        if ! git submodule add --force https://github.com/kcenon/thread_system.git; then
            print_error "Failed to add thread_system submodule."
            print_error "This might be a temporary network issue or repository access problem."
            return 1
        fi
    fi

    # Initialize and update the submodule
    print_status "Initializing submodule..."
    if ! git submodule update --init --recursive; then
        print_warning "Failed to initialize submodule."
        print_status "Attempting to deinit and reinitialize..."
        
        git submodule deinit -f thread_system
        
        if ! git submodule update --init --recursive; then
            print_error "Failed to reinitialize submodule after deinit."
            print_warning "Consider using '--clone-fresh' option instead."
            return 1
        fi
    fi

    print_success "thread_system submodule initialized successfully"
    return 0
}

# Function to setup vcpkg
setup_vcpkg() {
    print_status "Setting up vcpkg package manager..."
    
    # Move to parent directory for vcpkg setup
    cd ..
    
    if [ "$FORCE_VCPKG" == true ] && [ -d "./vcpkg" ]; then
        print_warning "Force reinstall requested - removing existing vcpkg..."
        rm -rf "./vcpkg"
    fi
    
    if [ ! -d "./vcpkg/" ]; then
        print_status "Cloning vcpkg repository..."
        if ! git clone https://github.com/microsoft/vcpkg.git; then
            print_error "Failed to clone vcpkg repository"
            return 1
        fi
    else
        print_status "vcpkg directory already exists, updating..."
        cd vcpkg
        git pull
        cd ..
    fi
    
    cd vcpkg
    
    # Bootstrap vcpkg
    print_status "Bootstrapping vcpkg..."
    if [ "$(uname)" == "Darwin" ] || [ "$(uname)" == "Linux" ]; then
        if ! ./bootstrap-vcpkg.sh; then
            print_error "Failed to bootstrap vcpkg"
            return 1
        fi
    else
        print_error "Unsupported platform for vcpkg bootstrap"
        return 1
    fi
    
    # Return to original directory
    cd "$ORIGINAL_DIR"
    
    print_success "vcpkg setup completed successfully"
    return 0
}

# Function to install vcpkg dependencies
install_vcpkg_dependencies() {
    print_status "Installing PACS system dependencies via vcpkg..."
    
    # Essential dependencies for PACS system
    ESSENTIAL_DEPS="dcmtk fmt gtest cryptopp"
    ADDITIONAL_DEPS="lz4 asio python3 crossguid libpq nlohmann-json spdlog sqlite3 libxml2 openssl zlib"
    
    cd ../vcpkg
    
    if [ "$MINIMAL_INSTALL" == true ]; then
        print_status "Installing minimal dependencies: $ESSENTIAL_DEPS"
        if ! ./vcpkg install $ESSENTIAL_DEPS --recurse; then
            print_error "Failed to install essential vcpkg dependencies"
            return 1
        fi
    else
        print_status "Installing full dependencies: $ESSENTIAL_DEPS $ADDITIONAL_DEPS"
        if ! ./vcpkg install $ESSENTIAL_DEPS $ADDITIONAL_DEPS --recurse; then
            print_error "Failed to install vcpkg dependencies"
            return 1
        fi
    fi
    
    cd "$ORIGINAL_DIR"
    
    print_success "vcpkg dependencies installed successfully"
    return 0
}

# Main execution starts here
print_status "Platform: $(uname) $(uname -m)"
print_status "Configuration: CLONE_FRESH=$CLONE_FRESH, UPDATE_SUBMODULE=$UPDATE_SUBMODULE, MINIMAL=$MINIMAL_INSTALL, DEVELOPMENT=$DEVELOPMENT_INSTALL"

# Install system dependencies first
install_system_dependencies

# Handle thread_system setup
if [ "$CLONE_FRESH" = true ]; then
    # Skip submodule setup and just clone the repo directly
    if ! clone_fresh_thread_system; then
        print_error "Failed to manually clone thread_system. Exiting."
        exit 1
    fi
else
    # Standard submodule approach
    if [ ! -d "./thread_system" ]; then
        print_status "thread_system directory not found. Initializing submodule..."
        if ! init_and_update_submodule; then
            print_warning "Failed to initialize thread_system submodule."
            print_status "Would you like to try direct cloning instead? (y/n)"
            read -r response
            if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
                if ! clone_fresh_thread_system; then
                    print_error "Both submodule and direct clone approaches failed. Exiting."
                    exit 1
                fi
            else
                print_error "Exiting without initializing thread_system."
                exit 1
            fi
        fi
    elif [ -z "$(ls -A ./thread_system)" ] || [ ! -d "./thread_system/.git" ]; then
        print_status "thread_system directory is empty or not properly initialized. Updating submodule..."
        if ! init_and_update_submodule; then
            print_warning "Failed to update thread_system submodule."
            print_status "Would you like to try direct cloning instead? (y/n)"
            read -r response
            if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
                if ! clone_fresh_thread_system; then
                    print_error "Both submodule and direct clone approaches failed. Exiting."
                    exit 1
                fi
            else
                print_error "Exiting without initializing thread_system."
                exit 1
            fi
        fi
    fi

    # Update submodule to latest if requested
    if [ "$UPDATE_SUBMODULE" = true ]; then
        print_status "Updating submodules to latest remote version..."
        
        # Make sure thread_system directory exists and is a git repo before trying to use git commands on it
        if [ -d "./thread_system/.git" ]; then
            # Try to update submodule to latest version
            if ! git submodule update --remote --recursive --merge thread_system; then
                print_warning "Failed to update thread_system to latest version."
                print_warning "This might be due to conflicts or repository access issues."
                print_status "Would you like to try direct cloning instead? (y/n)"
                read -r response
                if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
                    if ! clone_fresh_thread_system; then
                        print_error "Both submodule update and direct clone approaches failed. Exiting."
                        exit 1
                    fi
                else
                    print_warning "Continuing with existing thread_system version."
                fi
            else
                print_success "Successfully updated submodule to latest version"
            fi
        else
            print_warning "thread_system is not a git repository. Cannot update submodule."
            print_status "Would you like to try direct cloning instead? (y/n)"
            read -r response
            if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
                if ! clone_fresh_thread_system; then
                    print_error "Direct clone approach failed. Exiting."
                    exit 1
                fi
            else
                print_warning "Continuing with existing thread_system."
            fi
        fi
    fi
fi

# Run thread_system's dependency script
if [ -f "./thread_system/dependency.sh" ]; then
    print_status "Running thread_system dependency script..."
    if ! /bin/bash ./thread_system/dependency.sh; then
        print_error "thread_system dependency.sh execution failed."
        exit 1
    fi
    print_success "thread_system dependencies installed successfully"
else
    print_error "dependency.sh not found in thread_system."
    print_error "This indicates that the thread_system repository wasn't properly initialized."
    exit 1
fi

# Setup vcpkg
if ! setup_vcpkg; then
    print_error "Failed to setup vcpkg"
    exit 1
fi

# Install vcpkg dependencies
if ! install_vcpkg_dependencies; then
    print_error "Failed to install vcpkg dependencies"
    exit 1
fi

# Final verification
print_status "Performing final verification..."

# Verify essential tools are available
REQUIRED_TOOLS="cmake git"
for tool in $REQUIRED_TOOLS; do
    if command_exists $tool; then
        print_success "$tool is available"
    else
        print_error "$tool is not available"
        exit 1
    fi
done

# Verify directories exist
REQUIRED_DIRS="thread_system ../vcpkg"
for dir in $REQUIRED_DIRS; do
    if [ -d "$dir" ]; then
        print_success "$dir directory exists"
    else
        print_error "$dir directory is missing"
        exit 1
    fi
done

# Return to the original directory
cd "$ORIGINAL_DIR"

# Final success message
echo -e "${BOLD}${GREEN}============================================${NC}"
echo -e "${BOLD}${GREEN}    PACS System Dependencies Ready         ${NC}"
echo -e "${BOLD}${GREEN}============================================${NC}"

print_success "Dependency setup completed successfully!"
print_status "You can now run './build.sh' to build the PACS system"

if [ "$DEVELOPMENT_INSTALL" == true ]; then
    print_status "Development tools installed. Additional commands available:"
    print_status "  - Use 'ccache' for faster rebuilds"
    print_status "  - Use 'valgrind' for memory debugging"
    print_status "  - Use 'cppcheck' for static analysis"
fi

exit 0