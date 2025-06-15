@echo off
setlocal EnableDelayedExpansion

:: PACS System Build Script for Windows
:: Enhanced version based on thread_system submodule patterns

:: Display banner
echo.
echo ============================================
echo        PACS System Build Script
echo ============================================
echo.

:: Process command line arguments
set CLEAN_BUILD=0
set RUN_TESTS=0
set BUILD_DOCS=0
set CLEAN_DOCS=0
set BUILD_TYPE=Release
set BUILD_BENCHMARKS=0
set TARGET=all
set BUILD_CORES=0
set VERBOSE=0
set SELECTED_COMPILER=
set LIST_COMPILERS_ONLY=0
set INTERACTIVE_COMPILER_SELECTION=0

:: Parse command line arguments
:parse_args
if "%~1"=="" goto :end_parse
if "%~1"=="--clean" (
    set CLEAN_BUILD=1
    shift
    goto :parse_args
)
if "%~1"=="--debug" (
    set BUILD_TYPE=Debug
    shift
    goto :parse_args
)
if "%~1"=="--tests" (
    set RUN_TESTS=1
    shift
    goto :parse_args
)
if "%~1"=="--benchmark" (
    set BUILD_BENCHMARKS=1
    shift
    goto :parse_args
)
if "%~1"=="--all" (
    set TARGET=all
    shift
    goto :parse_args
)
if "%~1"=="--modules-only" (
    set TARGET=modules-only
    shift
    goto :parse_args
)
if "%~1"=="--samples" (
    set TARGET=samples
    shift
    goto :parse_args
)
if "%~1"=="--core-only" (
    set TARGET=core-only
    shift
    goto :parse_args
)
if "%~1"=="--docs" (
    set BUILD_DOCS=1
    shift
    goto :parse_args
)
if "%~1"=="--clean-docs" (
    set BUILD_DOCS=1
    set CLEAN_DOCS=1
    shift
    goto :parse_args
)
if "%~1"=="--cores" (
    set BUILD_CORES=%~2
    shift
    shift
    goto :parse_args
)
if "%~1"=="--verbose" (
    set VERBOSE=1
    shift
    goto :parse_args
)
if "%~1"=="--compiler" (
    set SELECTED_COMPILER=%~2
    shift
    shift
    goto :parse_args
)
if "%~1"=="--list-compilers" (
    set LIST_COMPILERS_ONLY=1
    shift
    goto :parse_args
)
if "%~1"=="--select-compiler" (
    set INTERACTIVE_COMPILER_SELECTION=1
    shift
    goto :parse_args
)
if "%~1"=="--help" (
    goto :show_help
)
if "%~1"=="/?" (
    goto :show_help
)
echo [ERROR] Unknown option: %~1
goto :show_help

:end_parse

:: Function to detect available compilers
call :detect_compilers

:: Handle compiler-related options
if %LIST_COMPILERS_ONLY%==1 (
    call :show_compilers
    exit /b 0
)

if %INTERACTIVE_COMPILER_SELECTION%==1 (
    call :select_compiler_interactive
) else if not "%SELECTED_COMPILER%"=="" (
    call :select_compiler_by_name "%SELECTED_COMPILER%"
    if errorlevel 1 exit /b 1
) else (
    :: Use the first available compiler as default
    call :use_default_compiler
)

:: Check for dependencies
echo [STATUS] Checking build dependencies...

:: Check for CMake
where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] CMake not found in PATH
    echo [WARNING] Please install CMake and add it to your PATH
    echo [WARNING] You can use 'dependency.bat' to install required dependencies
    exit /b 1
)

:: Check for Git
where git >nul 2>nul
if errorlevel 1 (
    echo [ERROR] Git not found in PATH
    echo [WARNING] Please install Git and add it to your PATH
    echo [WARNING] You can use 'dependency.bat' to install required dependencies
    exit /b 1
)

:: Check for thread_system submodule
if not exist "thread_system\" (
    echo [WARNING] thread_system submodule not found
    echo [WARNING] Running dependency script to set up thread_system...
    
    if exist "dependency.bat" (
        call dependency.bat
        if errorlevel 1 (
            echo [ERROR] Failed to run dependency.bat
            exit /b 1
        )
    ) else (
        echo [ERROR] dependency.bat script not found
        exit /b 1
    )
) else (
    :: Check if thread_system directory is empty
    dir /b thread_system 2>nul | findstr /r /v "^$" >nul
    if errorlevel 1 (
        echo [WARNING] thread_system directory is empty
        echo [WARNING] Running dependency script to set up thread_system...
        
        if exist "dependency.bat" (
            call dependency.bat
            if errorlevel 1 (
                echo [ERROR] Failed to run dependency.bat
                exit /b 1
            )
        ) else (
            echo [ERROR] dependency.bat script not found
            exit /b 1
        )
    )
)

:: Check for vcpkg
if not exist "..\vcpkg\" (
    echo [WARNING] vcpkg not found in parent directory
    echo [WARNING] Running dependency script to set up vcpkg...
    
    if exist "dependency.bat" (
        call dependency.bat
        if errorlevel 1 (
            echo [ERROR] Failed to run dependency.bat
            exit /b 1
        )
    ) else (
        echo [ERROR] dependency.bat script not found
        exit /b 1
    )
)

:: Check for doxygen if building docs
if %BUILD_DOCS%==1 (
    where doxygen >nul 2>nul
    if errorlevel 1 (
        echo [WARNING] Doxygen not found but documentation was requested
        echo [WARNING] Documentation will not be generated
        set BUILD_DOCS=0
    )
)

echo [SUCCESS] All dependencies are satisfied

:: Determine the number of cores to use for building
if %BUILD_CORES%==0 (
    :: Get the number of logical processors
    for /f "tokens=2 delims==" %%i in ('wmic cpu get NumberOfLogicalProcessors /value ^| findstr NumberOfLogicalProcessors') do set BUILD_CORES=%%i
    :: If detection failed, use a safe default
    if %BUILD_CORES%==0 set BUILD_CORES=4
)

echo [STATUS] Using %BUILD_CORES% cores for compilation

:: Check for platform-specific settings
echo [STATUS] Detected Windows platform

:: Clean build if requested
if %CLEAN_BUILD%==1 (
    echo [STATUS] Performing clean build...
    if exist build rmdir /s /q build
)

:: Create build directory if it doesn't exist
if not exist build (
    echo [STATUS] Creating build directory...
    mkdir build
)

:: Prepare CMake arguments
set CMAKE_ARGS=-DCMAKE_TOOLCHAIN_FILE=..\vcpkg\scripts\buildsystems\vcpkg.cmake -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if not "%SELECTED_COMPILER_PATH%"=="" (
    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_CXX_COMPILER="%SELECTED_COMPILER_PATH%"
)

:: Add feature flags based on options
if %BUILD_BENCHMARKS%==1 (
    set CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_BENCHMARKS=ON
)

:: Set target-specific options
if "%TARGET%"=="modules-only" (
    set CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_MODULES_ONLY=ON
) else if "%TARGET%"=="core-only" (
    set CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_CORE_ONLY=ON
) else if "%TARGET%"=="samples" (
    set CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_SAMPLES_ONLY=ON
)

:: Enter build directory
pushd build

:: Run CMake configuration
echo [STATUS] Configuring project with CMake...
echo [STATUS] CMake args: %CMAKE_ARGS%
cmake .. %CMAKE_ARGS%

:: Check if CMake configuration was successful
if errorlevel 1 (
    echo [ERROR] CMake configuration failed. See the output above for details.
    popd
    exit /b 1
)

:: Determine build target based on option
set "BUILD_TARGET="
if "%TARGET%"=="all" (
    set "BUILD_TARGET=ALL_BUILD"
) else if "%TARGET%"=="modules-only" (
    set "BUILD_TARGET=pacs_storage_scp;pacs_storage_scu;pacs_query_retrieve_scp;pacs_query_retrieve_scu;pacs_worklist_scp;pacs_worklist_scu;pacs_mpps_scp;pacs_mpps_scu"
) else if "%TARGET%"=="core-only" (
    set "BUILD_TARGET=pacs_common;pacs_dicom;pacs_security;thread_base;thread_pool;logger;utilities"
) else if "%TARGET%"=="samples" (
    set "BUILD_TARGET=storage_scp_sample;storage_scu_sample;query_retrieve_scp_sample;query_retrieve_scu_sample"
)

:: Set build verbosity
if %VERBOSE%==1 (
    set "CMAKE_BUILD_ARGS=--verbose"
) else (
    set "CMAKE_BUILD_ARGS="
)

:: Build the project
echo [STATUS] Building project in %BUILD_TYPE% mode...

:: Use MSBuild for Windows
set BUILD_FAILED=0
if not "%BUILD_TARGET%"=="" (
    if "%BUILD_TARGET%"=="ALL_BUILD" (
        echo [STATUS] Building all targets
        cmake --build . --config %BUILD_TYPE% -- /maxcpucount:%BUILD_CORES% %CMAKE_BUILD_ARGS%
        if errorlevel 1 set BUILD_FAILED=1
    ) else (
        :: Parse multiple targets separated by semicolons for Windows
        for %%t in (%BUILD_TARGET:;= %) do (
            echo [STATUS] Building target: %%t
            cmake --build . --config %BUILD_TYPE% --target %%t -- /maxcpucount:%BUILD_CORES% %CMAKE_BUILD_ARGS%
            if errorlevel 1 (
                echo [WARNING] Target %%t failed to build or does not exist
                set BUILD_FAILED=1
            )
        )
    )
) else (
    cmake --build . --config %BUILD_TYPE% -- /maxcpucount:%BUILD_CORES% %CMAKE_BUILD_ARGS%
    if errorlevel 1 set BUILD_FAILED=1
)

:: Check if any build failed
if %BUILD_FAILED%==1 (
    echo [WARNING] Some targets failed to build. Check the warnings above.
    :: Don't exit with error if some targets succeeded
)

:: Run tests if requested
if %RUN_TESTS%==1 (
    echo [STATUS] Running tests...
    
    :: Set locale
    set LC_ALL=C
    set LANGUAGE=
    
    :: Run CTest
    ctest --output-on-failure --build-config %BUILD_TYPE%
    set TEST_RESULT=!errorlevel!
    
    :: Also run integration tests if available
    if exist "bin\%BUILD_TYPE%\test_dcmtk_integration.exe" (
        echo [STATUS] Running DCMTK integration tests...
        bin\%BUILD_TYPE%\test_dcmtk_integration.exe
        set INTEGRATION_TEST_RESULT=!errorlevel!
        
        if !INTEGRATION_TEST_RESULT! neq 0 (
            echo [WARNING] Integration tests failed with exit code !INTEGRATION_TEST_RESULT!
        ) else (
            echo [SUCCESS] Integration tests passed!
        )
    ) else if exist "bin\test_dcmtk_integration.exe" (
        echo [STATUS] Running DCMTK integration tests...
        bin\test_dcmtk_integration.exe
        set INTEGRATION_TEST_RESULT=!errorlevel!
        
        if !INTEGRATION_TEST_RESULT! neq 0 (
            echo [WARNING] Integration tests failed with exit code !INTEGRATION_TEST_RESULT!
        ) else (
            echo [SUCCESS] Integration tests passed!
        )
    )
    
    if !TEST_RESULT! neq 0 (
        echo [ERROR] Tests failed with exit code !TEST_RESULT!
        popd
        exit /b !TEST_RESULT!
    ) else (
        echo [SUCCESS] All tests passed!
    )
)

:: Return to original directory
popd

:: Generate documentation if requested
if %BUILD_DOCS%==1 (
    echo [STATUS] Generating Doxygen documentation...
    
    :: Create docs directory if it doesn't exist
    if not exist documents mkdir documents
    
    :: Clean docs if requested
    if %CLEAN_DOCS%==1 (
        echo [STATUS] Cleaning documentation directory...
        if exist documents\html rmdir /s /q documents\html
    )
    
    :: Check if doxygen is installed
    where doxygen >nul 2>nul
    if errorlevel 1 (
        echo [ERROR] Doxygen is not installed. Please install it to generate documentation.
        exit /b 1
    )
    
    :: Run doxygen
    doxygen Doxyfile
    
    :: Check if documentation generation was successful
    if errorlevel 1 (
        echo [ERROR] Documentation generation failed. See the output above for details.
    ) else (
        echo [SUCCESS] Documentation generated successfully in the documents directory!
    )
)

:: Show completion message
if %BUILD_FAILED%==0 (
    echo [SUCCESS] Build completed successfully!
) else (
    echo [INFO] Build completed with some warnings. Check the output above.
)

:: Final success message
echo.
echo ============================================
echo       PACS System Build Complete
echo ============================================
echo.

:: Check for executables in both Release and Debug directories
set FOUND_EXECUTABLES=0
if exist build\bin\%BUILD_TYPE%\ (
    echo Available executables in %BUILD_TYPE%:
    dir /b build\bin\%BUILD_TYPE%\*.exe 2>nul
    if not errorlevel 1 set FOUND_EXECUTABLES=1
)
if exist build\bin\ (
    if %FOUND_EXECUTABLES%==0 (
        echo Available executables:
        dir /b build\bin\*.exe 2>nul
    )
)

echo Build type: %BUILD_TYPE%
echo Target: %TARGET%
echo Compiler: %SELECTED_COMPILER_PATH%

if %BUILD_BENCHMARKS%==1 (
    echo Benchmarks: Enabled
)

if %BUILD_DOCS%==1 (
    echo Documentation: Generated
)

exit /b 0

:show_help
echo Usage: %~nx0 [options]
echo.
echo Build Options:
echo   --clean           Perform a clean rebuild by removing the build directory
echo   --debug           Build in debug mode (default is release)
echo   --tests           Build and run tests
echo   --benchmark       Build with performance benchmarks enabled
echo.
echo Target Options:
echo   --all             Build all targets (default)
echo   --modules-only    Build only PACS modules (storage, query_retrieve, etc.)
echo   --samples         Build only sample applications
echo   --core-only       Build only core libraries
echo.
echo Documentation Options:
echo   --docs            Generate Doxygen documentation
echo   --clean-docs      Clean and regenerate Doxygen documentation
echo.
echo General Options:
echo   --cores N         Use N cores for compilation (default: auto-detect)
echo   --verbose         Show detailed build output
echo   --help            Display this help and exit
echo.
echo Compiler Options:
echo   --compiler NAME   Use specific compiler (e.g., cl, clang++, g++)
echo   --list-compilers  List all available compilers and exit
echo   --select-compiler Interactively select compiler
echo.
echo Examples:
echo   %~nx0 --clean --tests    # Clean build and run tests
echo   %~nx0 --modules-only     # Build only PACS modules
echo   %~nx0 --docs             # Build with documentation
echo   %~nx0 --compiler cl      # Use MSVC compiler
echo.
exit /b 0

:: Function to detect available compilers
:detect_compilers
echo [STATUS] Detecting available compilers...

set COMPILER_COUNT=0
set AVAILABLE_COMPILERS=
set COMPILER_DETAILS=

:: Check for MSVC (Visual Studio)
where cl >nul 2>nul
if not errorlevel 1 (
    set /a COMPILER_COUNT+=1
    set AVAILABLE_COMPILERS=%AVAILABLE_COMPILERS% cl
    for /f "tokens=*" %%i in ('cl 2^>^&1 ^| findstr /C:"Microsoft"') do (
        set COMPILER_DETAILS=%COMPILER_DETAILS%; MSVC: %%i
    )
)

:: Check for Clang
where clang++ >nul 2>nul
if not errorlevel 1 (
    set /a COMPILER_COUNT+=1
    set AVAILABLE_COMPILERS=%AVAILABLE_COMPILERS% clang++
    for /f "tokens=1-3" %%i in ('clang++ --version 2^>nul ^| findstr /C:"clang"') do (
        set COMPILER_DETAILS=%COMPILER_DETAILS%; Clang: %%i %%j %%k
        goto :clang_done
    )
    :clang_done
)

:: Check for GCC (MinGW/MSYS2)
where g++ >nul 2>nul
if not errorlevel 1 (
    set /a COMPILER_COUNT+=1
    set AVAILABLE_COMPILERS=%AVAILABLE_COMPILERS% g++
    for /f "tokens=1-3" %%i in ('g++ --version 2^>nul ^| findstr /C:"g++"') do (
        set COMPILER_DETAILS=%COMPILER_DETAILS%; GCC: %%i %%j %%k
        goto :gcc_done
    )
    :gcc_done
)

:: Check for specific versions
for %%v in (13 12 11 10 9) do (
    where g++-%%v >nul 2>nul
    if not errorlevel 1 (
        set /a COMPILER_COUNT+=1
        set AVAILABLE_COMPILERS=%AVAILABLE_COMPILERS% g++-%%v
        set COMPILER_DETAILS=%COMPILER_DETAILS%; GCC-%%v: found
    )
)

for %%v in (17 16 15 14 13 12 11 10) do (
    where clang++-%%v >nul 2>nul
    if not errorlevel 1 (
        set /a COMPILER_COUNT+=1
        set AVAILABLE_COMPILERS=%AVAILABLE_COMPILERS% clang++-%%v
        set COMPILER_DETAILS=%COMPILER_DETAILS%; Clang-%%v: found
    )
)

if %COMPILER_COUNT%==0 (
    echo [ERROR] No C++ compilers found!
    echo [WARNING] Please install a C++ compiler (MSVC, Clang, or GCC/MinGW)
    echo [INFO] Install Visual Studio Build Tools, LLVM/Clang, or MinGW-w64
    exit /b 1
)

echo [STATUS] Found %COMPILER_COUNT% compiler(s)
goto :eof

:: Function to show available compilers
:show_compilers
echo Available Compilers:
set INDEX=1
for %%c in (%AVAILABLE_COMPILERS%) do (
    echo   !INDEX!^) %%c
    set /a INDEX+=1
)
echo.
goto :eof

:: Function to select compiler interactively
:select_compiler_interactive
call :show_compilers

:select_loop
set /p "choice=Select compiler (1-%COMPILER_COUNT%) or 'q' to quit: "

if /i "%choice%"=="q" (
    echo [WARNING] Build cancelled by user
    exit /b 0
)

:: Validate choice
set /a "valid_choice=0"
if "%choice%" geq "1" if "%choice%" leq "%COMPILER_COUNT%" set valid_choice=1

if %valid_choice%==0 (
    echo [ERROR] Invalid choice. Please enter a number between 1 and %COMPILER_COUNT%
    goto :select_loop
)

:: Get selected compiler
set INDEX=1
for %%c in (%AVAILABLE_COMPILERS%) do (
    if !INDEX!==!choice! (
        set SELECTED_COMPILER_PATH=%%c
        echo [SUCCESS] Selected: %%c
        goto :eof
    )
    set /a INDEX+=1
)
goto :eof

:: Function to select compiler by name
:select_compiler_by_name
set COMPILER_NAME=%~1
set FOUND=0

for %%c in (%AVAILABLE_COMPILERS%) do (
    if /i "%%c"=="%COMPILER_NAME%" (
        set SELECTED_COMPILER_PATH=%%c
        set FOUND=1
        echo [SUCCESS] Selected: %%c
        goto :found_compiler
    )
)

:found_compiler
if %FOUND%==0 (
    echo [ERROR] Compiler '%COMPILER_NAME%' not found in available compilers
    call :show_compilers
    exit /b 1
)
goto :eof

:: Function to use default compiler
:use_default_compiler
for %%c in (%AVAILABLE_COMPILERS%) do (
    set SELECTED_COMPILER_PATH=%%c
    echo [STATUS] Using default compiler: %%c
    goto :eof
)
goto :eof