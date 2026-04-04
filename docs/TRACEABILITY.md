---
doc_id: "PAC-QUAL-008"
doc_title: "Feature-Test-Module Traceability Matrix"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "QUAL"
---

# Traceability Matrix

> **SSOT**: This document is the single source of truth for **PACS System Feature-Test-Module Traceability**.

## Feature -> Test -> Module Mapping

### DICOM Core

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-001 | DICOM Element | tests/core/dicom_element_test.cpp | include/kcenon/pacs/core/, src/core/ | Covered |
| PAC-FEAT-002 | DICOM Tag | tests/core/dicom_tag_test.cpp | include/kcenon/pacs/core/ | Covered |
| PAC-FEAT-003 | Tag Info | tests/core/tag_info_test.cpp | include/kcenon/pacs/core/ | Covered |
| PAC-FEAT-004 | DICOM Dataset | tests/core/dicom_dataset_test.cpp | include/kcenon/pacs/core/ | Covered |
| PAC-FEAT-005 | DICOM Dictionary | tests/core/dicom_dictionary_test.cpp | include/kcenon/pacs/core/ | Covered |
| PAC-FEAT-006 | DICOM File (Part 10) | tests/core/dicom_file_test.cpp | include/kcenon/pacs/core/ | Covered |
| PAC-FEAT-007 | Events | tests/core/events_test.cpp | include/kcenon/pacs/core/ | Covered |
| PAC-FEAT-008 | Private Tag Registry | tests/core/private_tag_registry_test.cpp | include/kcenon/pacs/core/ | Covered |

### Encoding & Transfer Syntaxes

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-009 | Implicit VR Codec | tests/encoding/implicit_vr_codec_test.cpp | include/kcenon/pacs/encoding/, src/encoding/ | Covered |
| PAC-FEAT-010 | Explicit VR Codec | tests/encoding/explicit_vr_codec_test.cpp | include/kcenon/pacs/encoding/ | Covered |
| PAC-FEAT-011 | Explicit VR Big Endian Codec | tests/encoding/explicit_vr_big_endian_codec_test.cpp | include/kcenon/pacs/encoding/ | Covered |
| PAC-FEAT-012 | Transfer Syntax Support | tests/encoding/transfer_syntax_test.cpp | include/kcenon/pacs/encoding/ | Covered |
| PAC-FEAT-013 | VR Type System | tests/encoding/vr_type_test.cpp, tests/encoding/vr_info_test.cpp | include/kcenon/pacs/encoding/ | Covered |
| PAC-FEAT-014 | Byte Swap | tests/encoding/byte_swap_test.cpp | include/kcenon/pacs/encoding/ | Covered |
| PAC-FEAT-015 | Character Set Handling | tests/encoding/character_set_test.cpp | include/kcenon/pacs/encoding/ | Covered |
| PAC-FEAT-016 | SIMD RLE | tests/encoding/simd/simd_rle_test.cpp | include/kcenon/pacs/encoding/simd/ | Covered |

### Compression Codecs

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-017 | JPEG Baseline Codec | tests/encoding/compression/jpeg_baseline_codec_test.cpp | src/encoding/compression/ | Covered |
| PAC-FEAT-018 | JPEG Lossless Codec | tests/encoding/compression/jpeg_lossless_codec_test.cpp | src/encoding/compression/ | Covered |
| PAC-FEAT-019 | JPEG 2000 Codec | tests/encoding/compression/jpeg2000_codec_test.cpp | src/encoding/compression/ | Covered |
| PAC-FEAT-020 | JPEG-LS Codec | tests/encoding/compression/jpeg_ls_codec_test.cpp | src/encoding/compression/ | Covered |
| PAC-FEAT-021 | HTJ2K Codec | tests/encoding/compression/htj2k_codec_test.cpp | src/encoding/compression/ | Covered |
| PAC-FEAT-022 | HEVC Codec | tests/encoding/compression/hevc_codec_test.cpp | src/encoding/compression/ | Covered |
| PAC-FEAT-023 | JPEG XL Codec | tests/encoding/compression/jpegxl_codec_test.cpp | src/encoding/compression/ | Covered |
| PAC-FEAT-024 | RLE Codec | tests/encoding/compression/rle_codec_test.cpp | src/encoding/compression/ | Covered |
| PAC-FEAT-025 | Frame Deflate Codec | tests/encoding/compression/frame_deflate_codec_test.cpp | src/encoding/compression/ | Covered |

### Network Protocol

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-026 | Association State Machine | tests/network/association_test.cpp | include/kcenon/pacs/network/, src/network/ | Covered |
| PAC-FEAT-027 | PDU Encoder | tests/network/pdu_encoder_test.cpp | src/network/ | Covered |
| PAC-FEAT-028 | PDU Decoder | tests/network/pdu_decoder_test.cpp | src/network/ | Covered |
| PAC-FEAT-029 | DIMSE Messages | tests/network/dimse/dimse_message_test.cpp | src/network/dimse/ | Covered |
| PAC-FEAT-030 | N-Service | tests/network/dimse/n_service_test.cpp | src/network/dimse/ | Covered |
| PAC-FEAT-031 | DICOM Server | tests/network/dicom_server_test.cpp | src/network/ | Covered |
| PAC-FEAT-032 | Accept Worker | tests/network/detail/accept_worker_test.cpp | src/network/detail/ | Covered |

### Network Pipeline

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-033 | Pipeline Coordinator | tests/network/pipeline/pipeline_coordinator_test.cpp | src/network/pipeline/ | Covered |
| PAC-FEAT-034 | Pipeline Integration | tests/network/pipeline/pipeline_integration_test.cpp | src/network/pipeline/ | Covered |
| PAC-FEAT-035 | Pipeline Job Types | tests/network/pipeline/pipeline_job_types_test.cpp | src/network/pipeline/ | Covered |

### Network V2

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-036 | DICOM Server V2 | tests/network/v2/dicom_server_v2_test.cpp | src/network/v2/ | Covered |
| PAC-FEAT-037 | Association Handler V2 | tests/network/v2/dicom_association_handler_test.cpp | src/network/v2/ | Covered |
| PAC-FEAT-038 | PDU Framing V2 | tests/network/v2/pdu_framing_test.cpp | src/network/v2/ | Covered |
| PAC-FEAT-039 | State Machine V2 | tests/network/v2/state_machine_test.cpp | src/network/v2/ | Covered |
| PAC-FEAT-040 | Service Dispatching V2 | tests/network/v2/service_dispatching_test.cpp | src/network/v2/ | Covered |
| PAC-FEAT-041 | Network System Integration V2 | tests/network/v2/network_system_integration_test.cpp | src/network/v2/ | Covered |

### DICOM Services

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-042 | Verification SCP (C-ECHO) | tests/services/verification_scp_test.cpp | src/services/sop_classes/ | Covered |
| PAC-FEAT-043 | Storage SCP | tests/services/storage_scp_test.cpp | src/services/sop_classes/ | Covered |
| PAC-FEAT-044 | Storage SCU | tests/services/storage_scu_test.cpp | src/services/ | Covered |
| PAC-FEAT-045 | Query SCP (C-FIND) | tests/services/query_scp_test.cpp | src/services/ | Covered |
| PAC-FEAT-046 | Retrieve SCP (C-MOVE) | tests/services/retrieve_scp_test.cpp | src/services/ | Covered |
| PAC-FEAT-047 | Retrieve SCU | tests/services/retrieve_scu_test.cpp | src/services/ | Covered |
| PAC-FEAT-048 | Worklist SCP | tests/services/worklist_scp_test.cpp | src/services/ | Covered |
| PAC-FEAT-049 | Worklist SCU | tests/services/worklist_scu_test.cpp | src/services/ | Covered |
| PAC-FEAT-050 | MPPS SCP | tests/services/mpps_scp_test.cpp | src/services/ | Covered |
| PAC-FEAT-051 | MPPS SCU | tests/services/mpps_scu_test.cpp | src/services/ | Covered |
| PAC-FEAT-052 | Storage Commitment SCP | tests/services/storage_commitment_scp_test.cpp | src/services/ | Covered |
| PAC-FEAT-053 | Storage Commitment SCU | tests/services/storage_commitment_scu_test.cpp | src/services/ | Covered |
| PAC-FEAT-054 | Storage Commitment Types | tests/services/storage_commitment_types_test.cpp | src/services/ | Covered |
| PAC-FEAT-055 | Print SCP | tests/services/print_scp_test.cpp | src/services/ | Covered |
| PAC-FEAT-056 | Print SCU | tests/services/print_scu_test.cpp | src/services/ | Covered |
| PAC-FEAT-057 | N-GET SCP | tests/services/n_get_scp_test.cpp | src/services/ | Covered |
| PAC-FEAT-058 | N-GET SCU | tests/services/n_get_scu_test.cpp | src/services/ | Covered |
| PAC-FEAT-059 | UPS Push SCP | tests/services/ups_push_scp_test.cpp | src/services/ups/ | Covered |
| PAC-FEAT-060 | UPS Push SCU | tests/services/ups_push_scu_test.cpp | src/services/ups/ | Covered |
| PAC-FEAT-061 | UPS Query SCP | tests/services/ups_query_scp_test.cpp | src/services/ups/ | Covered |
| PAC-FEAT-062 | UPS Watch SCP | tests/services/ups_watch_scp_test.cpp | src/services/ups/ | Covered |
| PAC-FEAT-063 | UPS Watch SCU | tests/services/ups_watch_scu_test.cpp | src/services/ups/ | Covered |

### Modality-Specific Storage

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-064 | CT Storage | tests/services/ct_storage_test.cpp | src/services/sop_classes/ | Covered |
| PAC-FEAT-065 | MR Storage (MR IOD) | tests/services/mr_iod_validator_test.cpp | src/services/validation/ | Covered |
| PAC-FEAT-066 | US Storage | tests/services/us_storage_test.cpp | src/services/sop_classes/ | Covered |
| PAC-FEAT-067 | XA Storage | tests/services/xa_storage_test.cpp | src/services/sop_classes/ | Covered |
| PAC-FEAT-068 | NM Storage | tests/services/nm_storage_test.cpp, tests/services/nm_iod_validator_test.cpp | src/services/sop_classes/ | Covered |
| PAC-FEAT-069 | PET Storage | tests/services/pet_storage_test.cpp, tests/services/pet_iod_validator_test.cpp | src/services/sop_classes/ | Covered |
| PAC-FEAT-070 | RT Storage | tests/services/rt_storage_test.cpp, tests/services/rt_iod_validator_test.cpp | src/services/sop_classes/ | Covered |
| PAC-FEAT-071 | SR Storage | tests/services/sr_storage_test.cpp | src/services/sop_classes/ | Covered |
| PAC-FEAT-072 | SEG Storage | tests/services/seg_storage_test.cpp | src/services/sop_classes/ | Covered |
| PAC-FEAT-073 | MG Storage | tests/services/mg_storage_test.cpp, tests/services/mg_iod_validator_test.cpp | src/services/sop_classes/ | Covered |
| PAC-FEAT-074 | DX Storage | tests/services/dx_storage_test.cpp | src/services/sop_classes/ | Covered |
| PAC-FEAT-075 | Waveform Storage | tests/services/waveform_storage_test.cpp | src/services/sop_classes/ | Covered |
| PAC-FEAT-076 | Ophthalmic Storage | tests/services/ophthalmic_storage_test.cpp, tests/services/ophthalmic_iod_validator_test.cpp | src/services/sop_classes/ | Covered |
| PAC-FEAT-077 | Parametric Map Storage | tests/services/parametric_map_storage_test.cpp, tests/services/parametric_map_iod_validator_test.cpp | src/services/sop_classes/ | Covered |
| PAC-FEAT-078 | WSI Storage | tests/services/wsi_storage_test.cpp, tests/services/wsi_iod_validator_test.cpp | src/services/sop_classes/ | Covered |

### IOD Validators

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-079 | CT IOD Validator | tests/services/ct_iod_validator_test.cpp | src/services/validation/ | Covered |
| PAC-FEAT-080 | CT Processing IOD Validator | tests/services/ct_processing_iod_validator_test.cpp | src/services/validation/ | Covered |
| PAC-FEAT-081 | Heightmap SEG IOD Validator | tests/services/heightmap_seg_iod_validator_test.cpp | src/services/validation/ | Covered |
| PAC-FEAT-082 | Label Map SEG IOD Validator | tests/services/label_map_seg_iod_validator_test.cpp | src/services/validation/ | Covered |

### Services - Cache & Query

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-083 | Query Cache | tests/services/cache/query_cache_test.cpp | src/services/cache/ | Covered |
| PAC-FEAT-084 | Simple LRU Cache | tests/services/cache/simple_lru_cache_test.cpp | src/services/cache/ | Covered |
| PAC-FEAT-085 | Parallel Query Executor | tests/services/cache/parallel_query_executor_test.cpp | src/services/cache/ | Covered |
| PAC-FEAT-086 | Streaming Query | tests/services/cache/streaming_query_test.cpp | src/services/cache/ | Covered |
| PAC-FEAT-087 | SQL Injection Prevention | tests/services/cache/sql_injection_prevention_test.cpp | src/services/cache/ | Covered |

### Advanced Services

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-088 | Patient Reconciliation (PIR) | tests/services/pir/patient_reconciliation_test.cpp | src/services/pir/ | Covered |
| PAC-FEAT-089 | XDS-I.b Imaging | tests/services/xds/xds_imaging_test.cpp | src/services/xds/ | Covered |
| PAC-FEAT-090 | Database Metrics Service | tests/services/monitoring/database_metrics_service_test.cpp | src/services/monitoring/ | Covered |

### Security

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-091 | Access Control Manager (RBAC) | tests/security/access_control_manager_test.cpp | include/kcenon/pacs/security/, src/security/ | Covered |
| PAC-FEAT-092 | Anonymizer (PS3.15) | tests/security/anonymizer_test.cpp | src/security/ | Covered |
| PAC-FEAT-093 | Digital Signature | tests/security/digital_signature_test.cpp | src/security/ | Covered |
| PAC-FEAT-094 | UID Mapping | tests/security/uid_mapping_test.cpp | src/security/ | Covered |
| PAC-FEAT-095 | TLS Policy | tests/security/tls_policy_test.cpp | src/security/ | Covered |
| PAC-FEAT-096 | SQLite Security Storage | tests/security/sqlite_security_storage_test.cpp | src/security/ | Covered |
| PAC-FEAT-097 | ATNA Audit Logger | tests/security/atna_audit_logger_test.cpp | src/security/ | Covered |
| PAC-FEAT-098 | ATNA Config | tests/security/atna_config_test.cpp | src/security/ | Covered |
| PAC-FEAT-099 | ATNA Service Auditor | tests/security/atna_service_auditor_test.cpp | src/security/ | Covered |
| PAC-FEAT-100 | ATNA Syslog Transport | tests/security/atna_syslog_transport_test.cpp | src/security/ | Covered |

### Storage Backend

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-101 | File Storage | tests/storage/file_storage_test.cpp | include/kcenon/pacs/storage/, src/storage/ | Covered |
| PAC-FEAT-102 | Index Database (SQLite) | tests/storage/index_database_test.cpp | src/storage/ | Covered |
| PAC-FEAT-103 | Base Repository | tests/storage/base_repository_test.cpp | src/storage/ | Covered |
| PAC-FEAT-104 | Patient Repository | tests/storage/patient_repository_test.cpp | src/storage/ | Covered |
| PAC-FEAT-105 | Study Repository | tests/storage/study_repository_test.cpp | src/storage/ | Covered |
| PAC-FEAT-106 | Series Repository | tests/storage/series_repository_test.cpp | src/storage/ | Covered |
| PAC-FEAT-107 | Instance Repository | tests/storage/instance_repository_test.cpp | src/storage/ | Covered |
| PAC-FEAT-108 | MPPS Repository | tests/storage/mpps_repository_test.cpp, tests/storage/mpps_test.cpp | src/storage/ | Covered |
| PAC-FEAT-109 | Worklist Repository | tests/storage/worklist_repository_test.cpp, tests/storage/worklist_test.cpp | src/storage/ | Covered |
| PAC-FEAT-110 | UPS Repository | tests/storage/ups_repository_test.cpp, tests/storage/ups_workitem_test.cpp | src/storage/ | Covered |
| PAC-FEAT-111 | Commitment Repository | tests/storage/commitment_repository_test.cpp | src/storage/ | Covered |
| PAC-FEAT-112 | Audit Repository | tests/storage/audit_repository_test.cpp | src/storage/ | Covered |
| PAC-FEAT-113 | Annotation Repository | tests/storage/annotation_repository_test.cpp | src/storage/ | Covered |
| PAC-FEAT-114 | Repository Factory | tests/storage/repository_factory_test.cpp | src/storage/ | Covered |
| PAC-FEAT-115 | Storage Interface | tests/storage/storage_interface_test.cpp | include/kcenon/pacs/storage/ | Covered |
| PAC-FEAT-116 | Migration Runner | tests/storage/migration_runner_test.cpp | src/storage/ | Covered |
| PAC-FEAT-117 | Database Adapter (PACS) | tests/storage/pacs_database_adapter_test.cpp | src/storage/ | Covered |
| PAC-FEAT-118 | S3 Cloud Storage | tests/storage/s3_storage_test.cpp | src/storage/ | Covered |
| PAC-FEAT-119 | Azure Blob Storage | tests/storage/azure_blob_storage_test.cpp | src/storage/ | Covered |
| PAC-FEAT-120 | HSM Storage | tests/storage/hsm_storage_test.cpp | src/storage/ | Covered |

### AI Integration

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-121 | AI Service Connector | tests/ai/ai_service_connector_test.cpp | include/kcenon/pacs/ai/, src/ai/ | Covered |
| PAC-FEAT-122 | AI Result Handler | tests/ai/ai_result_handler_test.cpp | src/ai/ | Covered |
| PAC-FEAT-123 | AIRA Assessment | tests/ai/aira_assessment_test.cpp | src/ai/ | Covered |

### Client Module

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-124 | Job Manager | tests/client/job_manager_test.cpp | include/kcenon/pacs/client/, src/client/ | Covered |
| PAC-FEAT-125 | Routing Manager | tests/client/routing_manager_test.cpp | src/client/ | Covered |
| PAC-FEAT-126 | Sync Manager | tests/client/sync_manager_test.cpp | src/client/ | Covered |
| PAC-FEAT-127 | Prefetch Manager | tests/client/prefetch_manager_test.cpp | src/client/ | Covered |

### Monitoring

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-128 | PACS Metrics | tests/monitoring/pacs_metrics_test.cpp | include/kcenon/pacs/monitoring/, src/monitoring/ | Covered |
| PAC-FEAT-129 | Health Checker | tests/monitoring/health_checker_test.cpp | src/monitoring/ | Covered |
| PAC-FEAT-130 | Health Status | tests/monitoring/health_status_test.cpp | src/monitoring/ | Covered |
| PAC-FEAT-131 | Health JSON | tests/monitoring/health_json_test.cpp | src/monitoring/ | Covered |
| PAC-FEAT-132 | Collectors | tests/monitoring/collectors_test.cpp | include/kcenon/pacs/monitoring/collectors/ | Covered |

### Workflow

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-133 | Auto Prefetch Service | tests/workflow/auto_prefetch_service_test.cpp | include/kcenon/pacs/workflow/, src/workflow/ | Covered |
| PAC-FEAT-134 | Task Scheduler | tests/workflow/task_scheduler_test.cpp | src/workflow/ | Covered |
| PAC-FEAT-135 | Study Lock Manager | tests/workflow/study_lock_manager_test.cpp | src/workflow/ | Covered |

### Web API (REST / DICOMweb)

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-136 | REST Server | tests/web/rest_server_test.cpp | include/kcenon/pacs/web/, src/web/ | Covered |
| PAC-FEAT-137 | DICOMweb Endpoints (WADO/STOW/QIDO) | tests/web/dicomweb_endpoints_test.cpp | src/web/endpoints/ | Covered |
| PAC-FEAT-138 | Patient/Study Endpoints | tests/web/patient_study_endpoints_test.cpp | src/web/endpoints/ | Covered |
| PAC-FEAT-139 | System Endpoints | tests/web/system_endpoints_test.cpp | src/web/endpoints/ | Covered |
| PAC-FEAT-140 | Jobs Endpoints | tests/web/jobs_endpoints_test.cpp | src/web/endpoints/ | Covered |
| PAC-FEAT-141 | WADO-URI Endpoints | tests/web/wado_uri_endpoints_test.cpp | src/web/endpoints/ | Covered |
| PAC-FEAT-142 | Annotation Endpoints | tests/web/annotation_endpoints_test.cpp | src/web/endpoints/ | Covered |
| PAC-FEAT-143 | Key Image Endpoints | tests/web/key_image_endpoints_test.cpp | src/web/endpoints/ | Covered |
| PAC-FEAT-144 | Measurement Endpoints | tests/web/measurement_endpoints_test.cpp | src/web/endpoints/ | Covered |
| PAC-FEAT-145 | Storage Commitment Endpoints | tests/web/storage_commitment_endpoints_test.cpp | src/web/endpoints/ | Covered |
| PAC-FEAT-146 | Viewer State Endpoints | tests/web/viewer_state_endpoints_test.cpp | src/web/endpoints/ | Covered |
| PAC-FEAT-147 | Worklist Audit Endpoints | tests/web/worklist_audit_endpoints_test.cpp | src/web/endpoints/ | Covered |
| PAC-FEAT-148 | Thumbnail Service | tests/web/thumbnail_service_test.cpp | src/web/ | Covered |
| PAC-FEAT-149 | Metadata Service | tests/web/metadata_service_test.cpp | src/web/ | Covered |

### Web Auth

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-150 | JWT Validator | tests/web/auth/jwt_validator_test.cpp | include/kcenon/pacs/web/auth/, src/web/auth/ | Covered |
| PAC-FEAT-151 | OAuth2 Middleware | tests/web/auth/oauth2_middleware_test.cpp | src/web/auth/ | Covered |

### Ecosystem Integration

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-152 | Container Adapter | tests/integration/container_adapter_test.cpp | include/kcenon/pacs/integration/, src/integration/ | Covered |
| PAC-FEAT-153 | Logger Adapter | tests/integration/logger_adapter_test.cpp | src/integration/ | Covered |
| PAC-FEAT-154 | Monitoring Adapter | tests/integration/monitoring_adapter_test.cpp | src/integration/ | Covered |
| PAC-FEAT-155 | Network Adapter | tests/integration/network_adapter_test.cpp | src/integration/ | Covered |
| PAC-FEAT-156 | Thread System Direct | tests/integration/thread_system_direct_test.cpp | src/integration/ | Covered |

### DI & Modules

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-157 | Service Registration (DI) | tests/di/service_registration_test.cpp | include/kcenon/pacs/di/ | Covered |
| PAC-FEAT-158 | ILogger Test (DI) | tests/di/ilogger_test.cpp | include/kcenon/pacs/di/ | Covered |
| PAC-FEAT-159 | Module Import | tests/modules/module_import_test.cpp | src/modules/ | Covered |

### Quality & Integration

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| PAC-FEAT-160 | Lock-Free Stress | tests/concurrency/lockfree_stress_test.cpp | (cross-cutting) | Covered |
| PAC-FEAT-161 | DICOM Workflow Integration | tests/integration/dicom_workflow_integration_test.cpp | (cross-cutting) | Covered |
| PAC-FEAT-162 | Error Propagation Integration | tests/integration/error_propagation_integration_test.cpp | (cross-cutting) | Covered |
| PAC-FEAT-163 | Load Integration | tests/integration/load_integration_test.cpp | (cross-cutting) | Covered |
| PAC-FEAT-164 | Shutdown Integration | tests/integration/shutdown_integration_test.cpp | (cross-cutting) | Covered |
| PAC-FEAT-165 | Config Reload Integration | tests/integration/config_reload_integration_test.cpp | (cross-cutting) | Covered |
| PAC-FEAT-166 | DCM Modify Utility | tests/utilities/dcm_modify_test.cpp | (utilities) | Covered |

## Coverage Summary

| Category | Total Features | Covered | Partial | Uncovered |
|----------|---------------|---------|---------|-----------|
| DICOM Core | 8 | 8 | 0 | 0 |
| Encoding & Transfer Syntaxes | 8 | 8 | 0 | 0 |
| Compression Codecs | 9 | 9 | 0 | 0 |
| Network Protocol | 7 | 7 | 0 | 0 |
| Network Pipeline | 3 | 3 | 0 | 0 |
| Network V2 | 6 | 6 | 0 | 0 |
| DICOM Services | 22 | 22 | 0 | 0 |
| Modality-Specific Storage | 15 | 15 | 0 | 0 |
| IOD Validators | 4 | 4 | 0 | 0 |
| Services - Cache & Query | 5 | 5 | 0 | 0 |
| Advanced Services | 3 | 3 | 0 | 0 |
| Security | 10 | 10 | 0 | 0 |
| Storage Backend | 20 | 20 | 0 | 0 |
| AI Integration | 3 | 3 | 0 | 0 |
| Client Module | 4 | 4 | 0 | 0 |
| Monitoring | 5 | 5 | 0 | 0 |
| Workflow | 3 | 3 | 0 | 0 |
| Web API | 14 | 14 | 0 | 0 |
| Web Auth | 2 | 2 | 0 | 0 |
| Ecosystem Integration | 5 | 5 | 0 | 0 |
| DI & Modules | 3 | 3 | 0 | 0 |
| Quality & Integration | 7 | 7 | 0 | 0 |
| **Total** | **166** | **166** | **0** | **0** |

## See Also

- [FEATURES.md](FEATURES.md) -- Detailed feature documentation
- [README.md](README.md) -- SSOT Documentation Registry
