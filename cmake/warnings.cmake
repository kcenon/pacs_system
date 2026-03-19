##################################################
# Apply Compiler Warnings to PACS Targets Only
#
# This must be done AFTER all pacs_* targets are defined
# and AFTER dependencies are added via add_subdirectory().
# This ensures warning flags are NOT inherited by dependencies.
##################################################

message(STATUS "")
message(STATUS "=== Applying Compiler Warnings to PACS Targets ===")

# Core PACS libraries
pacs_apply_warnings(pacs_core)
pacs_apply_warnings(pacs_encoding)
pacs_apply_warnings(pacs_network)
pacs_apply_warnings(pacs_services)

# Suppress deprecated warnings from database_system for pacs_services (Issue #420)
if(TARGET database)
    target_compile_options(pacs_services PRIVATE
        $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-deprecated-declarations>
        $<$<CXX_COMPILER_ID:MSVC>:/wd4996>
    )
endif()

# Optional PACS libraries
if(TARGET pacs_storage)
    pacs_apply_warnings(pacs_storage)
endif()

if(TARGET pacs_client)
    pacs_apply_warnings(pacs_client)
endif()

if(TARGET pacs_ai)
    pacs_apply_warnings(pacs_ai)
endif()

if(TARGET pacs_monitoring)
    pacs_apply_warnings(pacs_monitoring)
endif()

if(TARGET pacs_workflow)
    pacs_apply_warnings(pacs_workflow)
endif()

if(TARGET pacs_integration)
    pacs_apply_warnings(pacs_integration)
endif()

# Test executables
if(PACS_BUILD_TESTS)
    pacs_apply_warnings(core_tests)
    pacs_apply_warnings(encoding_tests)
    pacs_apply_warnings(network_tests)
    pacs_apply_warnings(services_tests)
    if(TARGET storage_tests)
        pacs_apply_warnings(storage_tests)
        # Suppress deprecated warnings from database_system headers
        target_compile_options(storage_tests PRIVATE
            $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-deprecated-declarations>
            $<$<CXX_COMPILER_ID:MSVC>:/wd4996>
        )
    endif()
    if(TARGET ai_tests)
        pacs_apply_warnings(ai_tests)
    endif()
    if(TARGET workflow_tests)
        pacs_apply_warnings(workflow_tests)
    endif()
    if(TARGET monitoring_tests)
        pacs_apply_warnings(monitoring_tests)
    endif()
    if(TARGET pacs_integration_tests)
        pacs_apply_warnings(pacs_integration_tests)
    endif()
    if(TARGET di_tests)
        pacs_apply_warnings(di_tests)
    endif()
endif()

# Benchmark executables
if(PACS_BUILD_BENCHMARKS)
    if(TARGET thread_performance_benchmarks)
        pacs_apply_warnings(thread_performance_benchmarks)
    endif()
endif()

if(PACS_WARNINGS_AS_ERRORS)
    message(STATUS "  Warning flags: ${PACS_WARNING_FLAGS} (APPLIED TO PACS TARGETS ONLY)")
else()
    message(STATUS "  Warning flags: ${PACS_WARNING_FLAGS} (warnings-as-errors disabled)")
endif()
message(STATUS "")

##################################################
# Developer Samples
##################################################

if(PACS_BUILD_SAMPLES)
    message(STATUS "")
    message(STATUS "=== Building Developer Samples ===")
    add_subdirectory(samples)
    pacs_apply_warnings(pacs_samples_common)
    message(STATUS "Developer samples: ON")
endif()
