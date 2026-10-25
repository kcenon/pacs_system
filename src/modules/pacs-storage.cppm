// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-storage.cppm
 * @brief C++20 module partition for DICOM storage types.
 *
 * This module partition exports storage-related types:
 * - storage_interface: Abstract storage backend
 * - file_storage: File-based storage
 * - s3_storage: AWS S3 cloud storage
 * - azure_blob_storage: Azure Blob storage
 * - hsm_storage: Hierarchical Storage Management
 * - hsm_migration_service: Background migration service
 * - index_database: Metadata indexing
 * - migration_runner: Database schema migration
 * - Record types: patient, study, series, instance
 *
 * The exports below exist only when pacs_storage is built
 * (PACS_MODULES_WITH_STORAGE is defined by cmake/targets.cmake);
 * otherwise this partition is empty.
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_MODULES_WITH_STORAGE
// PACS storage headers - Core interfaces
#include <kcenon/pacs/storage/storage_interface.h>
#include <kcenon/pacs/storage/file_storage.h>
#include <kcenon/pacs/storage/index_database.h>

// Cloud storage backends
#include <kcenon/pacs/storage/s3_storage.h>
#include <kcenon/pacs/storage/azure_blob_storage.h>

// Hierarchical Storage Management
#include <kcenon/pacs/storage/hsm_types.h>
#include <kcenon/pacs/storage/hsm_storage.h>
#include <kcenon/pacs/storage/hsm_migration_service.h>

// Database migration
#include <kcenon/pacs/storage/migration_record.h>
#include <kcenon/pacs/storage/migration_runner.h>

// Record types
#include <kcenon/pacs/storage/patient_record.h>
#include <kcenon/pacs/storage/study_record.h>
#include <kcenon/pacs/storage/series_record.h>
#include <kcenon/pacs/storage/instance_record.h>
#include <kcenon/pacs/storage/mpps_record.h>
#include <kcenon/pacs/storage/worklist_record.h>
#include <kcenon/pacs/storage/audit_record.h>

// Security storage
#include <kcenon/pacs/storage/sqlite_security_storage.h>
#endif // PACS_MODULES_WITH_STORAGE

export module kcenon.pacs:storage;

#ifdef PACS_MODULES_WITH_STORAGE

// ============================================================================
// Re-export kcenon::pacs::storage namespace
// ============================================================================

export namespace kcenon::pacs::storage {

// Storage interfaces
using kcenon::pacs::storage::storage_statistics;
using kcenon::pacs::storage::storage_interface;
using kcenon::pacs::storage::naming_scheme;
using kcenon::pacs::storage::duplicate_policy;
using kcenon::pacs::storage::file_storage_config;
using kcenon::pacs::storage::file_storage;

// Cloud storage
using kcenon::pacs::storage::cloud_storage_config;
using kcenon::pacs::storage::s3_object_info;
using kcenon::pacs::storage::progress_callback;
using kcenon::pacs::storage::s3_storage;
using kcenon::pacs::storage::azure_storage_config;
using kcenon::pacs::storage::azure_blob_info;
using kcenon::pacs::storage::azure_progress_callback;
using kcenon::pacs::storage::azure_blob_storage;

// HSM types
using kcenon::pacs::storage::storage_tier;
using kcenon::pacs::storage::storage_tier_from_string;
using kcenon::pacs::storage::tier_policy;
using kcenon::pacs::storage::tier_metadata;
using kcenon::pacs::storage::migration_result;
using kcenon::pacs::storage::tier_statistics;
using kcenon::pacs::storage::hsm_statistics;

// HSM storage
using kcenon::pacs::storage::hsm_storage_config;
using kcenon::pacs::storage::hsm_storage;
using kcenon::pacs::storage::migration_service_config;
using kcenon::pacs::storage::hsm_migration_service;

// Database
using kcenon::pacs::storage::index_config;
using kcenon::pacs::storage::index_database;

// Database migration
using kcenon::pacs::storage::migration_record;
using kcenon::pacs::storage::migration_function;
using kcenon::pacs::storage::migration_runner;

// Record and query types
using kcenon::pacs::storage::patient_record;
using kcenon::pacs::storage::patient_query;
using kcenon::pacs::storage::study_record;
using kcenon::pacs::storage::study_query;
using kcenon::pacs::storage::series_record;
using kcenon::pacs::storage::series_query;
using kcenon::pacs::storage::instance_record;
using kcenon::pacs::storage::instance_query;
using kcenon::pacs::storage::mpps_status;
using kcenon::pacs::storage::parse_mpps_status;
using kcenon::pacs::storage::performed_series_info;
using kcenon::pacs::storage::mpps_record;
using kcenon::pacs::storage::mpps_query;
using kcenon::pacs::storage::worklist_status;
using kcenon::pacs::storage::parse_worklist_status;
using kcenon::pacs::storage::worklist_item;
using kcenon::pacs::storage::worklist_query;
using kcenon::pacs::storage::audit_event_type;
using kcenon::pacs::storage::parse_audit_event_type;
using kcenon::pacs::storage::audit_outcome;
using kcenon::pacs::storage::audit_record;
using kcenon::pacs::storage::audit_query;

// to_string overloads (storage_tier, mpps, worklist, and audit enums)
using kcenon::pacs::storage::to_string;

// Security storage
using kcenon::pacs::storage::sqlite_security_storage;

} // namespace kcenon::pacs::storage

#endif // PACS_MODULES_WITH_STORAGE
