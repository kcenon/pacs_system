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

export module kcenon.pacs:storage;

// ============================================================================
// Re-export pacs::storage namespace
// ============================================================================

export namespace pacs::storage {

// Storage interfaces
using pacs::storage::storage_interface;
using pacs::storage::file_storage;

// Cloud storage
using pacs::storage::cloud_storage_config;
using pacs::storage::s3_storage;
using pacs::storage::azure_storage_config;
using pacs::storage::azure_blob_storage;

// HSM types
using pacs::storage::storage_tier;
using pacs::storage::to_string;
using pacs::storage::storage_tier_from_string;
using pacs::storage::tier_policy;

// HSM storage
using pacs::storage::hsm_storage_config;
using pacs::storage::hsm_storage;
using pacs::storage::migration_service_config;
using pacs::storage::hsm_migration_service;

// Database
using pacs::storage::index_database;

// Database migration
using pacs::storage::migration_record;
using pacs::storage::migration_runner;

// Record types
using pacs::storage::patient_record;
using pacs::storage::study_record;
using pacs::storage::series_record;
using pacs::storage::instance_record;
using pacs::storage::mpps_record;
using pacs::storage::worklist_record;
using pacs::storage::audit_record;

// Security storage
using pacs::storage::sqlite_security_storage;

} // namespace pacs::storage
