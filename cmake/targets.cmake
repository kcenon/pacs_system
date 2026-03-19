##################################################
# PACS Libraries
##################################################

# Core library
add_library(pacs_core
    src/core/dicom_tag.cpp
    src/core/dicom_element.cpp
    src/core/dicom_dataset.cpp
    src/core/dicom_file.cpp
    src/core/tag_info.cpp
    src/core/dicom_dictionary.cpp
    src/core/standard_tags_data.cpp
    src/core/pool_manager.cpp
    src/core/private_tag_registry.cpp
)
target_include_directories(pacs_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link common_system (Tier 0) - Result<T> pattern
if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
    pacs_link_external_dependency(
        pacs_core
        PUBLIC
        pacs_common_system_headers
        kcenon::common_system
    )
    # Use KCENON_HAS_COMMON_SYSTEM for unified feature flag naming
    # Keep PACS_WITH_COMMON_SYSTEM as legacy alias for migration window
    target_compile_definitions(pacs_core PUBLIC
        KCENON_HAS_COMMON_SYSTEM=1
        PACS_WITH_COMMON_SYSTEM=1
    )
endif()

# Link container_system (Tier 1) - DICOM serialization
if(TARGET container_system)
    pacs_link_external_dependency(
        pacs_core
        PUBLIC
        container_system
        ContainerSystem::container
    )
    target_compile_definitions(pacs_core PUBLIC PACS_WITH_CONTAINER_SYSTEM)
endif()

# Encoding library
add_library(pacs_encoding
    src/encoding/transfer_syntax.cpp
    src/encoding/vr_info.cpp
    src/encoding/implicit_vr_codec.cpp
    src/encoding/explicit_vr_codec.cpp
    src/encoding/explicit_vr_big_endian_codec.cpp
    src/encoding/character_set.cpp
    src/encoding/dataset_charset.cpp
    src/encoding/compression/jpeg_baseline_codec.cpp
    src/encoding/compression/jpeg_lossless_codec.cpp
    src/encoding/compression/frame_deflate_codec.cpp
    src/encoding/compression/hevc_codec.cpp
    src/encoding/compression/htj2k_codec.cpp
    src/encoding/compression/jpeg2000_codec.cpp
    src/encoding/compression/jpeg_ls_codec.cpp
    src/encoding/compression/jpegxl_codec.cpp
    src/encoding/compression/rle_codec.cpp
    src/encoding/compression/codec_factory.cpp
)
target_include_directories(pacs_encoding
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(pacs_encoding
    PUBLIC pacs_core
)

# Link iconv for character set conversion (required on macOS, optional on Linux)
# LGPL-2.1 compliance: iconv MUST be dynamically linked (see docs/SOUP.md SOUP-018)
if(NOT WIN32)
    # Prefer shared library (.dylib/.so) over static (.a) for LGPL compliance
    find_library(ICONV_LIBRARY
        NAMES iconv libiconv
        # CMAKE_FIND_LIBRARY_SUFFIXES defaults prefer shared on most platforms
    )
    if(ICONV_LIBRARY)
        # Verify dynamic linking: warn if static library detected
        get_filename_component(_iconv_ext "${ICONV_LIBRARY}" EXT)
        if(_iconv_ext STREQUAL ".a")
            message(WARNING
                "iconv appears to be statically linked (${ICONV_LIBRARY}).\n"
                "iconv is LGPL-2.1 licensed and MUST be dynamically linked.\n"
                "Please install the shared library (libiconv.so / libiconv.dylib).")
        endif()
        target_link_libraries(pacs_encoding PUBLIC ${ICONV_LIBRARY})
        message(STATUS "  [OK] iconv linked: ${ICONV_LIBRARY}")
    endif()
endif()

# Link libjpeg-turbo for JPEG compression codecs
if(PACS_JPEG_FOUND)
    target_link_libraries(pacs_encoding PUBLIC ${PACS_JPEG_TARGET})
    if(PACS_JPEG_INCLUDE_DIR)
        pacs_add_build_interface_include_dirs(
            pacs_encoding
            PUBLIC
            ${PACS_JPEG_INCLUDE_DIR}
        )
    endif()
    target_compile_definitions(pacs_encoding PUBLIC PACS_WITH_JPEG_CODEC)
    message(STATUS "  [OK] pacs_encoding: JPEG codec enabled")
endif()

# Link OpenJPEG for JPEG 2000 compression codec
if(PACS_JPEG2000_FOUND)
    if(TARGET OpenJPEG::OpenJPEG)
        target_link_libraries(pacs_encoding PUBLIC OpenJPEG::OpenJPEG)
    elseif(TARGET openjp2)
        pacs_link_external_dependency(
            pacs_encoding
            PUBLIC
            openjp2
            openjp2
        )
    elseif(OPENJPEG_LIBRARIES)
        target_link_libraries(pacs_encoding PUBLIC ${OPENJPEG_LIBRARIES})
        if(OPENJPEG_INCLUDE_DIRS)
            pacs_add_build_interface_include_dirs(
                pacs_encoding
                PUBLIC
                ${OPENJPEG_INCLUDE_DIRS}
            )
        endif()
    endif()
    target_compile_definitions(pacs_encoding PUBLIC PACS_WITH_JPEG2000_CODEC)
    message(STATUS "  [OK] pacs_encoding: JPEG 2000 codec enabled")
endif()

# Link CharLS for JPEG-LS compression codec
if(PACS_JPEGLS_FOUND)
    if(TARGET charls)
        pacs_link_external_dependency(
            pacs_encoding
            PUBLIC
            charls
            charls
        )
    elseif(TARGET charls::charls)
        target_link_libraries(pacs_encoding PUBLIC charls::charls)
    elseif(CHARLS_LIBRARIES)
        target_link_libraries(pacs_encoding PUBLIC ${CHARLS_LIBRARIES})
        if(CHARLS_INCLUDE_DIRS)
            pacs_add_build_interface_include_dirs(
                pacs_encoding
                PUBLIC
                ${CHARLS_INCLUDE_DIRS}
            )
        endif()
    endif()
    target_compile_definitions(pacs_encoding PUBLIC PACS_WITH_JPEGLS_CODEC)
    message(STATUS "  [OK] pacs_encoding: JPEG-LS codec enabled")
endif()

# Link OpenJPH for HTJ2K compression codec
if(PACS_HTJ2K_FOUND)
    if(TARGET openjphstatic)
        pacs_link_external_dependency(
            pacs_encoding
            PUBLIC
            openjphstatic
            openjph
        )
    elseif(TARGET openjph::openjph)
        target_link_libraries(pacs_encoding PUBLIC openjph::openjph)
    elseif(TARGET openjph)
        pacs_link_external_dependency(
            pacs_encoding
            PUBLIC
            openjph
            openjph
        )
    elseif(OPENJPH_LIBRARIES)
        target_link_libraries(pacs_encoding PUBLIC ${OPENJPH_LIBRARIES})
        if(OPENJPH_INCLUDE_DIRS)
            pacs_add_build_interface_include_dirs(
                pacs_encoding
                PUBLIC
                ${OPENJPH_INCLUDE_DIRS}
            )
        endif()
    endif()
    # FetchContent compatibility: add include bridge for openjph/ prefix
    if(DEFINED _openjph_compat_include)
        pacs_add_build_interface_include_dirs(
            pacs_encoding
            PUBLIC
            "${_openjph_compat_include}"
        )
    endif()
    target_compile_definitions(pacs_encoding PUBLIC PACS_WITH_HTJ2K_CODEC)
    message(STATUS "  [OK] pacs_encoding: HTJ2K codec enabled")
endif()

# Network library
add_library(pacs_network
    src/network/pdu_encoder.cpp
    src/network/pdu_decoder.cpp
    src/network/pdu_buffer_pool.cpp
    src/network/association.cpp
    src/network/dicom_server.cpp
    src/network/dimse/dimse_message.cpp
    src/network/detail/accept_worker.cpp
    src/network/v2/dicom_association_handler.cpp
    src/network/v2/dicom_server_v2.cpp
    # Pipeline infrastructure (Issue #517)
    src/network/pipeline/pipeline_coordinator.cpp
    # Pipeline jobs (Issue #517, #519-#522)
    src/network/pipeline/jobs/receive_network_io_job.cpp
    src/network/pipeline/jobs/send_network_io_job.cpp
    src/network/pipeline/jobs/pdu_decode_job.cpp
    src/network/pipeline/jobs/dimse_process_job.cpp
    src/network/pipeline/jobs/storage_query_exec_job.cpp
    src/network/pipeline/jobs/response_encode_job.cpp
    # Pipeline adapter (Issue #523)
    src/network/pipeline/pipeline_adapter.cpp
)
target_include_directories(pacs_network
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
# Suppress deprecated warnings from common_system/logger_system headers (logger_interface is deprecated)
target_compile_options(pacs_network PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-deprecated-declarations>
    $<$<CXX_COMPILER_ID:MSVC>:/wd4996>
)
target_link_libraries(pacs_network
    PUBLIC pacs_core pacs_encoding pacs_security
)

# Link network_system (Tier 4) - DICOM PDU/Association
if(TARGET NetworkSystem)
    pacs_link_external_dependency(
        pacs_network
        PUBLIC
        NetworkSystem
        NetworkSystem::NetworkSystem
    )
    target_compile_definitions(pacs_network PUBLIC PACS_WITH_NETWORK_SYSTEM)
endif()

# Link thread_system (Tier 1) - for accept_worker
if(TARGET ThreadSystem)
    pacs_link_external_dependency(
        pacs_network
        PUBLIC
        ThreadSystem
        ThreadSystem::thread_base
    )
    target_compile_definitions(pacs_network PUBLIC PACS_NETWORK_WITH_THREAD_SYSTEM)
elseif(TARGET thread_base)
    pacs_link_external_dependency(
        pacs_network
        PUBLIC
        thread_base
        ThreadSystem::thread_base
    )
    target_compile_definitions(pacs_network PUBLIC PACS_NETWORK_WITH_THREAD_SYSTEM)
    # CRITICAL: Explicitly add thread_system include directories
    # When thread_base is fetched via FetchContent (by logger_system), its INTERFACE
    # include directories may not be set up correctly. Ensure we can find thread_base.h
    # by explicitly adding the include directory from the sibling checkout.
    if(PACS_THREAD_SYSTEM_INCLUDE_DIR AND NOT PACS_THREAD_SYSTEM_INCLUDE_DIR STREQUAL "FOUND_VIA_PACKAGE")
        pacs_add_build_interface_include_dirs(
            pacs_network
            PUBLIC
            ${PACS_THREAD_SYSTEM_INCLUDE_DIR}
        )
    endif()
endif()

# CRITICAL: Inherit thread_system compile definitions for ABI compatibility
# thread_system uses add_definitions(-DUSE_STD_JTHREAD) which doesn't propagate
# as interface compile definitions. We must manually sync these flags to ensure
# thread_base class layout matches between library and consumer code.
# Without this, derived classes have incorrect member offsets causing crashes.
#
# NOTE: We must detect jthread support ourselves because SET_STD_JTHREAD from
# thread_system doesn't propagate when using FetchContent. AppleClang doesn't
# support std::jthread even in C++20 mode, so we need proper detection.
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
    #include <stop_token>
    #include <thread>
    int main() {
        std::jthread t([](){});
        return 0;
    }
" PACS_HAS_STD_JTHREAD)

if(PACS_HAS_STD_JTHREAD)
    message(STATUS "std::jthread detected - enabling USE_STD_JTHREAD for ABI compatibility")
    target_compile_definitions(pacs_network PUBLIC USE_STD_JTHREAD)
else()
    message(STATUS "std::jthread not available - using std::thread fallback")
endif()

# CRITICAL: Define KCENON_HAS_COMMON_EXECUTOR for thread_system ABI compatibility
# thread_system's thread_pool.h uses conditional compilation:
#   - KCENON_HAS_COMMON_EXECUTOR=1: is_running() declared with 'override' (virtual)
#   - KCENON_HAS_COMMON_EXECUTOR=0: is_running() declared as non-virtual
# When common_system is available, thread_system compiles with KCENON_HAS_COMMON_EXECUTOR=1.
# Consumer code must use the same definition to match the library's function signatures.
if(COMMON_SYSTEM_FOUND)
    message(STATUS "common_system found - enabling KCENON_HAS_COMMON_EXECUTOR for ABI compatibility")
    target_compile_definitions(pacs_network PUBLIC KCENON_HAS_COMMON_EXECUTOR=1)
endif()

# Client library (remote node management for SCU operations)
add_library(pacs_client
    src/client/remote_node_manager.cpp
    src/client/job_manager.cpp
    src/client/routing_manager.cpp
    src/client/prefetch_manager.cpp
    src/client/sync_manager.cpp
)
target_include_directories(pacs_client
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(pacs_client
    PUBLIC pacs_network
)
if(PACS_BUILD_STORAGE AND SQLITE3_FOUND)
    target_link_libraries(pacs_client PUBLIC pacs_storage)
endif()

# Link common_system (Tier 0)
if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
    pacs_link_external_dependency(
        pacs_client
        PUBLIC
        pacs_common_system_headers
        kcenon::common_system
    )
    target_compile_definitions(pacs_client PUBLIC
        KCENON_HAS_COMMON_SYSTEM=1
        PACS_WITH_COMMON_SYSTEM=1
    )
endif()

# Services library
add_library(pacs_services
    src/services/verification_scp.cpp
    src/services/storage_scp.cpp
    src/services/storage_scu.cpp
    src/services/query_scu.cpp
    src/services/retrieve_scu.cpp
    src/services/query_scp.cpp
    src/services/retrieve_scp.cpp
    src/services/worklist_scp.cpp
    src/services/worklist_scu.cpp
    src/services/mpps_scp.cpp
    src/services/mpps_scu.cpp
    src/services/n_get_scp.cpp
    src/services/n_get_scu.cpp
    src/services/storage_commitment_scp.cpp
    src/services/storage_commitment_scu.cpp
    src/services/print_scp.cpp
    src/services/print_scu.cpp
    src/services/ups/ups_push_scp.cpp
    src/services/ups/ups_push_scu.cpp
    src/services/ups/ups_query_scp.cpp
    src/services/ups/ups_watch_scp.cpp
    src/services/ups/ups_watch_scu.cpp
    src/services/sop_class_registry.cpp
    src/services/sop_classes/us_storage.cpp
    src/services/sop_classes/dx_storage.cpp
    src/services/sop_classes/mg_storage.cpp
    src/services/sop_classes/pet_storage.cpp
    src/services/sop_classes/nm_storage.cpp
    src/services/sop_classes/rt_storage.cpp
    src/services/sop_classes/seg_storage.cpp
    src/services/sop_classes/sr_storage.cpp
    src/services/sop_classes/ct_storage.cpp
    src/services/sop_classes/mr_storage.cpp
    src/services/sop_classes/wsi_storage.cpp
    src/services/sop_classes/ophthalmic_storage.cpp
    src/services/sop_classes/parametric_map_storage.cpp
    src/services/sop_classes/waveform_storage.cpp
    src/services/validation/us_iod_validator.cpp
    src/services/validation/dx_iod_validator.cpp
    src/services/validation/mg_iod_validator.cpp
    src/services/validation/pet_iod_validator.cpp
    src/services/validation/nm_iod_validator.cpp
    src/services/validation/rt_iod_validator.cpp
    src/services/validation/seg_iod_validator.cpp
    src/services/validation/sr_iod_validator.cpp
    src/services/validation/ct_iod_validator.cpp
    src/services/validation/mr_iod_validator.cpp
    src/services/validation/wsi_iod_validator.cpp
    src/services/validation/ophthalmic_iod_validator.cpp
    src/services/validation/parametric_map_iod_validator.cpp
    src/services/validation/ct_processing_iod_validator.cpp
    src/services/validation/heightmap_seg_iod_validator.cpp
    src/services/validation/label_map_seg_iod_validator.cpp
    src/services/pir/patient_reconciliation_service.cpp
    src/services/xds/imaging_document_source.cpp
    src/services/xds/imaging_document_consumer.cpp
    src/services/validation/waveform_ps_iod_validator.cpp
    src/services/cache/query_cache.cpp
    src/services/cache/database_cursor.cpp
    src/services/cache/query_result_stream.cpp
    src/services/cache/streaming_query_handler.cpp
    src/services/cache/parallel_query_executor.cpp
)

# Add monitoring sources conditionally (use TARGET check, not CMake variable)
if(TARGET database)
    target_sources(pacs_services PRIVATE
        src/services/monitoring/database_metrics_service.cpp
    )
endif()

target_include_directories(pacs_services
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(pacs_services
    PUBLIC pacs_network
)

# Link common_system for Result<T>
if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
    pacs_link_external_dependency(
        pacs_services
        PUBLIC
        pacs_common_system_headers
        kcenon::common_system
    )
    # Use KCENON_HAS_COMMON_SYSTEM for unified feature flag naming
    # Keep PACS_WITH_COMMON_SYSTEM as legacy alias for migration window
    target_compile_definitions(pacs_services PUBLIC
        KCENON_HAS_COMMON_SYSTEM=1
        PACS_WITH_COMMON_SYSTEM=1
    )
endif()

# Link database_system for database_cursor query building (Issue #420)
if(TARGET database)
    pacs_link_external_dependency(
        pacs_services
        PUBLIC
        database
        DatabaseSystem::database
    )
    target_compile_definitions(pacs_services PUBLIC PACS_WITH_DATABASE_SYSTEM=1)
    # Suppress deprecated warnings from database_system (database_base is deprecated)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(pacs_services PRIVATE -Wno-deprecated-declarations)
    endif()
    message(STATUS "    database_system: linked for pacs_services (Issue #420)")
endif()

message(STATUS "  [OK] pacs_services: ON")

# Link pacs_services to pacs_client (job_manager uses SCU classes)
# This must be done after pacs_services is defined
target_link_libraries(pacs_client PUBLIC pacs_services)

# Security library
# Base security sources (always included)
set(PACS_SECURITY_SOURCES
    src/security/access_control_manager.cpp
    src/security/tag_action.cpp
    src/security/uid_mapping.cpp
    src/security/anonymizer.cpp
    src/security/atna_audit_logger.cpp
    src/security/atna_syslog_transport.cpp
    src/security/atna_service_auditor.cpp
    src/security/atna_config.cpp
    src/security/tls_policy.cpp
)

# Digital signature sources (requires OpenSSL - Issue #191)
if(PACS_OPENSSL_FOUND)
    list(APPEND PACS_SECURITY_SOURCES
        src/security/certificate.cpp
        src/security/digital_signature.cpp
    )
endif()

add_library(pacs_security ${PACS_SECURITY_SOURCES})
target_include_directories(pacs_security
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link pacs_core for dicom_dataset
target_link_libraries(pacs_security PUBLIC pacs_core pacs_encoding)

# Link common_system (Tier 0)
if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
    pacs_link_external_dependency(
        pacs_security
        PUBLIC
        pacs_common_system_headers
        kcenon::common_system
    )
    # Use KCENON_HAS_COMMON_SYSTEM for unified feature flag naming
    # Keep PACS_WITH_COMMON_SYSTEM as legacy alias for migration window
    target_compile_definitions(pacs_security PUBLIC
        KCENON_HAS_COMMON_SYSTEM=1
        PACS_WITH_COMMON_SYSTEM=1
    )
endif()

# Link OpenSSL for digital signatures (Issue #191)
if(PACS_OPENSSL_FOUND)
    target_link_libraries(pacs_security PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(pacs_security PUBLIC PACS_WITH_DIGITAL_SIGNATURES)
    message(STATUS "  [OK] pacs_security: ON (with digital signatures)")
else()
    message(STATUS "  [OK] pacs_security: ON (without digital signatures)")
endif()

# Storage library
if(PACS_BUILD_STORAGE AND SQLITE3_FOUND)
    add_library(pacs_storage
        src/storage/storage_interface.cpp
        src/storage/file_storage.cpp
        src/storage/s3_storage.cpp
        src/storage/azure_blob_storage.cpp
        src/storage/hsm_storage.cpp
        src/storage/hsm_migration_service.cpp
        src/storage/sqlite_security_storage.cpp
        src/storage/migration_runner.cpp
        src/storage/index_database.cpp
        src/storage/node_repository.cpp
        src/storage/job_repository.cpp
        src/storage/routing_repository.cpp
        src/storage/prefetch_repository.cpp
        src/storage/sync_repository.cpp
        src/storage/annotation_repository.cpp
        src/storage/patient_repository.cpp
        src/storage/study_repository.cpp
        src/storage/series_repository.cpp
        src/storage/instance_repository.cpp
        src/storage/mpps_repository.cpp
        src/storage/worklist_repository.cpp
        src/storage/ups_repository.cpp
        src/storage/audit_repository.cpp
        src/storage/measurement_repository.cpp
        src/storage/key_image_repository.cpp
        src/storage/viewer_state_repository.cpp
        src/storage/viewer_state_record_repository.cpp
        src/storage/recent_study_repository.cpp
        src/storage/sync_config_repository.cpp
        src/storage/sync_conflict_repository.cpp
        src/storage/sync_history_repository.cpp
        src/storage/prefetch_rule_repository.cpp
        src/storage/prefetch_history_repository.cpp
        src/storage/pacs_database_adapter.cpp
        src/storage/repository_factory.cpp
        src/storage/commitment_repository.cpp
    )
    target_include_directories(pacs_storage
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    # Link common_system for Result<T>
    if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
        pacs_link_external_dependency(
            pacs_storage
            PUBLIC
            pacs_common_system_headers
            kcenon::common_system
        )
        # Use KCENON_HAS_COMMON_SYSTEM for unified feature flag naming
        # Keep PACS_WITH_COMMON_SYSTEM as legacy alias for migration window
        target_compile_definitions(pacs_storage PUBLIC
            KCENON_HAS_COMMON_SYSTEM=1
            PACS_WITH_COMMON_SYSTEM=1
        )
    endif()

    # Link SQLite3
    if(SQLITE3_FROM_FETCH)
        pacs_link_external_dependency(
            pacs_storage
            PUBLIC
            sqlite3_lib
            SQLite::SQLite3
        )
    else()
        target_link_libraries(pacs_storage PUBLIC SQLite::SQLite3)
    endif()

    # Link pacs_core for dicom_dataset and pacs_encoding for transfer_syntax
    target_link_libraries(pacs_storage PUBLIC pacs_core pacs_encoding pacs_security)

    # Link database_system for secure query building (SQL injection prevention)
    if(TARGET database)
        pacs_link_external_dependency(
            pacs_storage
            PUBLIC
            database
            DatabaseSystem::database
        )
        # Link integrated_database for unified_database_system (Issue #606)
        # integrated_database depends on monitoring_system, so both must be linked together.
        # On Linux, use --start-group/--end-group to resolve circular dependencies.
        if(TARGET integrated_database AND TARGET monitoring_system)
            if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
                target_link_libraries(pacs_storage PUBLIC
                    "$<BUILD_LOCAL_INTERFACE:-Wl,--start-group>"
                    "$<BUILD_LOCAL_INTERFACE:integrated_database>"
                    "$<BUILD_LOCAL_INTERFACE:monitoring_system>"
                    "$<BUILD_LOCAL_INTERFACE:-Wl,--end-group>"
                    "$<INSTALL_INTERFACE:-Wl,--start-group>"
                    "$<INSTALL_INTERFACE:DatabaseSystem::integrated_database>"
                    "$<INSTALL_INTERFACE:monitoring_system::monitoring_system>"
                    "$<INSTALL_INTERFACE:-Wl,--end-group>"
                )
            else()
                target_link_libraries(pacs_storage PUBLIC
                    "$<BUILD_LOCAL_INTERFACE:integrated_database>"
                    "$<BUILD_LOCAL_INTERFACE:monitoring_system>"
                    "$<INSTALL_INTERFACE:DatabaseSystem::integrated_database>"
                    "$<INSTALL_INTERFACE:monitoring_system::monitoring_system>"
                )
            endif()
        elseif(TARGET integrated_database)
            pacs_link_external_dependency(
                pacs_storage
                PUBLIC
                integrated_database
                DatabaseSystem::integrated_database
            )
        endif()
        target_compile_definitions(pacs_storage PUBLIC PACS_WITH_DATABASE_SYSTEM=1)
        # Suppress deprecated warnings from database_system (database_base is deprecated)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(pacs_storage PRIVATE -Wno-deprecated-declarations)
        endif()
        message(STATUS "    database_system: linked (SQL injection protection enabled)")
    endif()

    # AWS SDK integration for S3 storage (Issue #724)
    if(PACS_WITH_AWS_SDK)
        find_package(AWSSDK COMPONENTS s3)
        if(AWSSDK_FOUND)
            target_link_libraries(pacs_storage PUBLIC ${AWSSDK_LINK_LIBRARIES})
            target_compile_definitions(pacs_storage PUBLIC PACS_WITH_AWS_SDK)
            message(STATUS "    AWS SDK: linked (S3 storage enabled)")
        else()
            message(WARNING "PACS_WITH_AWS_SDK is ON but AWS SDK not found. Using mock S3 client.")
        endif()
    endif()

    # Azure SDK integration for Blob storage (Issue #725)
    if(PACS_WITH_AZURE_SDK)
        find_package(azure-storage-blobs-cpp CONFIG)
        if(azure-storage-blobs-cpp_FOUND)
            target_link_libraries(pacs_storage PUBLIC Azure::azure-storage-blobs)
            target_compile_definitions(pacs_storage PUBLIC PACS_WITH_AZURE_SDK)
            message(STATUS "    Azure SDK: linked (Blob storage enabled)")
        else()
            message(WARNING "PACS_WITH_AZURE_SDK is ON but Azure SDK not found. Using mock Azure client.")
        endif()
    endif()

    message(STATUS "  [OK] pacs_storage: ON (SQLite3)")
else()
    message(STATUS "  [--] pacs_storage: OFF")
endif()

# AI library (AI result handler for SR/SEG/PR objects - Issue #204,
#             AI service connector for inference requests - Issue #205)
# Requires pacs_storage for storing AI-generated DICOM objects
# Requires pacs_integration for logger/monitoring adapters
if(TARGET pacs_storage)
    add_library(pacs_ai
        src/ai/ai_result_handler.cpp
        src/ai/ai_service_connector.cpp
        src/ai/aira_assessment.cpp
        src/ai/aira_assessment_manager.cpp
    )
    target_include_directories(pacs_ai
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(pacs_ai
        PUBLIC pacs_core pacs_storage
    )

    # Link common_system for Result<T>
    if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
        pacs_link_external_dependency(
            pacs_ai
            PUBLIC
            pacs_common_system_headers
            kcenon::common_system
        )
        # Use KCENON_HAS_COMMON_SYSTEM for unified feature flag naming
        # Keep PACS_WITH_COMMON_SYSTEM as legacy alias for migration window
        target_compile_definitions(pacs_ai PUBLIC
            KCENON_HAS_COMMON_SYSTEM=1
            PACS_WITH_COMMON_SYSTEM=1
        )
    endif()

    # Note: pacs_integration is linked later after it's defined (for ai_service_connector)
    # This is handled in the pacs_integration section below

    message(STATUS "  [OK] pacs_ai: ON (AI result handler, AI service connector)")
else()
    message(STATUS "  [--] pacs_ai: OFF (requires pacs_storage)")
endif()

# Monitoring library (health check endpoint - Issue #211, metrics - Issue #210)
# Requires pacs_storage for file_storage and index_database checks
if(TARGET pacs_storage)
    add_library(pacs_monitoring
        src/monitoring/health_checker.cpp
        src/monitoring/pacs_metrics.cpp
    )
    target_include_directories(pacs_monitoring
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(pacs_monitoring
        PUBLIC pacs_storage
    )

    message(STATUS "  [OK] pacs_monitoring: ON (health check)")
else()
    message(STATUS "  [--] pacs_monitoring: OFF (requires pacs_storage)")
endif()

# Workflow library (auto prefetch service - Issue #206, task scheduler - Issue #207)
# Requires pacs_storage for index_database and pacs_integration for adapters
if(TARGET pacs_storage)
    add_library(pacs_workflow
        src/workflow/auto_prefetch_service.cpp
        src/workflow/task_scheduler.cpp
        src/workflow/study_lock_manager.cpp
    )
    target_include_directories(pacs_workflow
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(pacs_workflow
        PUBLIC pacs_storage pacs_services
    )

    # Link common_system for Result<T>
    if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
        pacs_link_external_dependency(
            pacs_workflow
            PUBLIC
            pacs_common_system_headers
            kcenon::common_system
        )
        # Use KCENON_HAS_COMMON_SYSTEM for unified feature flag naming
        # Keep PACS_WITH_COMMON_SYSTEM as legacy alias for migration window
        target_compile_definitions(pacs_workflow PUBLIC
            KCENON_HAS_COMMON_SYSTEM=1
            PACS_WITH_COMMON_SYSTEM=1
        )
    endif()

    message(STATUS "  [OK] pacs_workflow: ON (auto prefetch service)")
else()
    message(STATUS "  [--] pacs_workflow: OFF (requires pacs_storage)")
endif()

# Web library (REST API server - Issue #194)
# Requires Crow framework (fetched above)
if(TARGET Crow::Crow)
    add_library(pacs_web
        src/web/rest_server.cpp
        src/web/thumbnail_service.cpp
        src/web/metadata_service.cpp
        src/web/endpoints/system_endpoints.cpp
        src/web/endpoints/security_endpoints.cpp
        src/web/endpoints/patient_endpoints.cpp
        src/web/endpoints/study_endpoints.cpp
        src/web/endpoints/series_endpoints.cpp
        src/web/endpoints/worklist_endpoints.cpp
        src/web/endpoints/audit_endpoints.cpp
        src/web/endpoints/association_endpoints.cpp
        src/web/endpoints/dicomweb_endpoints.cpp
        src/web/endpoints/remote_nodes_endpoints.cpp
        src/web/endpoints/jobs_endpoints.cpp
        src/web/endpoints/routing_endpoints.cpp
        src/web/endpoints/thumbnail_endpoints.cpp
        src/web/endpoints/metadata_endpoints.cpp
        src/web/endpoints/annotation_endpoints.cpp
        src/web/endpoints/measurement_endpoints.cpp
        src/web/endpoints/key_image_endpoints.cpp
        src/web/endpoints/viewer_state_endpoints.cpp
        src/web/endpoints/wado_uri_endpoints.cpp
        src/web/endpoints/storage_commitment_endpoints.cpp
        src/web/auth/jwt_validator.cpp
        src/web/auth/jwks_provider.cpp
        src/web/auth/oauth2_middleware.cpp
    )

    # Add metrics endpoints conditionally (use TARGET check, not CMake variable)
    if(TARGET database)
        target_sources(pacs_web PRIVATE
            src/web/endpoints/metrics_endpoints.cpp
        )
    endif()

    target_include_directories(pacs_web
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(pacs_web
        PUBLIC pacs_core pacs_storage pacs_client
    )
    pacs_link_external_dependency(
        pacs_web
        PUBLIC
        Crow::Crow
        Crow::Crow
    )

    # Link pacs_monitoring if available
    if(TARGET pacs_monitoring)
        target_link_libraries(pacs_web PUBLIC pacs_monitoring)
        target_compile_definitions(pacs_web PUBLIC PACS_WITH_MONITORING)
    endif()

    # Link OpenSSL for JWT signature verification (Issue #860)
    if(PACS_OPENSSL_FOUND)
        target_link_libraries(pacs_web PUBLIC OpenSSL::SSL OpenSSL::Crypto)
        target_compile_definitions(pacs_web PUBLIC PACS_WITH_DIGITAL_SIGNATURES)
    endif()

    message(STATUS "  [OK] pacs_web: ON (REST API server)")
else()
    message(STATUS "  [--] pacs_web: OFF (requires Crow)")
endif()

# Integration library (requires container_system and logger_system via network_system)
if(TARGET container_system AND COMMON_SYSTEM_FOUND AND TARGET NetworkSystem)
    # Define integration library sources
    set(PACS_INTEGRATION_SOURCES
        src/integration/container_adapter.cpp
        src/integration/logger_adapter.cpp
        src/integration/thread_adapter.cpp
        src/integration/thread_pool_adapter.cpp
        src/integration/executor_adapter.cpp
        src/integration/network_adapter.cpp
        src/integration/dicom_session.cpp
    )

    # Include monitoring_adapter.cpp when monitoring_system is available (local or vcpkg)
    if(MONITORING_SYSTEM_FOUND OR TARGET monitoring_system)
        list(APPEND PACS_INTEGRATION_SOURCES src/integration/monitoring_adapter.cpp)
    endif()

    add_library(pacs_integration ${PACS_INTEGRATION_SOURCES})
    target_include_directories(pacs_integration
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    # Suppress deprecated warnings from thread_system headers (logger_interface is deprecated)
    target_compile_options(pacs_integration PRIVATE
        $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-deprecated-declarations>
        $<$<CXX_COMPILER_ID:MSVC>:/wd4996>
    )
    target_link_libraries(pacs_integration
        PUBLIC pacs_core pacs_encoding pacs_network
    )
    pacs_link_external_dependency(
        pacs_integration
        PUBLIC
        container_system
        ContainerSystem::container
    )

    # Link logger_system (REQUIRED for logger_adapter)
    if(TARGET LoggerSystem)
        pacs_link_external_dependency(
            pacs_integration
            PUBLIC
            LoggerSystem
            LoggerSystem::LoggerSystem
        )
        target_compile_definitions(pacs_integration PUBLIC PACS_WITH_LOGGER_SYSTEM)
        message(STATUS "    - logger_adapter: ON (LoggerSystem)")
    else()
        message(WARNING "    - logger_adapter: OFF (LoggerSystem target not found)")
    endif()

    # Link thread_system (REQUIRED for thread_adapter)
    # thread_system creates 'thread_base' as main target (legacy build) or 'ThreadSystem' (new build)
    if(TARGET ThreadSystem)
        pacs_link_external_dependency(
            pacs_integration
            PUBLIC
            ThreadSystem
            ThreadSystem::thread_base
        )
        target_compile_definitions(pacs_integration PUBLIC PACS_WITH_THREAD_SYSTEM)
        message(STATUS "    - thread_adapter: ON (ThreadSystem)")
    elseif(TARGET thread_base)
        pacs_link_external_dependency(
            pacs_integration
            PUBLIC
            thread_base
            ThreadSystem::thread_base
        )
        target_compile_definitions(pacs_integration PUBLIC PACS_WITH_THREAD_SYSTEM)
        message(STATUS "    - thread_adapter: ON (thread_base)")
    elseif(PACS_THREAD_SYSTEM_INCLUDE_DIR)
        # Fallback: include headers only (will fail at link time)
        pacs_link_external_dependency(
            pacs_integration
            PUBLIC
            pacs_thread_system_headers
            ThreadSystem::thread_base
        )
        target_compile_definitions(pacs_integration PUBLIC PACS_WITH_THREAD_SYSTEM)
        message(WARNING "    - thread_adapter: ON (headers only - linking may fail)")
    else()
        message(WARNING "    - thread_adapter: OFF (ThreadSystem/thread_base target not found)")
    endif()

    # CRITICAL: Inherit thread_system compile definitions for ABI compatibility
    # Use the same PACS_HAS_STD_JTHREAD detection result from pacs_network
    if(PACS_HAS_STD_JTHREAD)
        target_compile_definitions(pacs_integration PUBLIC USE_STD_JTHREAD)
    endif()

    # Link common_system for Result<T>
    if(PACS_COMMON_SYSTEM_INCLUDE_DIR)
        pacs_link_external_dependency(
            pacs_integration
            PUBLIC
            pacs_common_system_headers
            kcenon::common_system
        )
        # Use KCENON_HAS_COMMON_SYSTEM for unified feature flag naming
        # Keep PACS_WITH_COMMON_SYSTEM as legacy alias for migration window
        # KCENON_HAS_COMMON_EXECUTOR is required for thread_system ABI compatibility
        target_compile_definitions(pacs_integration PUBLIC
            KCENON_HAS_COMMON_SYSTEM=1
            PACS_WITH_COMMON_SYSTEM=1
            KCENON_HAS_COMMON_EXECUTOR=1
        )
    endif()

    # Link network_system (REQUIRED for network_adapter)
    if(TARGET NetworkSystem)
        pacs_link_external_dependency(
            pacs_integration
            PUBLIC
            NetworkSystem
            NetworkSystem::NetworkSystem
        )
        target_compile_definitions(pacs_integration PUBLIC PACS_WITH_NETWORK_SYSTEM)
        message(STATUS "    - network_adapter: ON (NetworkSystem)")
    else()
        message(WARNING "    - network_adapter: OFF (NetworkSystem target not found)")
    endif()

    # Link monitoring_system (for monitoring_adapter)
    # NOTE: monitoring_system subdirectory is added earlier (before pacs_storage)
    #       to ensure TARGET monitoring_system exists when pacs_storage links it.
    if(TARGET monitoring_system)
        pacs_link_external_dependency(
            pacs_integration
            PUBLIC
            monitoring_system
            monitoring_system::monitoring_system
        )
        target_compile_definitions(pacs_integration PUBLIC PACS_WITH_MONITORING_SYSTEM)
        message(STATUS "    - monitoring_adapter: ON (monitoring_system)")
    elseif(PACS_MONITORING_SYSTEM_INCLUDE_DIR)
        # Fallback: include headers only
        pacs_link_external_dependency(
            pacs_integration
            PUBLIC
            pacs_monitoring_system_headers
            monitoring_system::monitoring_system
        )
        target_compile_definitions(pacs_integration PUBLIC PACS_WITH_MONITORING_SYSTEM)
        message(STATUS "    - monitoring_adapter: ON (headers only)")
    else()
        message(WARNING "    - monitoring_adapter: OFF (monitoring_system target not found)")
    endif()

    message(STATUS "  [OK] pacs_integration: ON (container_system, logger_system, thread_system, network_system, monitoring_system)")

    # CRITICAL: Link pacs_integration to pacs_network for thread_adapter usage in dicom_server.
    # dicom_server.cpp uses thread_adapter::submit_fire_and_forget() for association worker threads.
    # This creates a circular dependency (pacs_integration -> pacs_network -> pacs_integration)
    # but CMake handles this correctly for static libraries by passing libraries multiple times
    # to the linker. Using PUBLIC so that targets linking pacs_network also get pacs_integration
    # (required because static libraries don't actually link - symbols are resolved at final link).
    target_link_libraries(pacs_network PUBLIC pacs_integration)
    message(STATUS "    - pacs_network: linked pacs_integration for thread_adapter")

    # Link pacs_integration to pacs_ai for ai_service_connector (Issue #205)
    # ai_service_connector uses logger_adapter and monitoring_adapter
    if(TARGET pacs_ai)
        target_link_libraries(pacs_ai PUBLIC pacs_integration)
        message(STATUS "    - pacs_ai: linked pacs_integration for ai_service_connector")
    endif()

    # Link pacs_integration to pacs_workflow for auto_prefetch_service (Issue #206)
    # auto_prefetch_service uses logger_adapter, monitoring_adapter, and thread_adapter
    if(TARGET pacs_workflow)
        target_link_libraries(pacs_workflow PUBLIC pacs_integration)
        message(STATUS "    - pacs_workflow: linked pacs_integration for auto_prefetch_service")
    endif()
else()
    message(STATUS "  [--] pacs_integration: OFF (requires container_system and network_system)")
endif()

##################################################
# C++20 Module Support
##################################################
# Enable C++20 modules with CMake 3.28+ when building with module-capable compilers.
# This provides an alternative to the header-based interface.
#
# Usage:
#   cmake -DPACS_BUILD_MODULES=ON ..
#
# Requirements:
#   - CMake 3.28 or later
#   - Clang 16+, GCC 14+, or MSVC 2022 17.4+
##################################################

if(PACS_BUILD_MODULES)
    # Check CMake version
    if(CMAKE_VERSION VERSION_LESS "3.28")
        message(WARNING "C++20 modules require CMake 3.28+. Disabling module build.")
        set(PACS_BUILD_MODULES OFF)
    else()
        message(STATUS "")
        message(STATUS "=== C++20 Module Build ===")

        # Create module library target
        add_library(pacs_system_modules)
        add_library(kcenon::pacs_modules ALIAS pacs_system_modules)

        # Set C++20 standard for module target
        target_compile_features(pacs_system_modules PUBLIC cxx_std_20)

        # Enable module scanning
        set_target_properties(pacs_system_modules PROPERTIES
            CXX_SCAN_FOR_MODULES ON
        )

        # Add module source files
        target_sources(pacs_system_modules
            PUBLIC FILE_SET CXX_MODULES
            FILES
                # Primary module
                src/modules/pacs.cppm
                # Tier 1: Core partition
                src/modules/pacs-core.cppm
                # Tier 2: Infrastructure partitions
                src/modules/pacs-encoding.cppm
                src/modules/pacs-storage.cppm
                src/modules/pacs-security.cppm
                # Tier 3: Protocol partition
                src/modules/pacs-network.cppm
                # Tier 4: Services partition
                src/modules/pacs-services.cppm
                # Tier 5: Application partitions
                src/modules/pacs-workflow.cppm
                src/modules/pacs-web.cppm
                src/modules/pacs-integration.cppm
                # Tier 6: Optional partitions
                src/modules/pacs-ai.cppm
                src/modules/pacs-monitoring.cppm
                src/modules/pacs-di.cppm
        )

        # Include directories for module target
        target_include_directories(pacs_system_modules PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )

        # Link required libraries
        target_link_libraries(pacs_system_modules PUBLIC
            pacs_core
            pacs_encoding
            pacs_network
            pacs_services
            pacs_security
        )

        # Conditionally link optional libraries
        if(TARGET pacs_storage)
            target_link_libraries(pacs_system_modules PUBLIC pacs_storage)
        endif()
        if(TARGET pacs_workflow)
            target_link_libraries(pacs_system_modules PUBLIC pacs_workflow)
        endif()
        if(TARGET pacs_web)
            target_link_libraries(pacs_system_modules PUBLIC pacs_web)
        endif()
        if(TARGET pacs_integration)
            target_link_libraries(pacs_system_modules PUBLIC pacs_integration)
        endif()
        if(TARGET pacs_ai)
            target_link_libraries(pacs_system_modules PUBLIC pacs_ai)
        endif()
        if(TARGET pacs_monitoring)
            target_link_libraries(pacs_system_modules PUBLIC pacs_monitoring)
        endif()

        # Compile definitions
        target_compile_definitions(pacs_system_modules PUBLIC
            KCENON_USE_MODULES=1
        )

        message(STATUS "  C++20 module target: pacs_system_modules")
        message(STATUS "  Module partitions: 12")
        message(STATUS "==============================")
    endif()
endif()
