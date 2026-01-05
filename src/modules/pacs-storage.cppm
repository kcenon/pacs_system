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
 * - index_database: Metadata indexing
 * - Record types: patient, study, series, instance
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports
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

// PACS storage headers
#include <pacs/storage/storage_interface.hpp>
#include <pacs/storage/file_storage.hpp>
#include <pacs/storage/index_database.hpp>
#include <pacs/storage/patient_record.hpp>
#include <pacs/storage/study_record.hpp>
#include <pacs/storage/series_record.hpp>
#include <pacs/storage/instance_record.hpp>
#include <pacs/storage/mpps_record.hpp>
#include <pacs/storage/worklist_record.hpp>
#include <pacs/storage/audit_record.hpp>

export module kcenon.pacs:storage;

// ============================================================================
// Re-export pacs::storage namespace
// ============================================================================

export namespace pacs::storage {

// Storage interfaces
using pacs::storage::storage_interface;
using pacs::storage::file_storage;

// Database
using pacs::storage::index_database;

// Record types
using pacs::storage::patient_record;
using pacs::storage::study_record;
using pacs::storage::series_record;
using pacs::storage::instance_record;
using pacs::storage::mpps_record;
using pacs::storage::worklist_record;
using pacs::storage::audit_record;

} // namespace pacs::storage
