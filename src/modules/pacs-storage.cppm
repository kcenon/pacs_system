// BSD 3-Clause License
// Copyright (c) 2025, kcenon
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
#include <pacs/storage/storage_interface.hpp>
#include <pacs/storage/file_storage.hpp>
#include <pacs/storage/index_database.hpp>

// Cloud storage backends
#include <pacs/storage/s3_storage.hpp>
#include <pacs/storage/azure_blob_storage.hpp>

// Hierarchical Storage Management
#include <pacs/storage/hsm_types.hpp>
#include <pacs/storage/hsm_storage.hpp>
#include <pacs/storage/hsm_migration_service.hpp>

// Database migration
#include <pacs/storage/migration_record.hpp>
#include <pacs/storage/migration_runner.hpp>

// Record types
#include <pacs/storage/patient_record.hpp>
#include <pacs/storage/study_record.hpp>
#include <pacs/storage/series_record.hpp>
#include <pacs/storage/instance_record.hpp>
#include <pacs/storage/mpps_record.hpp>
#include <pacs/storage/worklist_record.hpp>
#include <pacs/storage/audit_record.hpp>

// Security storage
#include <pacs/storage/sqlite_security_storage.hpp>

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
