# Testing
if(PACS_BUILD_TESTS)
    enable_testing()

    # Fetch Catch2 for testing
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)

    # Helper function to link database dependencies with proper ordering for Linux
    # On Linux, static library link order matters - using --start-group/--end-group
    # resolves circular dependencies between integrated_database and monitoring_system.
    function(pacs_link_database_deps TARGET_NAME)
        if(TARGET integrated_database AND TARGET monitoring_system)
            if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
                target_link_libraries(${TARGET_NAME} PRIVATE
                    -Wl,--start-group
                    integrated_database
                    monitoring_system
                    -Wl,--end-group
                )
            else()
                target_link_libraries(${TARGET_NAME} PRIVATE
                    integrated_database
                    monitoring_system
                )
            endif()
        elseif(TARGET integrated_database)
            target_link_libraries(${TARGET_NAME} PRIVATE integrated_database)
        elseif(TARGET monitoring_system)
            target_link_libraries(${TARGET_NAME} PRIVATE monitoring_system)
        endif()
    endfunction()

    # Create tests directory if it doesn't exist
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/core)
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/encoding)
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/network)
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/services)
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/storage)
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/integration)

    # Core tests
    add_executable(core_tests
        tests/core/dicom_tag_test.cpp
        tests/core/dicom_element_test.cpp
        tests/core/dicom_dataset_test.cpp
        tests/core/dicom_file_test.cpp
        tests/core/tag_info_test.cpp
        tests/core/dicom_dictionary_test.cpp
        tests/core/events_test.cpp
        tests/core/private_tag_registry_test.cpp
    )
    target_link_libraries(core_tests
        PRIVATE
            pacs_core
            pacs_encoding
            Catch2::Catch2WithMain
    )
    target_include_directories(core_tests PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})

    # Utilities tests (dcm_modify, etc.)
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/utilities)
    add_executable(utilities_tests
        tests/utilities/dcm_modify_test.cpp
    )
    target_link_libraries(utilities_tests
        PRIVATE
            pacs_core
            pacs_encoding
            Catch2::Catch2WithMain
    )

    # Encoding tests
    add_executable(encoding_tests
        tests/encoding/transfer_syntax_test.cpp
        tests/encoding/vr_type_test.cpp
        tests/encoding/vr_info_test.cpp
        tests/encoding/implicit_vr_codec_test.cpp
        tests/encoding/explicit_vr_codec_test.cpp
        tests/encoding/byte_swap_test.cpp
        tests/encoding/explicit_vr_big_endian_codec_test.cpp
        tests/encoding/compression/jpeg_baseline_codec_test.cpp
        tests/encoding/compression/jpeg_lossless_codec_test.cpp
        tests/encoding/compression/frame_deflate_codec_test.cpp
        tests/encoding/compression/hevc_codec_test.cpp
        tests/encoding/compression/htj2k_codec_test.cpp
        tests/encoding/compression/jpeg2000_codec_test.cpp
        tests/encoding/compression/jpeg_ls_codec_test.cpp
        tests/encoding/compression/jpegxl_codec_test.cpp
        tests/encoding/compression/rle_codec_test.cpp
        tests/encoding/simd/simd_rle_test.cpp
        tests/encoding/character_set_test.cpp
    )
    target_link_libraries(encoding_tests
        PRIVATE
            pacs_encoding
            Catch2::Catch2WithMain
    )

    # Network tests
    add_executable(network_tests
        tests/network/pdu_encoder_test.cpp
        tests/network/pdu_decoder_test.cpp
        tests/network/association_test.cpp
        tests/network/dicom_server_test.cpp
        tests/network/dimse/dimse_message_test.cpp
        tests/network/dimse/n_service_test.cpp
        tests/network/detail/accept_worker_test.cpp
        tests/network/v2/dicom_association_handler_test.cpp
        tests/network/v2/dicom_server_v2_test.cpp
        tests/network/v2/pdu_framing_test.cpp
        tests/network/v2/state_machine_test.cpp
        tests/network/v2/service_dispatching_test.cpp
        tests/network/v2/network_system_integration_test.cpp
        tests/network/v2/stress_test.cpp
        # Pipeline tests (Issue #524)
        tests/network/pipeline/pipeline_job_types_test.cpp
        tests/network/pipeline/pipeline_coordinator_test.cpp
        tests/network/pipeline/pipeline_integration_test.cpp
    )
    target_link_libraries(network_tests
        PRIVATE
            pacs_network
            pacs_services
            Catch2::Catch2WithMain
    )
    # Link pacs_integration for thread_pool_adapter (used by dicom_server)
    if(TARGET pacs_integration)
        target_link_libraries(network_tests PRIVATE pacs_integration)
    endif()

    # Services tests
    add_executable(services_tests
        tests/services/verification_scp_test.cpp
        tests/services/storage_scp_test.cpp
        tests/services/storage_scu_test.cpp
        tests/services/retrieve_scu_test.cpp
        tests/services/query_scp_test.cpp
        tests/services/retrieve_scp_test.cpp
        tests/services/worklist_scp_test.cpp
        tests/services/worklist_scu_test.cpp
        tests/services/mpps_scp_test.cpp
        tests/services/mpps_scu_test.cpp
        tests/services/n_get_scp_test.cpp
        tests/services/n_get_scu_test.cpp
        tests/services/storage_commitment_types_test.cpp
        tests/services/storage_commitment_scp_test.cpp
        tests/services/storage_commitment_scu_test.cpp
        tests/services/print_scp_test.cpp
        tests/services/print_scu_test.cpp
        tests/services/ups_push_scp_test.cpp
        tests/services/ups_push_scu_test.cpp
        tests/services/ups_query_scp_test.cpp
        tests/services/ups_watch_scp_test.cpp
        tests/services/ups_watch_scu_test.cpp
        tests/services/us_storage_test.cpp
        tests/services/dx_storage_test.cpp
        tests/services/mg_storage_test.cpp
        tests/services/pet_storage_test.cpp
        tests/services/nm_storage_test.cpp
        tests/services/mg_iod_validator_test.cpp
        tests/services/pet_iod_validator_test.cpp
        tests/services/nm_iod_validator_test.cpp
        tests/services/rt_storage_test.cpp
        tests/services/rt_iod_validator_test.cpp
        tests/services/seg_storage_test.cpp
        tests/services/sr_storage_test.cpp
        tests/services/ct_storage_test.cpp
        tests/services/wsi_storage_test.cpp
        tests/services/ophthalmic_storage_test.cpp
        tests/services/parametric_map_storage_test.cpp
        tests/services/ct_iod_validator_test.cpp
        tests/services/mr_iod_validator_test.cpp
        tests/services/wsi_iod_validator_test.cpp
        tests/services/ophthalmic_iod_validator_test.cpp
        tests/services/parametric_map_iod_validator_test.cpp
        tests/services/ct_processing_iod_validator_test.cpp
        tests/services/heightmap_seg_iod_validator_test.cpp
        tests/services/label_map_seg_iod_validator_test.cpp
        tests/services/pir/patient_reconciliation_test.cpp
        tests/services/xds/xds_imaging_test.cpp
        tests/services/waveform_storage_test.cpp
        tests/services/cache/simple_lru_cache_test.cpp
        tests/services/cache/query_cache_test.cpp
        tests/services/cache/streaming_query_test.cpp
        tests/services/cache/parallel_query_executor_test.cpp
    )

    # Add monitoring tests conditionally (use TARGET check, not CMake variable)
    if(TARGET database)
        target_sources(services_tests PRIVATE
            tests/services/monitoring/database_metrics_service_test.cpp
            tests/services/cache/sql_injection_prevention_test.cpp
        )
    endif()

    target_link_libraries(services_tests
        PRIVATE
            pacs_services
            pacs_storage
            Catch2::Catch2WithMain
    )
    # Link pacs_integration for thread_pool_adapter (used by parallel_query_executor tests)
    if(TARGET pacs_integration)
        target_link_libraries(services_tests PRIVATE pacs_integration)
    endif()
    # Link integrated_database and monitoring_system for database_cursor tests (Issue #642)
    pacs_link_database_deps(services_tests)

    # Security tests (Issue #193 - RBAC, Issue #191 - Digital Signatures, Issue #192 - Anonymization)
    set(SECURITY_TEST_SOURCES
        tests/security/access_control_manager_test.cpp
        tests/security/sqlite_security_storage_test.cpp
        tests/security/uid_mapping_test.cpp
        tests/security/anonymizer_test.cpp
        tests/security/atna_audit_logger_test.cpp
        tests/security/atna_syslog_transport_test.cpp
        tests/security/atna_service_auditor_test.cpp
        tests/security/atna_config_test.cpp
        tests/security/tls_policy_test.cpp
        tests/security/audit_log_cipher_test.cpp
    )

    # Add digital signature tests if OpenSSL is available (Issue #191)
    if(PACS_OPENSSL_FOUND)
        list(APPEND SECURITY_TEST_SOURCES
            tests/security/digital_signature_test.cpp
        )
    endif()

    add_executable(security_tests ${SECURITY_TEST_SOURCES})
    target_link_libraries(security_tests
        PRIVATE
            pacs_security
            pacs_storage
            Catch2::Catch2WithMain
    )
    # Link integrated_database and monitoring_system for database dependencies (Issue #642)
    pacs_link_database_deps(security_tests)

    # Storage tests
    if(TARGET pacs_storage)
        add_executable(storage_tests
            tests/storage/storage_interface_test.cpp
            tests/storage/file_storage_test.cpp
            tests/storage/s3_storage_test.cpp
            tests/storage/azure_blob_storage_test.cpp
            tests/storage/hsm_storage_test.cpp
            tests/storage/migration_runner_test.cpp
            tests/storage/index_database_test.cpp
            tests/storage/mpps_test.cpp
            tests/storage/worklist_test.cpp
            tests/storage/ups_workitem_test.cpp
            tests/storage/pacs_database_adapter_test.cpp
            tests/storage/base_repository_test.cpp
            tests/storage/commitment_repository_test.cpp
            tests/storage/patient_repository_test.cpp
            tests/storage/study_repository_test.cpp
            tests/storage/series_repository_test.cpp
            tests/storage/instance_repository_test.cpp
            tests/storage/mpps_repository_test.cpp
            tests/storage/worklist_repository_test.cpp
            tests/storage/ups_repository_test.cpp
            tests/storage/audit_repository_test.cpp
            tests/storage/repository_factory_test.cpp
            # Annotation & Measurement API integration tests (Issue #545, #584)
            tests/integration/annotation_api_test.cpp
            tests/integration/measurement_api_test.cpp
        )
        target_link_libraries(storage_tests
            PRIVATE
                pacs_storage
                pacs_services
                Catch2::Catch2WithMain
                $<$<TARGET_EXISTS:database>:database>
        )
        # Use mock S3/Azure clients in tests regardless of SDK availability (Issue #724)
        target_compile_definitions(storage_tests PRIVATE PACS_USE_MOCK_S3 PACS_USE_MOCK_AZURE)
        # Link integrated_database and monitoring_system for database adapter tests (Issue #642)
        pacs_link_database_deps(storage_tests)
    endif()

    # AI tests (Issue #204 - AI result handler for SR/SEG/PR objects,
    #           Issue #205 - AI service connector for inference requests)
    if(TARGET pacs_ai)
        add_executable(ai_tests
            tests/ai/ai_result_handler_test.cpp
            tests/ai/ai_service_connector_test.cpp
            tests/ai/aira_assessment_test.cpp
        )
        target_link_libraries(ai_tests
            PRIVATE
                pacs_ai
                Catch2::Catch2WithMain
        )
        # Link pacs_integration for logger/monitoring adapters (required by ai_service_connector)
        if(TARGET pacs_integration)
            target_link_libraries(ai_tests PRIVATE pacs_integration)
        endif()
    endif()

    # Workflow tests (Issue #206 - auto prefetch service, Issue #207 - task scheduler)
    if(TARGET pacs_workflow)
        add_executable(workflow_tests
            tests/workflow/auto_prefetch_service_test.cpp
            tests/workflow/task_scheduler_test.cpp
            tests/workflow/study_lock_manager_test.cpp
        )
        target_link_libraries(workflow_tests
            PRIVATE
                pacs_workflow
                Catch2::Catch2WithMain
        )
        # Link pacs_integration for logger/monitoring/thread adapters
        if(TARGET pacs_integration)
            target_link_libraries(workflow_tests PRIVATE pacs_integration)
        endif()
    endif()

    # Monitoring tests (Issue #211 - health check endpoint, Issue #210 - metrics)
    if(TARGET pacs_monitoring)
        add_executable(monitoring_tests
            tests/monitoring/health_status_test.cpp
            tests/monitoring/health_checker_test.cpp
            tests/monitoring/health_json_test.cpp
            tests/monitoring/pacs_metrics_test.cpp
            tests/monitoring/collectors_test.cpp
        )
        target_link_libraries(monitoring_tests
            PRIVATE
                pacs_monitoring
                Catch2::Catch2WithMain
        )
    endif()

    # Web tests (Issue #194 - REST API server)
    if(TARGET pacs_web)
        add_executable(web_tests
            tests/web/rest_server_test.cpp
            tests/web/system_endpoints_test.cpp
            tests/web/patient_study_endpoints_test.cpp
            tests/web/worklist_audit_endpoints_test.cpp
            tests/web/dicomweb_endpoints_test.cpp
            tests/web/jobs_endpoints_test.cpp
            tests/web/thumbnail_service_test.cpp
            tests/web/metadata_service_test.cpp
            tests/web/annotation_endpoints_test.cpp
            tests/web/measurement_endpoints_test.cpp
            tests/web/key_image_endpoints_test.cpp
            tests/web/viewer_state_endpoints_test.cpp
            tests/web/wado_uri_endpoints_test.cpp
            tests/web/storage_commitment_endpoints_test.cpp
            tests/web/auth/jwt_validator_test.cpp
            tests/web/auth/oauth2_middleware_test.cpp
        )
        target_link_libraries(web_tests
            PRIVATE
                pacs_web
                pacs_services
                pacs_client
                Catch2::Catch2WithMain
        )
        # Link integrated_database and monitoring_system for database dependencies (Issue #642)
        pacs_link_database_deps(web_tests)
    endif()

    # Integration tests
    if(TARGET pacs_integration)
        add_executable(pacs_integration_tests
            tests/integration/container_adapter_test.cpp
            tests/integration/logger_adapter_test.cpp
            tests/integration/thread_system_direct_test.cpp
            tests/integration/network_adapter_test.cpp
            tests/integration/monitoring_adapter_test.cpp
            # Cross-system integration tests (Issue #390)
            tests/integration/dicom_workflow_integration_test.cpp
            tests/integration/error_propagation_integration_test.cpp
            tests/integration/load_integration_test.cpp
            tests/integration/shutdown_integration_test.cpp
            tests/integration/config_reload_integration_test.cpp
        )
        target_link_libraries(pacs_integration_tests
            PRIVATE
                pacs_integration
                pacs_network
                Catch2::Catch2WithMain
        )
        # Link integrated_database and monitoring_system for database dependencies (Issue #642)
        pacs_link_database_deps(pacs_integration_tests)
    endif()

    # XDS-I.b Gazelle-style registry integration test (Issue #1115)
    # Exercises the full ITI-41 / ITI-43 round trip against an in-process
    # mock registry, asserting metadata parity (entry_uuid, unique_id,
    # patient ID CX form, MIME type) between submit and retrieve.
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/integration/services/xds)
    add_executable(xds_gazelle_integration_tests
        tests/integration/services/xds/xds_gazelle_roundtrip_test.cpp
    )
    target_link_libraries(xds_gazelle_integration_tests
        PRIVATE
            pacs_services
            Catch2::Catch2WithMain
    )

    # IHE XDS.b Document Source (ITI-41) tests (Issue #1128)
    # Only defined when the pacs_ihe_xds target was created (requires pugixml,
    # libcurl, and OpenSSL). Tests reach into internal headers under
    # src/ihe/xds/common/ for the envelope / signer / packager layers, plus
    # the public document_source.h for the end-to-end orchestrator.
    if(TARGET pacs_ihe_xds)
        file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/ihe/xds)
        add_executable(pacs_ihe_xds_tests
            tests/ihe/xds/soap_envelope_test.cpp
            tests/ihe/xds/wss_signer_test.cpp
            tests/ihe/xds/mtom_packager_test.cpp
            tests/ihe/xds/document_source_test.cpp
        )
        target_link_libraries(pacs_ihe_xds_tests
            PRIVATE
                pacs_ihe_xds
                OpenSSL::SSL
                OpenSSL::Crypto
                Catch2::Catch2WithMain
        )
        if(TARGET pugixml::pugixml)
            target_link_libraries(pacs_ihe_xds_tests PRIVATE pugixml::pugixml)
        elseif(TARGET pugixml::static)
            target_link_libraries(pacs_ihe_xds_tests PRIVATE pugixml::static)
        elseif(TARGET pugixml::shared)
            target_link_libraries(pacs_ihe_xds_tests PRIVATE pugixml::shared)
        elseif(TARGET pugixml)
            target_link_libraries(pacs_ihe_xds_tests PRIVATE pugixml)
        endif()
        message(STATUS "  [OK] pacs_ihe_xds_tests: ON (ITI-41)")
    endif()

    # DI tests (Issue #312 - ServiceContainer based DI Integration)
    # (Issue #309 - Full Adoption of ILogger Interface)
    if(TARGET pacs_integration AND TARGET pacs_storage)
        add_executable(di_tests
            tests/di/service_registration_test.cpp
            tests/di/ilogger_test.cpp
        )
        target_link_libraries(di_tests
            PRIVATE
                pacs_core
                pacs_storage
                pacs_network
                pacs_integration
                pacs_encoding
                pacs_services
                Catch2::Catch2WithMain
        )
        if(TARGET thread_system_lib)
            target_link_libraries(di_tests PRIVATE thread_system_lib)
        endif()
        if(TARGET common_system_lib)
            target_link_libraries(di_tests PRIVATE common_system_lib)
        endif()
        # Link integrated_database and monitoring_system for database dependencies (Issue #642)
        pacs_link_database_deps(di_tests)
    endif()

    # Client tests (Issue #554 - Job Manager, Issue #539 - Routing Manager, Issue #541 - Prefetch Manager, Issue #542 - Sync Manager)
    if(TARGET pacs_client AND TARGET pacs_storage)
        add_executable(client_tests
            tests/client/job_manager_test.cpp
            tests/client/routing_manager_test.cpp
            tests/client/prefetch_manager_test.cpp
            tests/client/sync_manager_test.cpp
        )
        target_link_libraries(client_tests
            PRIVATE
                pacs_client
                pacs_core
                pacs_storage
                pacs_network
                pacs_services
                Catch2::Catch2WithMain
        )
        if(TARGET pacs_integration)
            target_link_libraries(client_tests PRIVATE pacs_integration)
        endif()
        if(TARGET thread_system_lib)
            target_link_libraries(client_tests PRIVATE thread_system_lib)
        endif()
        if(TARGET common_system_lib)
            target_link_libraries(client_tests PRIVATE common_system_lib)
        endif()
        # Link integrated_database and monitoring_system for database dependencies (Issue #642)
        pacs_link_database_deps(client_tests)
    endif()

    # Concurrency tests (Issue #337 - Lock-free structure stress tests)
    if(TARGET thread_base OR TARGET thread_system)
        add_executable(concurrency_tests
            tests/concurrency/lockfree_stress_test.cpp
        )
        target_link_libraries(concurrency_tests
            PRIVATE
                Catch2::Catch2WithMain
        )
        # Link thread_system for lockfree_queue
        if(TARGET thread_system)
            target_link_libraries(concurrency_tests PRIVATE thread_system)
        elseif(TARGET thread_base)
            target_link_libraries(concurrency_tests PRIVATE thread_base)
        endif()
        # Add thread_system include directory explicitly (submodule mode only)
        if(PACS_THREAD_SYSTEM_INCLUDE_DIR AND NOT PACS_THREAD_SYSTEM_INCLUDE_DIR STREQUAL "FOUND_VIA_PACKAGE")
            target_include_directories(concurrency_tests PRIVATE ${PACS_THREAD_SYSTEM_INCLUDE_DIR})
        endif()
        # Suppress deprecated warnings
        target_compile_options(concurrency_tests PRIVATE
            $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-deprecated-declarations>
            $<$<CXX_COMPILER_ID:MSVC>:/wd4996>
        )
    endif()

    include(CTest)
    include(Catch)
    find_package(Python3 COMPONENTS Interpreter QUIET)

    ##################################################
    # CTest Labels for CI/CD Integration
    # @see Issue #139 - CI/CD Integration
    #
    # Labels:
    #   - unit: Fast unit tests (timeout: 60s)
    #   - integration: Integration tests (timeout: 300s)
    #   - stress: Stress/performance tests (timeout: 600s)
    #   - slow: Long-running tests (manual trigger only)
    ##################################################

    # Unit tests - fast, isolated component tests
    catch_discover_tests(core_tests
        PROPERTIES LABELS "unit"
    )
    catch_discover_tests(utilities_tests
        PROPERTIES LABELS "unit"
    )
    catch_discover_tests(encoding_tests
        PROPERTIES LABELS "unit"
    )
    catch_discover_tests(network_tests
        PROPERTIES LABELS "unit" SKIP_RETURN_CODE 4
    )
    catch_discover_tests(services_tests
        PROPERTIES LABELS "unit"
    )
    if(TARGET storage_tests)
        catch_discover_tests(storage_tests
            PROPERTIES LABELS "unit"
        )
    endif()
    if(TARGET ai_tests)
        catch_discover_tests(ai_tests
            PROPERTIES LABELS "unit"
        )
    endif()
    if(TARGET workflow_tests)
        catch_discover_tests(workflow_tests
            PROPERTIES LABELS "unit"
        )
    endif()
    if(TARGET monitoring_tests)
        catch_discover_tests(monitoring_tests
            PROPERTIES LABELS "unit"
        )
    endif()
    if(TARGET web_tests)
        catch_discover_tests(web_tests
            PROPERTIES LABELS "unit"
        )
    endif()

    if(TARGET security_tests)
        catch_discover_tests(security_tests
            PROPERTIES LABELS "unit"
        )
    endif()

    if(TARGET di_tests)
        catch_discover_tests(di_tests
            PROPERTIES LABELS "unit"
        )
    endif()

    if(TARGET client_tests)
        catch_discover_tests(client_tests
            PROPERTIES LABELS "unit"
        )
    endif()

    # Concurrency stress tests (Issue #337 - Lock-free structure stress tests)
    # TEST_PREFIX ensures these tests are excluded from unit test runs
    # Label "stress" allows separate CI runs for long-running tests
    if(TARGET concurrency_tests)
        catch_discover_tests(concurrency_tests
            TEST_PREFIX "stress::"
            PROPERTIES LABELS "stress"
        )
    endif()

    # Integration tests - cross-module adapter tests
    # TEST_PREFIX ensures these tests are excluded from unit test runs
    # (CI uses --exclude-regex "integration::" for unit tests)
    if(TARGET pacs_integration_tests)
        catch_discover_tests(pacs_integration_tests
            TEST_PREFIX "integration::"
            PROPERTIES LABELS "integration"
        )
    endif()

    # XDS-I.b Gazelle-style registry integration test (Issue #1115).
    # Uses the same "integration::" TEST_PREFIX so the test is excluded
    # from unit-test runs but still discovered by CI integration labels.
    if(TARGET xds_gazelle_integration_tests)
        catch_discover_tests(xds_gazelle_integration_tests
            TEST_PREFIX "integration::"
            PROPERTIES LABELS "integration"
        )
    endif()

    # IHE XDS.b ITI-41 unit tests (Issue #1128).
    if(TARGET pacs_ihe_xds_tests)
        catch_discover_tests(pacs_ihe_xds_tests
            PROPERTIES LABELS "unit"
        )
    endif()

    if(Python3_Interpreter_FOUND)
        add_test(
            NAME storage_boundary_guard
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/storage/check_storage_boundary.py
                    ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(storage_boundary_guard PROPERTIES
            LABELS "unit;boundary"
            TIMEOUT 10
        )
    endif()

    ##################################################
    # C++20 Module Tests (Issue #507)
    # Tests module import functionality when PACS_BUILD_MODULES=ON
    ##################################################
    if(PACS_BUILD_MODULES AND TARGET pacs_system_modules)
        file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/modules)
        add_executable(module_tests
            tests/modules/module_import_test.cpp
        )
        target_link_libraries(module_tests
            PRIVATE
                pacs_system_modules
                Catch2::Catch2WithMain
        )

        # Enable module scanning for test target
        set_target_properties(module_tests PROPERTIES
            CXX_SCAN_FOR_MODULES ON
        )

        # Register module tests with CTest
        catch_discover_tests(module_tests
            TEST_PREFIX "modules::"
            PROPERTIES LABELS "modules"
        )
        message(STATUS "  [OK] module_tests: C++20 module import tests")
    endif()
endif()
