# PACS System - Windows Build Guide

Enhanced Windows build system based on thread_system submodule patterns.

## 🚀 Quick Start

### Prerequisites

- **Visual Studio 2019/2022** or **Build Tools for Visual Studio**
- **Git** (with command-line tools)
- **CMake** 3.16 or later
- **PowerShell** or **Command Prompt**

### Optional Tools

- **Chocolatey** or **winget** (for automatic dependency installation)
- **Visual Studio Code** (for development)
- **LLVM/Clang** (alternative compiler)

## 📦 Installation

### Method 1: Automatic Setup (Recommended)

```batch
# Clone the repository
git clone https://github.com/your-org/pacs_system.git
cd pacs_system

# Setup all dependencies automatically
dependency.bat

# Build the system
build.bat
```

### Method 2: Manual Setup

1. **Install vcpkg** (if not using dependency.bat):
   ```batch
   cd ..
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   ```

2. **Setup thread_system submodule**:
   ```batch
   cd pacs_system
   git submodule add https://github.com/kcenon/thread_system.git
   git submodule update --init --recursive
   ```

3. **Install dependencies**:
   ```batch
   ..\vcpkg\vcpkg install dcmtk fmt gtest cryptopp lz4 asio
   ```

## 🔧 Build Options

### Basic Build Commands

```batch
# Help and information
build.bat --help                    # Show all options
build.bat --list-compilers         # List available compilers

# Basic builds
build.bat                          # Standard release build
build.bat --debug                  # Debug build
build.bat --clean                  # Clean rebuild

# Target-specific builds
build.bat --modules-only           # Build only PACS modules
build.bat --core-only              # Build only core libraries
build.bat --samples                # Build only sample applications

# Advanced options
build.bat --tests                  # Build and run tests
build.bat --docs                   # Generate documentation
build.bat --compiler cl            # Use specific compiler
build.bat --cores 8                # Use 8 CPU cores
build.bat --verbose                # Verbose output
```

### Dependency Script Options

```batch
# Help and information
dependency.bat --help              # Show all options

# Setup options
dependency.bat                     # Standard setup
dependency.bat --minimal           # Install only essential dependencies
dependency.bat --development       # Install development tools
dependency.bat --force-vcpkg       # Force reinstall vcpkg

# thread_system options
dependency.bat --clone-fresh       # Clone thread_system directly
dependency.bat --submodule         # Update submodule to latest
```

## 🎯 Supported Compilers

The build system automatically detects and supports:

- **MSVC** (Microsoft Visual C++ - cl.exe)
- **Clang** (LLVM Clang - clang++.exe)
- **GCC** (MinGW/MSYS2 - g++.exe)

### Compiler Selection

```batch
# List available compilers
build.bat --list-compilers

# Use specific compiler
build.bat --compiler cl              # Use MSVC
build.bat --compiler clang++         # Use Clang
build.bat --compiler g++             # Use GCC

# Interactive selection
build.bat --select-compiler
```

## 📁 Project Structure

```
pacs_system/
├── build.bat                    # Windows build script
├── dependency.bat               # Windows dependency setup
├── README_Windows.md            # This file
├── thread_system/               # Submodule for threading
├── modules/                     # PACS modules
│   ├── storage/                 # DICOM storage service
│   ├── query_retrieve/          # Query/Retrieve service
│   ├── worklist/                # Worklist service
│   └── mpps/                    # MPPS service
├── common/                      # Common utilities
├── core/                        # Core functionality
├── samples/                     # Example applications
└── tests/                       # Test suites
```

## 🔍 Building Specific Components

### PACS Modules Only
```batch
build.bat --modules-only
```
Builds: Storage SCP/SCU, Query/Retrieve SCP/SCU, Worklist SCP/SCU, MPPS SCP/SCU

### Core Libraries Only
```batch
build.bat --core-only
```
Builds: pacs_common, pacs_dicom, pacs_security, thread_base, thread_pool, logger

### Sample Applications
```batch
build.bat --samples
```
Builds: All sample applications demonstrating PACS functionality

## 🧪 Testing

```batch
# Build and run all tests
build.bat --tests

# Manual test execution
cd build
ctest --output-on-failure --build-config Release

# Run integration tests
bin\Release\test_dcmtk_integration.exe
# or
bin\test_dcmtk_integration.exe
```

## 📚 Documentation

```batch
# Generate Doxygen documentation
build.bat --docs

# Clean and regenerate documentation
build.bat --clean-docs

# View documentation
start documents\html\index.html
```

## 🛠️ Development Setup

### For Visual Studio Development

1. **Generate Visual Studio solution**:
   ```batch
   mkdir build && cd build
   cmake .. -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=..\vcpkg\scripts\buildsystems\vcpkg.cmake
   ```

2. **Open solution**:
   ```batch
   start pacs_system.sln
   ```

### For VS Code Development

1. **Install extensions**:
   - C/C++ Extension Pack
   - CMake Tools
   - GitLens

2. **Configure workspace**:
   ```json
   {
     "cmake.configureArgs": [
       "-DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake"
     ]
   }
   ```

## 🚨 Troubleshooting

### Common Issues

**Issue**: `CMake not found in PATH`
```batch
# Install CMake via Chocolatey
choco install cmake

# Or via winget
winget install Kitware.CMake

# Or download from https://cmake.org/download/
```

**Issue**: `Git not found in PATH`
```batch
# Install Git via Chocolatey
choco install git

# Or via winget
winget install Git.Git

# Or download from https://git-scm.com/download/win
```

**Issue**: `No compiler found`
```batch
# Install Visual Studio Build Tools
winget install Microsoft.VisualStudio.2022.BuildTools

# Or install Visual Studio Community
winget install Microsoft.VisualStudio.2022.Community
```

**Issue**: `vcpkg bootstrap failed`
- Ensure you have a C++ compiler installed
- Run `dependency.bat --force-vcpkg` to reinstall vcpkg
- Check that PowerShell execution policy allows script execution

**Issue**: `thread_system submodule failed`
```batch
# Try direct cloning instead
dependency.bat --clone-fresh
```

### Build Failures

1. **Clean build directory**:
   ```batch
   build.bat --clean
   ```

2. **Reinstall dependencies**:
   ```batch
   dependency.bat --force-vcpkg
   ```

3. **Check compiler**:
   ```batch
   build.bat --list-compilers
   ```

4. **Verbose output**:
   ```batch
   build.bat --verbose
   ```

## 🔗 Integration with Package Managers

### Chocolatey Integration
```batch
# Install via Chocolatey
choco install git cmake 7zip python3

# Automatic installation during dependency setup
dependency.bat
```

### winget Integration
```batch
# Install via winget
winget install Git.Git
winget install Kitware.CMake
winget install Microsoft.VisualStudio.2022.BuildTools

# Automatic installation during dependency setup
dependency.bat
```

## 📋 Environment Variables

The build system supports these environment variables:

- `VCPKG_ROOT` - Custom vcpkg installation path
- `CMAKE_GENERATOR` - Preferred CMake generator
- `CC` / `CXX` - C/C++ compiler override

## 🏗️ Advanced Configuration

### Custom Build Types
```batch
# Release with debug info
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Minimum size release
cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel
```

### Static Analysis
```batch
# Use Clang static analyzer (if available)
build.bat --compiler clang++ --verbose
```

### Performance Profiling
```batch
# Build with profiling enabled
build.bat --benchmark --debug
```

## 📞 Support

For Windows-specific issues:

1. Check this README for common solutions
2. Verify all prerequisites are installed
3. Try `dependency.bat --development` for additional tools
4. Use `build.bat --verbose` for detailed error information

## 📄 License

This project is licensed under the BSD 3-Clause License - see the LICENSE file for details.