##################################################
# Examples
##################################################

if(PACS_BUILD_EXAMPLES)
    message(STATUS "")
    message(STATUS "=== Building Examples ===")

    # DCM Dump - DICOM file inspection utility (no network dependencies)
    add_subdirectory(examples/dcm_dump)
    message(STATUS "  [OK] dcm_dump: DICOM file inspection utility")

    # DCM Info - DICOM file summary utility (no network dependencies)
    add_subdirectory(examples/dcm_info)
    message(STATUS "  [OK] dcm_info: DICOM file summary utility")

    # DCM Conv - Transfer Syntax conversion utility (no network dependencies)
    add_subdirectory(examples/dcm_conv)
    message(STATUS "  [OK] dcm_conv: Transfer Syntax conversion utility")

    # DCM Modify - DICOM tag modification utility (no network dependencies)
    add_subdirectory(examples/dcm_modify)
    message(STATUS "  [OK] dcm_modify: DICOM tag modification utility")

    # DCM to JSON - DICOM to JSON conversion utility (DICOM PS3.18)
    add_subdirectory(examples/dcm_to_json)
    message(STATUS "  [OK] dcm_to_json: DICOM to JSON conversion utility")

    # JSON to DCM - JSON to DICOM conversion utility (DICOM PS3.18)
    add_subdirectory(examples/json_to_dcm)
    message(STATUS "  [OK] json_to_dcm: JSON to DICOM conversion utility")

    # DCM to XML - DICOM to XML conversion utility (DICOM Native XML PS3.19)
    add_subdirectory(examples/dcm_to_xml)
    message(STATUS "  [OK] dcm_to_xml: DICOM to XML conversion utility")

    # XML to DCM - XML to DICOM conversion utility (DICOM Native XML PS3.19)
    add_subdirectory(examples/xml_to_dcm)
    message(STATUS "  [OK] xml_to_dcm: XML to DICOM conversion utility")

    # DCM Anonymize - DICOM de-identification utility (PS3.15)
    add_subdirectory(examples/dcm_anonymize)
    message(STATUS "  [OK] dcm_anonymize: DICOM de-identification utility")

    # DCM Dir - DICOMDIR creation/management utility (PS3.10)
    add_subdirectory(examples/dcm_dir)
    message(STATUS "  [OK] dcm_dir: DICOMDIR creation/management utility")

    # Img to DCM - Image to DICOM conversion utility
    add_subdirectory(examples/img_to_dcm)
    message(STATUS "  [OK] img_to_dcm: Image to DICOM conversion utility")

    # DCM Extract - DICOM pixel data extraction utility
    add_subdirectory(examples/dcm_extract)
    message(STATUS "  [OK] dcm_extract: DICOM pixel data extraction utility")

    # Echo SCU/SCP samples - basic connectivity test
    if(TARGET pacs_integration)
        add_subdirectory(examples/echo_scu)
        add_subdirectory(examples/echo_scp)
        message(STATUS "  [OK] echo_scu: DICOM connectivity test client")
        message(STATUS "  [OK] echo_scp: DICOM connectivity test server")
    else()
        message(STATUS "  [--] echo_scu/scp: OFF (requires pacs_integration)")
    endif()

    # Secure Echo SCU/SCP samples - TLS-secured connectivity test
    if(TARGET pacs_integration)
        add_subdirectory(examples/secure_dicom)
        message(STATUS "  [OK] secure_echo_scu: TLS-secured DICOM client")
        message(STATUS "  [OK] secure_echo_scp: TLS-secured DICOM server")
    else()
        message(STATUS "  [--] secure_dicom: OFF (requires pacs_integration)")
    endif()

    # Store SCU sample - DICOM image sender
    if(TARGET pacs_integration)
        add_subdirectory(examples/store_scu)
        message(STATUS "  [OK] store_scu: DICOM image sender")
    else()
        message(STATUS "  [--] store_scu: OFF (requires pacs_integration)")
    endif()

    # Query SCU sample - DICOM C-FIND client
    if(TARGET pacs_integration)
        add_subdirectory(examples/query_scu)
        message(STATUS "  [OK] query_scu: DICOM C-FIND client")
    else()
        message(STATUS "  [--] query_scu: OFF (requires pacs_integration)")
    endif()

    # find_scu - dcmtk-compatible C-FIND SCU utility
    if(TARGET pacs_integration)
        add_subdirectory(examples/find_scu)
        message(STATUS "  [OK] find_scu: dcmtk-compatible C-FIND SCU utility")
    else()
        message(STATUS "  [--] find_scu: OFF (requires pacs_integration)")
    endif()

    # Retrieve SCU sample - DICOM C-MOVE/C-GET client
    if(TARGET pacs_integration)
        add_subdirectory(examples/retrieve_scu)
        message(STATUS "  [OK] retrieve_scu: DICOM C-MOVE/C-GET client")
    else()
        message(STATUS "  [--] retrieve_scu: OFF (requires pacs_integration)")
    endif()

    # move_scu - dcmtk-compatible C-MOVE SCU utility
    if(TARGET pacs_integration)
        add_subdirectory(examples/move_scu)
        message(STATUS "  [OK] move_scu: dcmtk-compatible C-MOVE SCU utility")
    else()
        message(STATUS "  [--] move_scu: OFF (requires pacs_integration)")
    endif()

    # get_scu - dcmtk-compatible C-GET SCU utility
    if(TARGET pacs_integration)
        add_subdirectory(examples/get_scu)
        message(STATUS "  [OK] get_scu: dcmtk-compatible C-GET SCU utility")
    else()
        message(STATUS "  [--] get_scu: OFF (requires pacs_integration)")
    endif()

    # Worklist SCU sample - Modality Worklist Query client
    if(TARGET pacs_integration)
        add_subdirectory(examples/worklist_scu)
        message(STATUS "  [OK] worklist_scu: Modality Worklist query client")
    else()
        message(STATUS "  [--] worklist_scu: OFF (requires pacs_integration)")
    endif()

    # MPPS SCU sample - Modality Performed Procedure Step client
    if(TARGET pacs_integration)
        add_subdirectory(examples/mpps_scu)
        message(STATUS "  [OK] mpps_scu: MPPS N-CREATE/N-SET client")
    else()
        message(STATUS "  [--] mpps_scu: OFF (requires pacs_integration)")
    endif()

    # Print SCU sample - Print Management client
    if(TARGET pacs_integration)
        add_subdirectory(examples/print_scu)
        message(STATUS "  [OK] print_scu: Print Management client")
    else()
        message(STATUS "  [--] print_scu: OFF (requires pacs_integration)")
    endif()

    # Store SCP sample - DICOM image receiver
    if(TARGET pacs_storage AND TARGET pacs_integration)
        add_subdirectory(examples/store_scp)
        message(STATUS "  [OK] store_scp: DICOM image receiver")
    else()
        message(STATUS "  [--] store_scp: OFF (requires pacs_storage and pacs_integration)")
    endif()

    # Query/Retrieve SCP - DICOM Q/R server (C-FIND, C-MOVE, C-GET)
    if(TARGET pacs_storage AND TARGET pacs_integration)
        add_subdirectory(examples/qr_scp)
        message(STATUS "  [OK] qr_scp: DICOM Query/Retrieve server")
    else()
        message(STATUS "  [--] qr_scp: OFF (requires pacs_storage and pacs_integration)")
    endif()

    # Modality Worklist SCP - DICOM MWL server
    if(TARGET pacs_integration)
        add_subdirectory(examples/worklist_scp)
        message(STATUS "  [OK] worklist_scp: Modality Worklist server")
    else()
        message(STATUS "  [--] worklist_scp: OFF (requires pacs_integration)")
    endif()

    # MPPS SCP - DICOM Modality Performed Procedure Step server
    if(TARGET pacs_integration)
        add_subdirectory(examples/mpps_scp)
        message(STATUS "  [OK] mpps_scp: MPPS N-CREATE/N-SET server")
    else()
        message(STATUS "  [--] mpps_scp: OFF (requires pacs_integration)")
    endif()

    # Database Browser - PACS index database viewer
    if(TARGET pacs_storage)
        add_subdirectory(examples/db_browser)
        message(STATUS "  [OK] db_browser: PACS index database viewer")
    else()
        message(STATUS "  [--] db_browser: OFF (requires pacs_storage)")
    endif()

    # PACS Server example requires all modules
    if(TARGET pacs_storage AND TARGET pacs_integration)
        add_subdirectory(examples/pacs_server)
        message(STATUS "  [OK] pacs_server: Complete DICOM archive")
    else()
        message(STATUS "  [--] pacs_server: OFF (requires pacs_storage and pacs_integration)")
    endif()

    # Integration Test Suite - End-to-end workflow tests
    if(TARGET pacs_storage AND TARGET pacs_integration AND TARGET Catch2::Catch2WithMain)
        add_subdirectory(examples/integration_tests)
        message(STATUS "  [OK] pacs_integration_e2e: End-to-end workflow tests")
    else()
        message(STATUS "  [--] pacs_integration_e2e: OFF (requires pacs_storage, pacs_integration, and Catch2)")
    endif()

    # C++20 Module Example (Issue #507)
    add_subdirectory(examples/module_example)
    if(PACS_BUILD_MODULES AND TARGET pacs_system_modules)
        message(STATUS "  [OK] module_example: C++20 module usage demonstration")
    else()
        message(STATUS "  [OK] module_example: C++20 module demo (fallback to headers)")
    endif()

    # CLI Smoke Tests - verify all CLI binaries can execute
    # @see Issue #757 - Add CTest Smoke Tests for All CLI Binaries
    if(PACS_BUILD_TESTS)
        message(STATUS "")
        message(STATUS "=== Registering CLI Smoke Tests ===")

        # File Utilities (always built)
        set(CLI_FILE_TOOLS
            dcm_dump
            dcm_info
            dcm_conv
            dcm_modify
            dcm_anonymize
            dcm_dir
            dcm_to_json
            json_to_dcm
            dcm_to_xml
            xml_to_dcm
            img_to_dcm
            dcm_extract
        )

        # Network Tools (require pacs_integration)
        set(CLI_NETWORK_TOOLS
            echo_scu
            echo_scp
            store_scu
            store_scp
            query_scu
            find_scu
            retrieve_scu
            move_scu
            get_scu
            worklist_scu
            worklist_scp
            mpps_scu
            mpps_scp
            print_scu
            qr_scp
            secure_echo_scu
            secure_echo_scp
        )

        # Storage Tools (require pacs_storage)
        set(CLI_STORAGE_TOOLS
            db_browser
            pacs_server
        )

        # Tools that start a server with default args (no no_args test)
        set(CLI_SERVER_DEFAULT_TOOLS
            pacs_server
        )

        set(CLI_ALL_TOOLS
            ${CLI_FILE_TOOLS}
            ${CLI_NETWORK_TOOLS}
            ${CLI_STORAGE_TOOLS}
        )

        set(_smoke_count 0)
        foreach(tool ${CLI_ALL_TOOLS})
            if(TARGET ${tool})
                # Test 1: --help outputs usage text
                add_test(
                    NAME "cli_smoke::${tool}::help"
                    COMMAND ${tool} --help
                )
                set_tests_properties("cli_smoke::${tool}::help" PROPERTIES
                    LABELS "cli;smoke"
                    TIMEOUT 10
                    PASS_REGULAR_EXPRESSION "Usage|usage|USAGE|Options|options"
                )

                # Test 2: no arguments exits non-zero
                # Skip for tools that start a server with default arguments
                list(FIND CLI_SERVER_DEFAULT_TOOLS ${tool} _is_server_default)
                if(_is_server_default EQUAL -1)
                    add_test(
                        NAME "cli_smoke::${tool}::no_args"
                        COMMAND ${tool}
                    )
                    set_tests_properties("cli_smoke::${tool}::no_args" PROPERTIES
                        LABELS "cli;smoke"
                        TIMEOUT 10
                        WILL_FAIL true
                    )
                endif()

                math(EXPR _smoke_count "${_smoke_count} + 1")
            endif()
        endforeach()

        message(STATUS "  Registered smoke tests for ${_smoke_count} CLI tools")
    endif()
endif()

##################################################
# Benchmarks
##################################################

if(PACS_BUILD_BENCHMARKS)
    message(STATUS "")
    message(STATUS "=== Building Benchmarks ===")

    # Thread Performance Benchmarks
    # @see Issue #154 - Establish performance baseline benchmarks for thread migration
    if(TARGET pacs_integration AND TARGET Catch2::Catch2WithMain)
        add_subdirectory(benchmarks/thread_performance)
        message(STATUS "  [OK] thread_performance_benchmarks: Thread migration baseline")
    else()
        message(STATUS "  [--] thread_performance_benchmarks: OFF (requires pacs_integration and Catch2)")
    endif()

    # SIMD Performance Benchmarks
    # @see Issue #332 - Add SIMD optimization benchmarks
    # @see Issue #313 - Apply SIMD Optimization (Image Processing)
    if(TARGET pacs_encoding AND TARGET Catch2::Catch2WithMain)
        add_subdirectory(benchmarks/simd_performance)
        message(STATUS "  [OK] simd_performance_benchmarks: SIMD optimization measurement")
    else()
        message(STATUS "  [--] simd_performance_benchmarks: OFF (requires pacs_encoding and Catch2)")
    endif()
endif()
