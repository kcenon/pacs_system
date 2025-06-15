@echo off
setlocal EnableDelayedExpansion

:: PACS System Dependency Setup Script for Windows
:: Enhanced version based on thread_system submodule patterns

:: Display banner
echo.
echo ============================================
echo    PACS System Dependency Setup
echo ============================================
echo.

:: Function to check if a command exists
:command_exists
where %1 >nul 2>nul
goto :eof

:: Function to print status messages
:print_status
echo [STATUS] %~1
goto :eof

:: Function to print success messages
:print_success
echo [SUCCESS] %~1
goto :eof

:: Function to print error messages
:print_error
echo [ERROR] %~1
goto :eof

:: Function to print warning messages
:print_warning
echo [WARNING] %~1
goto :eof

:: Process command line arguments
set CLONE_FRESH=false
set UPDATE_SUBMODULE=false
set FORCE_VCPKG=false
set MINIMAL_INSTALL=false
set DEVELOPMENT_INSTALL=false

:parse_args
if "%~1"=="" goto :end_parse
if "%~1"=="--help" (
    goto :show_usage
)
if "%~1"=="--submodule" (
    set UPDATE_SUBMODULE=true
    shift
    goto :parse_args
)
if "%~1"=="--clone-fresh" (
    set CLONE_FRESH=true
    shift
    goto :parse_args
)
if "%~1"=="--force-vcpkg" (
    set FORCE_VCPKG=true
    shift
    goto :parse_args
)
if "%~1"=="--minimal" (
    set MINIMAL_INSTALL=true
    shift
    goto :parse_args
)
if "%~1"=="--development" (
    set DEVELOPMENT_INSTALL=true
    shift
    goto :parse_args
)
call :print_error "Unknown option: %~1"
goto :show_usage

:end_parse

:: Save current directory
set ORIGINAL_DIR=%CD%
call :print_status "Starting dependency setup from directory: %ORIGINAL_DIR%"

:: Display configuration
call :print_status "Platform: Windows %PROCESSOR_ARCHITECTURE%"
call :print_status "Configuration: CLONE_FRESH=%CLONE_FRESH%, UPDATE_SUBMODULE=%UPDATE_SUBMODULE%, MINIMAL=%MINIMAL_INSTALL%, DEVELOPMENT=%DEVELOPMENT_INSTALL%"

:: Install system dependencies
call :install_system_dependencies

:: Handle thread_system setup
if "%CLONE_FRESH%"=="true" (
    :: Skip submodule setup and just clone the repo directly
    call :clone_fresh_thread_system
    if errorlevel 1 (
        call :print_error "Failed to manually clone thread_system. Exiting."
        exit /b 1
    )
) else (
    :: Standard submodule approach
    if not exist "thread_system\" (
        call :print_status "thread_system directory not found. Initializing submodule..."
        call :init_and_update_submodule
        if errorlevel 1 (
            call :print_warning "Failed to initialize thread_system submodule."
            set /p "response=Would you like to try direct cloning instead? (y/n): "
            if /i "!response!"=="y" (
                call :clone_fresh_thread_system
                if errorlevel 1 (
                    call :print_error "Both submodule and direct clone approaches failed. Exiting."
                    exit /b 1
                )
            ) else (
                call :print_error "Exiting without initializing thread_system."
                exit /b 1
            )
        )
    ) else (
        :: Check if thread_system directory is empty
        dir /b thread_system 2>nul | findstr /r /v "^$" >nul
        if errorlevel 1 (
            call :print_status "thread_system directory is empty or not properly initialized. Updating submodule..."
            call :init_and_update_submodule
            if errorlevel 1 (
                call :print_warning "Failed to update thread_system submodule."
                set /p "response=Would you like to try direct cloning instead? (y/n): "
                if /i "!response!"=="y" (
                    call :clone_fresh_thread_system
                    if errorlevel 1 (
                        call :print_error "Both submodule and direct clone approaches failed. Exiting."
                        exit /b 1
                    )
                ) else (
                    call :print_error "Exiting without initializing thread_system."
                    exit /b 1
                )
            )
        )
    )

    :: Update submodule to latest if requested
    if "%UPDATE_SUBMODULE%"=="true" (
        call :print_status "Updating submodules to latest remote version..."
        
        :: Make sure thread_system directory exists and is a git repo
        if exist "thread_system\.git\" (
            :: Try to update submodule to latest version
            git submodule update --remote --recursive --merge thread_system
            if errorlevel 1 (
                call :print_warning "Failed to update thread_system to latest version."
                call :print_warning "This might be due to conflicts or repository access issues."
                set /p "response=Would you like to try direct cloning instead? (y/n): "
                if /i "!response!"=="y" (
                    call :clone_fresh_thread_system
                    if errorlevel 1 (
                        call :print_error "Both submodule update and direct clone approaches failed. Exiting."
                        exit /b 1
                    )
                ) else (
                    call :print_warning "Continuing with existing thread_system version."
                )
            ) else (
                call :print_success "Successfully updated submodule to latest version"
            )
        ) else (
            call :print_warning "thread_system is not a git repository. Cannot update submodule."
            set /p "response=Would you like to try direct cloning instead? (y/n): "
            if /i "!response!"=="y" (
                call :clone_fresh_thread_system
                if errorlevel 1 (
                    call :print_error "Direct clone approach failed. Exiting."
                    exit /b 1
                )
            ) else (
                call :print_warning "Continuing with existing thread_system."
            )
        )
    )
)

:: Run thread_system's dependency script
if exist "thread_system\dependency.bat" (
    call :print_status "Running thread_system dependency script..."
    pushd thread_system
    call dependency.bat
    set THREAD_SYSTEM_RESULT=!errorlevel!
    popd
    if !THREAD_SYSTEM_RESULT! neq 0 (
        call :print_error "thread_system dependency.bat execution failed."
        exit /b 1
    )
    call :print_success "thread_system dependencies installed successfully"
) else (
    call :print_error "dependency.bat not found in thread_system."
    call :print_error "This indicates that the thread_system repository wasn't properly initialized."
    exit /b 1
)

:: Setup vcpkg
call :setup_vcpkg
if errorlevel 1 (
    call :print_error "Failed to setup vcpkg"
    exit /b 1
)

:: Install vcpkg dependencies
call :install_vcpkg_dependencies
if errorlevel 1 (
    call :print_error "Failed to install vcpkg dependencies"
    exit /b 1
)

:: Final verification
call :print_status "Performing final verification..."

:: Verify essential tools are available
set REQUIRED_TOOLS=cmake git
for %%t in (%REQUIRED_TOOLS%) do (
    call :command_exists %%t
    if not errorlevel 1 (
        call :print_success "%%t is available"
    ) else (
        call :print_error "%%t is not available"
        exit /b 1
    )
)

:: Verify directories exist
set REQUIRED_DIRS=thread_system ..\vcpkg
for %%d in (%REQUIRED_DIRS%) do (
    if exist "%%d\" (
        call :print_success "%%d directory exists"
    ) else (
        call :print_error "%%d directory is missing"
        exit /b 1
    )
)

:: Return to the original directory
cd /d "%ORIGINAL_DIR%"

:: Final success message
echo.
echo ============================================
echo    PACS System Dependencies Ready
echo ============================================
echo.

call :print_success "Dependency setup completed successfully!"
call :print_status "You can now run 'build.bat' to build the PACS system"

if "%DEVELOPMENT_INSTALL%"=="true" (
    call :print_status "Development tools installed. Additional commands available:"
    call :print_status "  - Use Visual Studio for development and debugging"
    call :print_status "  - Use vcpkg for package management"
    call :print_status "  - Use CMake for build configuration"
)

exit /b 0

:show_usage
echo Usage: %~nx0 [OPTIONS]
echo.
echo Options:
echo   --submodule         Update thread_system submodule to latest version
echo   --clone-fresh       Clone thread_system repository directly instead of using submodule
echo   --force-vcpkg       Force reinstall vcpkg even if it exists
echo   --minimal           Install only essential dependencies
echo   --development       Install additional development tools
echo   --help              Show this help message
echo.
echo Examples:
echo   %~nx0                  # Standard setup with submodule
echo   %~nx0 --clone-fresh    # Clone thread_system directly
echo   %~nx0 --development    # Install with dev tools
echo.
exit /b 0

:: Function to install system dependencies
:install_system_dependencies
call :print_status "Installing system dependencies..."

call :print_status "Detected Windows - checking for package managers..."

:: Check for Chocolatey
call :command_exists choco
if not errorlevel 1 (
    call :print_status "Found Chocolatey package manager"
    set HAS_CHOCO=true
) else (
    set HAS_CHOCO=false
)

:: Check for winget
call :command_exists winget
if not errorlevel 1 (
    call :print_status "Found winget package manager"
    set HAS_WINGET=true
) else (
    set HAS_WINGET=false
)

:: Essential packages
set ESSENTIAL_PACKAGES=git cmake
set BUILD_PACKAGES=python3 7zip
set DEV_PACKAGES=

:: Install essential packages
call :print_status "Installing essential packages..."
for %%p in (%ESSENTIAL_PACKAGES%) do (
    call :command_exists %%p
    if errorlevel 1 (
        call :print_status "Installing %%p..."
        call :install_package %%p
    ) else (
        call :print_status "%%p is already installed"
    )
)

:: Install build packages if not minimal
if not "%MINIMAL_INSTALL%"=="true" (
    call :print_status "Installing build packages..."
    for %%p in (%BUILD_PACKAGES%) do (
        call :command_exists %%p
        if errorlevel 1 (
            call :print_status "Installing %%p..."
            call :install_package %%p
        ) else (
            call :print_status "%%p is already installed"
        )
    )
)

:: Install development packages if requested
if "%DEVELOPMENT_INSTALL%"=="true" (
    call :print_status "Installing development packages..."
    call :print_status "Consider installing Visual Studio Build Tools or Visual Studio Community"
    call :print_status "Consider installing LLVM/Clang for additional compiler support"
)

call :print_success "System dependencies installation completed"
goto :eof

:: Function to install a package
:install_package
set PACKAGE_NAME=%~1

if "%HAS_CHOCO%"=="true" (
    choco install %PACKAGE_NAME% -y
) else if "%HAS_WINGET%"=="true" (
    winget install %PACKAGE_NAME%
) else (
    call :print_warning "No package manager available for %PACKAGE_NAME%"
    call :print_warning "Please install %PACKAGE_NAME% manually"
    call :print_warning "Consider installing Chocolatey or using winget"
)
goto :eof

:: Function to manually clone the thread_system repo directly
:clone_fresh_thread_system
call :print_status "Cloning thread_system repository directly..."

:: Create a backup of existing thread_system if it exists
if exist "thread_system\" (
    call :print_warning "Backing up existing thread_system directory..."
    set timestamp=%DATE:~6,4%%DATE:~3,2%%DATE:~0,2%_%TIME:~0,2%%TIME:~3,2%%TIME:~6,2%
    set timestamp=!timestamp: =0!
    move "thread_system" "thread_system_backup_!timestamp!"
    call :print_status "Backup created: thread_system_backup_!timestamp!"
)

:: Clone the repository directly
git clone https://github.com/kcenon/thread_system.git
if errorlevel 1 (
    call :print_error "Failed to clone thread_system repository."
    call :print_error "Please check your network connection and that the repository exists."
    exit /b 1
)

call :print_success "Successfully cloned thread_system repository"
goto :eof

:: Function to initialize and update thread_system as a submodule
:init_and_update_submodule
call :print_status "Setting up thread_system as a submodule..."

:: First, check if we're in a git repository
git rev-parse --is-inside-work-tree >nul 2>nul
if errorlevel 1 (
    call :print_error "Not inside a git repository. Cannot use submodule."
    call :print_warning "Try using '--clone-fresh' option instead."
    exit /b 1
)

:: Clean up existing .git in thread_system if it exists
if exist "thread_system\.git\" (
    call :print_status "Removing existing git repository..."
    rmdir /s /q "thread_system\.git"
)

:: Try to add as submodule if not already registered
git submodule status thread_system >nul 2>nul
if errorlevel 1 (
    call :print_status "Adding thread_system as a submodule..."
    git submodule add --force https://github.com/kcenon/thread_system.git
    if errorlevel 1 (
        call :print_error "Failed to add thread_system submodule."
        call :print_error "This might be a temporary network issue or repository access problem."
        exit /b 1
    )
)

:: Initialize and update the submodule
call :print_status "Initializing submodule..."
git submodule update --init --recursive
if errorlevel 1 (
    call :print_warning "Failed to initialize submodule."
    call :print_status "Attempting to deinit and reinitialize..."
    
    git submodule deinit -f thread_system
    
    git submodule update --init --recursive
    if errorlevel 1 (
        call :print_error "Failed to reinitialize submodule after deinit."
        call :print_warning "Consider using '--clone-fresh' option instead."
        exit /b 1
    )
)

call :print_success "thread_system submodule initialized successfully"
goto :eof

:: Function to setup vcpkg
:setup_vcpkg
call :print_status "Setting up vcpkg package manager..."

:: Move to parent directory for vcpkg setup
pushd ..

if "%FORCE_VCPKG%"=="true" (
    if exist "vcpkg\" (
        call :print_warning "Force reinstall requested - removing existing vcpkg..."
        rmdir /s /q "vcpkg"
    )
)

if not exist "vcpkg\" (
    call :print_status "Cloning vcpkg repository..."
    git clone https://github.com/microsoft/vcpkg.git
    if errorlevel 1 (
        call :print_error "Failed to clone vcpkg repository"
        popd
        exit /b 1
    )
) else (
    call :print_status "vcpkg directory already exists, updating..."
    pushd vcpkg
    git pull
    popd
)

pushd vcpkg

:: Bootstrap vcpkg
call :print_status "Bootstrapping vcpkg..."
call bootstrap-vcpkg.bat
if errorlevel 1 (
    call :print_error "Failed to bootstrap vcpkg"
    popd
    popd
    exit /b 1
)

:: Return to original directory
popd
popd

call :print_success "vcpkg setup completed successfully"
goto :eof

:: Function to install vcpkg dependencies
:install_vcpkg_dependencies
call :print_status "Installing PACS system dependencies via vcpkg..."

:: Essential dependencies for PACS system
set ESSENTIAL_DEPS=dcmtk fmt gtest cryptopp
set ADDITIONAL_DEPS=lz4 asio python3 crossguid libpq nlohmann-json spdlog sqlite3 libxml2 openssl zlib

pushd ..\vcpkg

if "%MINIMAL_INSTALL%"=="true" (
    call :print_status "Installing minimal dependencies: %ESSENTIAL_DEPS%"
    .\vcpkg install %ESSENTIAL_DEPS% --recurse
    if errorlevel 1 (
        call :print_error "Failed to install essential vcpkg dependencies"
        popd
        exit /b 1
    )
) else (
    call :print_status "Installing full dependencies: %ESSENTIAL_DEPS% %ADDITIONAL_DEPS%"
    .\vcpkg install %ESSENTIAL_DEPS% %ADDITIONAL_DEPS% --recurse
    if errorlevel 1 (
        call :print_error "Failed to install vcpkg dependencies"
        popd
        exit /b 1
    )
)

popd

call :print_success "vcpkg dependencies installed successfully"
goto :eof