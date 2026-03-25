##################################################
# Options and Utility Functions
##################################################

##################################################
# Dependency Chain Documentation (Option A)
#
# Tier 5: pacs_system
#   Required (Direct):
#     - common_system (Tier 0) - Result<T> pattern
#     - container_system (Tier 1) - DICOM serialization
#     - network_system (Tier 4) - DICOM PDU/Association
#
#   Auto-included via network_system:
#     - thread_system (Tier 1)
#     - logger_system (Tier 2)
#
#   Optional:
#     - monitoring_system (Tier 3) - via network_system config
##################################################

##################################################
# Options
##################################################

option(PACS_BUILD_TESTS "Build unit tests" ON)
option(PACS_BUILD_EXAMPLES "Build examples" OFF)
option(PACS_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(PACS_BUILD_SAMPLES "Build developer samples" OFF)

# Dependency integration options
option(PACS_WITH_COMMON_SYSTEM "Enable common_system integration (REQUIRED)" ON)
option(PACS_WITH_CONTAINER_SYSTEM "Enable container_system integration (REQUIRED)" ON)
option(PACS_WITH_NETWORK_SYSTEM "Enable network_system integration (REQUIRED)" ON)
option(PACS_BUILD_STORAGE "Build storage module (requires SQLite3)" ON)
option(PACS_WITH_AWS_SDK "Enable AWS SDK integration for S3 storage" OFF)
option(PACS_WITH_AZURE_SDK "Enable Azure SDK integration for Blob storage" OFF)
option(PACS_USE_MOCK_S3 "Use mock S3 client instead of AWS SDK (testing only)" OFF)
option(PACS_WARNINGS_AS_ERRORS "Treat warnings as errors (disable in CI if dependency warnings occur)" ON)
option(PACS_BUILD_MODULES "Build C++20 module version of pacs_system" OFF)
option(PACS_BUILD_CODECS "Enable image compression codecs (JPEG, PNG, JPEG2000, JPEG-LS, HTJ2K)" ON)
option(PACS_WITH_OPENSSL "Enable OpenSSL for digital signatures and TLS" ON)
option(PACS_WITH_REST_API "Enable DICOMweb REST API via Crow HTTP framework" ON)

# Prevent mock S3 from being used in Release builds
if(PACS_USE_MOCK_S3 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR "PACS_USE_MOCK_S3 cannot be enabled in Release builds")
endif()

##################################################
# Utility Functions
##################################################

function(pacs_get_export_bridge_target out_var build_target install_target)
    if(NOT TARGET "${build_target}")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()

    string(MAKE_C_IDENTIFIER "${build_target}" _bridge_suffix)
    set(_bridge_target "pacs_export_dep_${_bridge_suffix}")

    if(NOT TARGET ${_bridge_target})
        add_library(${_bridge_target} INTERFACE)

        # Leave bridge targets link-empty during export generation.
        # The installed package config hydrates them after find_dependency()
        # so CMake never resolves install-side package targets back to local
        # unified-build aliases while validating install(EXPORT).
        set_property(GLOBAL APPEND PROPERTY PACS_EXPORT_BRIDGE_MAPPINGS
            "${_bridge_target}=${install_target}"
        )

        set_property(GLOBAL APPEND PROPERTY PACS_EXPORT_BRIDGE_TARGETS ${_bridge_target})
    endif()

    set(${out_var} ${_bridge_target} PARENT_SCOPE)
endfunction()

function(pacs_link_external_dependency target_name visibility build_target install_target)
    pacs_get_export_bridge_target(_bridge_target "${build_target}" "${install_target}")
    if(_bridge_target)
        # Build time: link directly to the local build target.
        # BUILD_LOCAL_INTERFACE ensures this is excluded from all exports.
        target_link_libraries(${target_name} ${visibility}
            "$<BUILD_LOCAL_INTERFACE:${build_target}>"
        )
        # Install time: link to the bridge target (which provides
        # the installed package dependency via INSTALL_INTERFACE).
        target_link_libraries(${target_name} ${visibility}
            "$<INSTALL_INTERFACE:${_bridge_target}>"
        )
    endif()
endfunction()

function(pacs_add_build_interface_include_dirs target_name visibility)
    foreach(_dir IN ITEMS ${ARGN})
        if(_dir)
            target_include_directories(${target_name} ${visibility}
                "$<BUILD_INTERFACE:${_dir}>"
            )
        endif()
    endforeach()
endfunction()

function(pacs_register_export_target target_name export_name)
    if(TARGET ${target_name})
        set_target_properties(${target_name} PROPERTIES EXPORT_NAME ${export_name})
        if(NOT TARGET pacs_system::${export_name})
            add_library(pacs_system::${export_name} ALIAS ${target_name})
        endif()
    endif()
endfunction()
