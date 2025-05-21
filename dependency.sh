#!/bin/bash

set -e  # Exit on error

# Display how to use this script
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "Options:"
    echo "  --submodule         Update thread_system submodule to latest version"
    echo "  --clone-fresh       Clone thread_system repository directly instead of using submodule"
    echo "  --help              Show this help message"
}

# Process command line arguments
CLONE_FRESH=false
UPDATE_SUBMODULE=false

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
        *)
            echo "Unknown option: $arg"
            show_usage
            exit 1
            ;;
    esac
done

# Save current directory to return to later
pushd . > /dev/null

# Function to manually clone the thread_system repo directly
clone_fresh_thread_system() {
    echo "Cloning thread_system repository directly..."
    
    # Create a backup of existing thread_system if it exists
    if [ -d "./thread_system" ]; then
        echo "Backing up existing thread_system directory..."
        timestamp=$(date +%Y%m%d%H%M%S)
        mv "./thread_system" "./thread_system_backup_$timestamp"
    fi
    
    # Clone the repository directly
    if ! git clone https://github.com/kcenon/thread_system.git; then
        echo "Failed to clone thread_system repository."
        echo "Please check your network connection and that the repository exists."
        return 1
    fi
    
    echo "Successfully cloned thread_system repository."
    return 0
}

# Function to initialize and update thread_system as a submodule
init_and_update_submodule() {
    # First, check if we're in a git repository
    if ! git rev-parse --is-inside-work-tree &>/dev/null; then
        echo "ERROR: Not inside a git repository. Cannot use submodule."
        echo "Try using '--clone-fresh' option instead."
        return 1
    fi
    
    # Clean up existing .git in thread_system if it exists
    if [ -d "./thread_system/.git" ]; then
        echo "Removing existing git repository..."
        rm -rf thread_system/.git
    fi

    echo "Setting up thread_system as a submodule..."
    
    # Try to add as submodule if not already registered
    if ! git submodule status ./thread_system &>/dev/null; then
        echo "Adding thread_system as a submodule..."
        if ! git submodule add --force https://github.com/kcenon/thread_system.git; then
            echo "Failed to add thread_system submodule."
            echo "This might be a temporary network issue or repository access problem."
            return 1
        fi
    fi

    # Initialize and update the submodule
    echo "Initializing submodule..."
    if ! git submodule update --init --recursive; then
        echo "Failed to initialize submodule."
        echo "Attempting to deinit and reinitialize..."
        
        git submodule deinit -f thread_system
        
        if ! git submodule update --init --recursive; then
            echo "Failed to reinitialize submodule after deinit."
            echo "Consider using '--clone-fresh' option instead."
            return 1
        fi
    fi

    return 0
}

# Handle thread_system setup
if [ "$CLONE_FRESH" = true ]; then
    # Skip submodule setup and just clone the repo directly
    if ! clone_fresh_thread_system; then
        echo "Failed to manually clone thread_system. Exiting."
        exit 1
    fi
else
    # Standard submodule approach
    if [ ! -d "./thread_system" ]; then
        echo "thread_system directory not found. Initializing submodule..."
        if ! init_and_update_submodule; then
            echo "Failed to initialize thread_system submodule."
            echo "Would you like to try direct cloning instead? (y/n)"
            read -r response
            if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
                if ! clone_fresh_thread_system; then
                    echo "Both submodule and direct clone approaches failed. Exiting."
                    exit 1
                fi
            else
                echo "Exiting without initializing thread_system."
                exit 1
            fi
        fi
    elif [ -z "$(ls -A ./thread_system)" ] || [ ! -d "./thread_system/.git" ]; then
        echo "thread_system directory is empty or not properly initialized. Updating submodule..."
        if ! init_and_update_submodule; then
            echo "Failed to update thread_system submodule."
            echo "Would you like to try direct cloning instead? (y/n)"
            read -r response
            if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
                if ! clone_fresh_thread_system; then
                    echo "Both submodule and direct clone approaches failed. Exiting."
                    exit 1
                fi
            else
                echo "Exiting without initializing thread_system."
                exit 1
            fi
        fi
    fi

    # Update submodule to latest if requested
    if [ "$UPDATE_SUBMODULE" = true ]; then
        echo "Updating submodules to latest remote version..."
        
        # Make sure thread_system directory exists and is a git repo before trying to use git commands on it
        if [ -d "./thread_system/.git" ]; then
            # Try to update submodule to latest version
            if ! git submodule update --remote --recursive --merge thread_system; then
                echo "Failed to update thread_system to latest version."
                echo "This might be due to conflicts or repository access issues."
                echo "Would you like to try direct cloning instead? (y/n)"
                read -r response
                if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
                    if ! clone_fresh_thread_system; then
                        echo "Both submodule update and direct clone approaches failed. Exiting."
                        exit 1
                    fi
                else
                    echo "Continuing with existing thread_system version."
                fi
            else
                echo "Successfully updated submodule to latest version."
            fi
        else
            echo "thread_system is not a git repository. Cannot update submodule."
            echo "Would you like to try direct cloning instead? (y/n)"
            read -r response
            if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
                if ! clone_fresh_thread_system; then
                    echo "Direct clone approach failed. Exiting."
                    exit 1
                fi
            else
                echo "Continuing with existing thread_system."
            fi
        fi
    fi
fi

# Run thread_system's dependency script
if [ -f "./thread_system/dependency.sh" ]; then
    echo "Running thread_system dependency script..."
    if ! /bin/bash ./thread_system/dependency.sh; then
        echo "Error: thread_system dependency.sh execution failed."
        exit 1
    fi
else
    echo "Error: dependency.sh not found in thread_system."
    echo "This indicates that the thread_system repository wasn't properly initialized."
    exit 1
fi

# Install vcpkg dependencies
cd ..
if [ -d "./vcpkg" ]; then
    echo "Installing dependencies via vcpkg..."
    cd vcpkg
    if ! ./vcpkg install lz4 fmt cryptopp asio python3 crossguid libpq gtest dcmtk --recurse; then
        echo "Error: vcpkg install failed."
        exit 1
    fi
    echo "Successfully installed all vcpkg dependencies."
else
    echo "Error: vcpkg directory not found."
    echo "Please ensure vcpkg is properly installed in the parent directory."
    exit 1
fi

# Return to the original directory
popd > /dev/null

echo "Dependency setup completed successfully."