/**
 * @file index_database.hpp
 * @brief PACS index database for metadata storage and retrieval
 *
 * This file provides the index_database class for managing DICOM metadata
 * in a SQLite database. Supports CRUD operations for patients, studies,
 * series, and instances.
 *
 * When compiled with PACS_WITH_DATABASE_SYSTEM, uses database_system's
 * query builder for parameterized queries. Otherwise, uses direct SQLite
 * with prepared statements.
 *
 * @see SRS-STOR-003, FR-4.2
 */

#pragma once

#include "audit_record.hpp"
#include "instance_record.hpp"
#include "migration_runner.hpp"
#include "mpps_record.hpp"
#include "patient_record.hpp"
#include "series_record.hpp"
#include "study_record.hpp"
#include "worklist_record.hpp"

#include <kcenon/common/patterns/result.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM
#include <database/database_manager.h>
#include <database/core/database_context.h>
#endif

// Forward declaration of SQLite handle
struct sqlite3;

namespace pacs::storage {

/**
 * @brief Configuration for index database
 *
 * Allows customization of SQLite database behavior including
 * caching, journaling mode, and other performance options.
 */
struct index_config {
    /// Cache size in megabytes (default: 64 MB)
    size_t cache_size_mb = 64;

    /// Enable WAL (Write-Ahead Logging) mode for better concurrency
    bool wal_mode = true;

    /// Enable memory-mapped I/O for faster reads
    bool mmap_enabled = true;

    /// Maximum memory map size in bytes (default: 1 GB)
    size_t mmap_size = 1024 * 1024 * 1024;
};

/// Result type alias for operations returning a value
template <typename T>
using Result = kcenon::common::Result<T>;

/// Result type alias for void operations
using VoidResult = kcenon::common::VoidResult;

/**
 * @brief PACS index database manager
 *
 * Provides database operations for DICOM metadata storage and retrieval.
 * Uses SQLite for persistence with automatic schema migration.
 *
 * Thread Safety: This class is NOT thread-safe. External synchronization
 * is required for concurrent access. Consider using a connection pool
 * for multi-threaded applications.
 *
 * @example
 * @code
 * // Open or create database
 * auto db_result = index_database::open(":memory:");
 * if (db_result.is_err()) {
 *     // Handle error
 * }
 * auto db = std::move(db_result.value());
 *
 * // Insert patient
 * auto pk = db->upsert_patient("12345", "Doe^John", "19800115", "M");
 *
 * // Find patient
 * auto patient = db->find_patient("12345");
 * if (patient) {
 *     std::cout << patient->patient_name << std::endl;
 * }
 *
 * // Search with wildcards
 * patient_query query;
 * query.patient_name = "Doe*";
 * auto results = db->search_patients(query);
 * @endcode
 */
class index_database {
public:
    /**
     * @brief Open or create a database with default configuration
     *
     * Opens an existing database or creates a new one at the specified path.
     * Automatically runs pending migrations. Uses default configuration
     * with WAL mode enabled.
     *
     * @param db_path Path to the database file, or ":memory:" for in-memory DB
     * @return Result containing the database instance or error
     */
    [[nodiscard]] static auto open(std::string_view db_path)
        -> Result<std::unique_ptr<index_database>>;

    /**
     * @brief Open or create a database with custom configuration
     *
     * Opens an existing database or creates a new one at the specified path.
     * Automatically runs pending migrations.
     *
     * @param db_path Path to the database file, or ":memory:" for in-memory DB
     * @param config Configuration options for database behavior
     * @return Result containing the database instance or error
     */
    [[nodiscard]] static auto open(std::string_view db_path,
                                   const index_config& config)
        -> Result<std::unique_ptr<index_database>>;

    /**
     * @brief Destructor - closes database connection
     */
    ~index_database();

    // Non-copyable, movable
    index_database(const index_database&) = delete;
    auto operator=(const index_database&) -> index_database& = delete;
    index_database(index_database&&) noexcept;
    auto operator=(index_database&&) noexcept -> index_database&;

    // ========================================================================
    // Patient Operations
    // ========================================================================

    /**
     * @brief Insert or update a patient record
     *
     * If a patient with the same patient_id exists, updates the record.
     * Otherwise, inserts a new record.
     *
     * @param patient_id Patient ID (required, max 64 chars)
     * @param patient_name Patient's name in DICOM PN format
     * @param birth_date Birth date in YYYYMMDD format
     * @param sex Sex code (M, F, O)
     * @return Result containing the patient's primary key or error
     */
    [[nodiscard]] auto upsert_patient(std::string_view patient_id,
                                      std::string_view patient_name = "",
                                      std::string_view birth_date = "",
                                      std::string_view sex = "")
        -> Result<int64_t>;

    /**
     * @brief Insert or update a patient record with full details
     *
     * @param record Complete patient record (pk field is ignored for insert)
     * @return Result containing the patient's primary key or error
     */
    [[nodiscard]] auto upsert_patient(const patient_record& record)
        -> Result<int64_t>;

    /**
     * @brief Find a patient by patient ID
     *
     * @param patient_id The patient ID to search for
     * @return Optional containing the patient record if found
     */
    [[nodiscard]] auto find_patient(std::string_view patient_id) const
        -> std::optional<patient_record>;

    /**
     * @brief Find a patient by primary key
     *
     * @param pk The patient's primary key
     * @return Optional containing the patient record if found
     */
    [[nodiscard]] auto find_patient_by_pk(int64_t pk) const
        -> std::optional<patient_record>;

    /**
     * @brief Search patients with query criteria
     *
     * Supports wildcard matching using '*' character.
     * - "Doe*" matches names starting with "Doe"
     * - "*John" matches names ending with "John"
     * - "*oh*" matches names containing "oh"
     *
     * @param query Query parameters with optional filters
     * @return Result containing vector of matching patient records or error
     */
    [[nodiscard]] auto search_patients(const patient_query& query) const
        -> Result<std::vector<patient_record>>;

    /**
     * @brief Delete a patient by patient ID
     *
     * This operation cascades to delete all related studies, series,
     * and instances.
     *
     * @param patient_id The patient ID to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto delete_patient(std::string_view patient_id)
        -> VoidResult;

    /**
     * @brief Get total patient count
     *
     * @return Result containing number of patients in the database or error
     */
    [[nodiscard]] auto patient_count() const -> Result<size_t>;

    // ========================================================================
    // Study Operations
    // ========================================================================

    /**
     * @brief Insert or update a study record
     *
     * If a study with the same study_uid exists, updates the record.
     * Otherwise, inserts a new record.
     *
     * @param patient_pk Foreign key to the patient (required)
     * @param study_uid Study Instance UID (required, max 64 chars)
     * @param study_id Study ID
     * @param study_date Study date in YYYYMMDD format
     * @param study_time Study time in HHMMSS format
     * @param accession_number Accession number
     * @param referring_physician Referring physician's name
     * @param study_description Study description
     * @return Result containing the study's primary key or error
     */
    [[nodiscard]] auto upsert_study(int64_t patient_pk,
                                    std::string_view study_uid,
                                    std::string_view study_id = "",
                                    std::string_view study_date = "",
                                    std::string_view study_time = "",
                                    std::string_view accession_number = "",
                                    std::string_view referring_physician = "",
                                    std::string_view study_description = "")
        -> Result<int64_t>;

    /**
     * @brief Insert or update a study record with full details
     *
     * @param record Complete study record (pk field is ignored for insert)
     * @return Result containing the study's primary key or error
     */
    [[nodiscard]] auto upsert_study(const study_record& record)
        -> Result<int64_t>;

    /**
     * @brief Find a study by Study Instance UID
     *
     * @param study_uid The Study Instance UID to search for
     * @return Optional containing the study record if found
     */
    [[nodiscard]] auto find_study(std::string_view study_uid) const
        -> std::optional<study_record>;

    /**
     * @brief Find a study by primary key
     *
     * @param pk The study's primary key
     * @return Optional containing the study record if found
     */
    [[nodiscard]] auto find_study_by_pk(int64_t pk) const
        -> std::optional<study_record>;

    /**
     * @brief List all studies for a patient
     *
     * @param patient_id The patient ID to list studies for
     * @return Result containing vector of study records for the patient or error
     */
    [[nodiscard]] auto list_studies(std::string_view patient_id) const
        -> Result<std::vector<study_record>>;

    /**
     * @brief Search studies with query criteria
     *
     * Supports wildcard matching using '*' character.
     * Can filter by patient attributes, study attributes, and date ranges.
     *
     * @param query Query parameters with optional filters
     * @return Result containing vector of matching study records or error
     */
    [[nodiscard]] auto search_studies(const study_query& query) const
        -> Result<std::vector<study_record>>;

    /**
     * @brief Delete a study by Study Instance UID
     *
     * This operation cascades to delete all related series and instances.
     *
     * @param study_uid The Study Instance UID to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto delete_study(std::string_view study_uid) -> VoidResult;

    /**
     * @brief Get total study count
     *
     * @return Result containing number of studies in the database or error
     */
    [[nodiscard]] auto study_count() const -> Result<size_t>;

    /**
     * @brief Get study count for a specific patient
     *
     * @param patient_id The patient ID
     * @return Result containing number of studies for the patient or error
     */
    [[nodiscard]] auto study_count(std::string_view patient_id) const -> Result<size_t>;

    /**
     * @brief Update modalities in study (denormalized field)
     *
     * Called after series insert/delete to update the modalities_in_study field.
     *
     * @param study_pk The study's primary key
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_modalities_in_study(int64_t study_pk)
        -> VoidResult;

    // ========================================================================
    // Series Operations
    // ========================================================================

    /**
     * @brief Insert or update a series record
     *
     * If a series with the same series_uid exists, updates the record.
     * Otherwise, inserts a new record.
     *
     * @param study_pk Foreign key to the study (required)
     * @param series_uid Series Instance UID (required, max 64 chars)
     * @param modality Modality (e.g., CT, MR)
     * @param series_number Series number
     * @param series_description Series description
     * @param body_part_examined Body part examined
     * @param station_name Station name
     * @return Result containing the series's primary key or error
     */
    [[nodiscard]] auto upsert_series(int64_t study_pk,
                                     std::string_view series_uid,
                                     std::string_view modality = "",
                                     std::optional<int> series_number = std::nullopt,
                                     std::string_view series_description = "",
                                     std::string_view body_part_examined = "",
                                     std::string_view station_name = "")
        -> Result<int64_t>;

    /**
     * @brief Insert or update a series record with full details
     *
     * @param record Complete series record (pk field is ignored for insert)
     * @return Result containing the series's primary key or error
     */
    [[nodiscard]] auto upsert_series(const series_record& record)
        -> Result<int64_t>;

    /**
     * @brief Find a series by Series Instance UID
     *
     * @param series_uid The Series Instance UID to search for
     * @return Optional containing the series record if found
     */
    [[nodiscard]] auto find_series(std::string_view series_uid) const
        -> std::optional<series_record>;

    /**
     * @brief Find a series by primary key
     *
     * @param pk The series's primary key
     * @return Optional containing the series record if found
     */
    [[nodiscard]] auto find_series_by_pk(int64_t pk) const
        -> std::optional<series_record>;

    /**
     * @brief List all series for a study
     *
     * @param study_uid The Study Instance UID to list series for
     * @return Result containing vector of series records for the study or error
     */
    [[nodiscard]] auto list_series(std::string_view study_uid) const
        -> Result<std::vector<series_record>>;

    /**
     * @brief Search series with query criteria
     *
     * Supports wildcard matching using '*' character.
     * Can filter by study UID, modality, and other attributes.
     *
     * @param query Query parameters with optional filters
     * @return Result containing vector of matching series records or error
     */
    [[nodiscard]] auto search_series(const series_query& query) const
        -> Result<std::vector<series_record>>;

    /**
     * @brief Delete a series by Series Instance UID
     *
     * This operation cascades to delete all related instances.
     *
     * @param series_uid The Series Instance UID to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto delete_series(std::string_view series_uid) -> VoidResult;

    /**
     * @brief Get total series count
     *
     * @return Result containing number of series in the database or error
     */
    [[nodiscard]] auto series_count() const -> Result<size_t>;

    /**
     * @brief Get series count for a specific study
     *
     * @param study_uid The Study Instance UID
     * @return Result containing number of series for the study or error
     */
    [[nodiscard]] auto series_count(std::string_view study_uid) const -> Result<size_t>;

    // ========================================================================
    // Instance Operations
    // ========================================================================

    /**
     * @brief Insert or update an instance record
     *
     * If an instance with the same sop_uid exists, updates the record.
     * Otherwise, inserts a new record.
     *
     * @param series_pk Foreign key to the series (required)
     * @param sop_uid SOP Instance UID (required, max 64 chars)
     * @param sop_class_uid SOP Class UID (required)
     * @param file_path Path to the stored DICOM file (required)
     * @param file_size Size of the file in bytes (required, >= 0)
     * @param transfer_syntax Transfer Syntax UID
     * @param instance_number Instance number
     * @return Result containing the instance's primary key or error
     */
    [[nodiscard]] auto upsert_instance(int64_t series_pk,
                                       std::string_view sop_uid,
                                       std::string_view sop_class_uid,
                                       std::string_view file_path,
                                       int64_t file_size,
                                       std::string_view transfer_syntax = "",
                                       std::optional<int> instance_number = std::nullopt)
        -> Result<int64_t>;

    /**
     * @brief Insert or update an instance record with full details
     *
     * @param record Complete instance record (pk field is ignored for insert)
     * @return Result containing the instance's primary key or error
     */
    [[nodiscard]] auto upsert_instance(const instance_record& record)
        -> Result<int64_t>;

    /**
     * @brief Find an instance by SOP Instance UID
     *
     * @param sop_uid The SOP Instance UID to search for
     * @return Optional containing the instance record if found
     */
    [[nodiscard]] auto find_instance(std::string_view sop_uid) const
        -> std::optional<instance_record>;

    /**
     * @brief Find an instance by primary key
     *
     * @param pk The instance's primary key
     * @return Optional containing the instance record if found
     */
    [[nodiscard]] auto find_instance_by_pk(int64_t pk) const
        -> std::optional<instance_record>;

    /**
     * @brief List all instances for a series
     *
     * @param series_uid The Series Instance UID to list instances for
     * @return Result containing vector of instance records for the series or error
     */
    [[nodiscard]] auto list_instances(std::string_view series_uid) const
        -> Result<std::vector<instance_record>>;

    /**
     * @brief Search instances with query criteria
     *
     * Can filter by series UID, SOP class, and other attributes.
     *
     * @param query Query parameters with optional filters
     * @return Result containing vector of matching instance records or error
     */
    [[nodiscard]] auto search_instances(const instance_query& query) const
        -> Result<std::vector<instance_record>>;

    /**
     * @brief Delete an instance by SOP Instance UID
     *
     * @param sop_uid The SOP Instance UID to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto delete_instance(std::string_view sop_uid) -> VoidResult;

    /**
     * @brief Get total instance count
     *
     * @return Result containing number of instances in the database or error
     */
    [[nodiscard]] auto instance_count() const -> Result<size_t>;

    /**
     * @brief Get instance count for a specific series
     *
     * @param series_uid The Series Instance UID
     * @return Result containing number of instances for the series or error
     */
    [[nodiscard]] auto instance_count(std::string_view series_uid) const -> Result<size_t>;

    // ========================================================================
    // MPPS Operations
    // ========================================================================

    /**
     * @brief Create a new MPPS record (N-CREATE)
     *
     * Creates a new MPPS with status "IN PROGRESS". This corresponds to the
     * DICOM N-CREATE operation received from modalities.
     *
     * @param mpps_uid SOP Instance UID for the MPPS (required, must be unique)
     * @param station_ae Performing station AE Title
     * @param modality Modality type (CT, MR, etc.)
     * @param study_uid Related Study Instance UID
     * @param accession_no Accession number
     * @param start_datetime Procedure step start date/time (YYYYMMDDHHMMSS)
     * @return Result containing the MPPS primary key or error
     */
    [[nodiscard]] auto create_mpps(std::string_view mpps_uid,
                                   std::string_view station_ae = "",
                                   std::string_view modality = "",
                                   std::string_view study_uid = "",
                                   std::string_view accession_no = "",
                                   std::string_view start_datetime = "")
        -> Result<int64_t>;

    /**
     * @brief Create a new MPPS record with full details
     *
     * @param record Complete MPPS record (pk field is ignored)
     * @return Result containing the MPPS primary key or error
     */
    [[nodiscard]] auto create_mpps(const mpps_record& record) -> Result<int64_t>;

    /**
     * @brief Update an existing MPPS record (N-SET)
     *
     * Updates the MPPS status and attributes. This corresponds to the
     * DICOM N-SET operation. Status transitions are validated:
     * - IN PROGRESS -> COMPLETED or DISCONTINUED (allowed)
     * - COMPLETED or DISCONTINUED -> any (not allowed, final states)
     *
     * @param mpps_uid SOP Instance UID of the MPPS to update
     * @param new_status New status (COMPLETED or DISCONTINUED)
     * @param end_datetime Procedure step end date/time
     * @param performed_series JSON string of performed series information
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_mpps(std::string_view mpps_uid,
                                   std::string_view new_status,
                                   std::string_view end_datetime = "",
                                   std::string_view performed_series = "")
        -> VoidResult;

    /**
     * @brief Update an existing MPPS record with partial data
     *
     * Only non-empty fields in the record will be updated.
     *
     * @param record MPPS record with fields to update (mpps_uid required)
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_mpps(const mpps_record& record) -> VoidResult;

    /**
     * @brief Find an MPPS by SOP Instance UID
     *
     * @param mpps_uid The MPPS SOP Instance UID to search for
     * @return Optional containing the MPPS record if found
     */
    [[nodiscard]] auto find_mpps(std::string_view mpps_uid) const
        -> std::optional<mpps_record>;

    /**
     * @brief Find an MPPS by primary key
     *
     * @param pk The MPPS primary key
     * @return Optional containing the MPPS record if found
     */
    [[nodiscard]] auto find_mpps_by_pk(int64_t pk) const
        -> std::optional<mpps_record>;

    /**
     * @brief List active (IN PROGRESS) MPPS records for a station
     *
     * @param station_ae The station AE Title to filter by
     * @return Result containing vector of active MPPS records or error
     */
    [[nodiscard]] auto list_active_mpps(std::string_view station_ae) const
        -> Result<std::vector<mpps_record>>;

    /**
     * @brief Find MPPS records by Study Instance UID
     *
     * @param study_uid The Study Instance UID to search for
     * @return Result containing vector of MPPS records associated with the study or error
     */
    [[nodiscard]] auto find_mpps_by_study(std::string_view study_uid) const
        -> Result<std::vector<mpps_record>>;

    /**
     * @brief Search MPPS records with query criteria
     *
     * @param query Query parameters with optional filters
     * @return Result containing vector of matching MPPS records or error
     */
    [[nodiscard]] auto search_mpps(const mpps_query& query) const
        -> Result<std::vector<mpps_record>>;

    /**
     * @brief Delete an MPPS record
     *
     * @param mpps_uid The MPPS SOP Instance UID to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto delete_mpps(std::string_view mpps_uid) -> VoidResult;

    /**
     * @brief Get total MPPS count
     *
     * @return Result containing number of MPPS records in the database or error
     */
    [[nodiscard]] auto mpps_count() const -> Result<size_t>;

    /**
     * @brief Get MPPS count by status
     *
     * @param status The status to count (IN PROGRESS, COMPLETED, DISCONTINUED)
     * @return Result containing number of MPPS records with the given status or error
     */
    [[nodiscard]] auto mpps_count(std::string_view status) const -> Result<size_t>;

    // ========================================================================
    // Worklist Operations
    // ========================================================================

    /**
     * @brief Add a new worklist item
     *
     * Creates a new scheduled procedure step entry for Modality Worklist.
     * The step_status is set to 'SCHEDULED' by default.
     *
     * @param item Worklist item data (pk field is ignored)
     * @return Result containing the worklist primary key or error
     */
    [[nodiscard]] auto add_worklist_item(const worklist_item& item)
        -> Result<int64_t>;

    /**
     * @brief Update worklist item status
     *
     * Called when MPPS is received to update the corresponding worklist
     * item status from SCHEDULED to STARTED or COMPLETED.
     *
     * @param step_id Scheduled Procedure Step ID
     * @param accession_no Accession Number
     * @param new_status New status (STARTED or COMPLETED)
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_worklist_status(std::string_view step_id,
                                              std::string_view accession_no,
                                              std::string_view new_status)
        -> VoidResult;

    /**
     * @brief Query worklist items
     *
     * Returns worklist items matching the query criteria.
     * By default, only returns items with status 'SCHEDULED'.
     * Used for MWL C-FIND operations.
     *
     * @param query Query parameters with optional filters
     * @return Result containing vector of matching worklist items or error
     */
    [[nodiscard]] auto query_worklist(const worklist_query& query) const
        -> Result<std::vector<worklist_item>>;

    /**
     * @brief Find a worklist item by step ID and accession number
     *
     * @param step_id Scheduled Procedure Step ID
     * @param accession_no Accession Number
     * @return Optional containing the worklist item if found
     */
    [[nodiscard]] auto find_worklist_item(std::string_view step_id,
                                          std::string_view accession_no) const
        -> std::optional<worklist_item>;

    /**
     * @brief Find a worklist item by primary key
     *
     * @param pk The worklist primary key
     * @return Optional containing the worklist item if found
     */
    [[nodiscard]] auto find_worklist_by_pk(int64_t pk) const
        -> std::optional<worklist_item>;

    /**
     * @brief Delete a worklist item
     *
     * @param step_id Scheduled Procedure Step ID
     * @param accession_no Accession Number
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto delete_worklist_item(std::string_view step_id,
                                            std::string_view accession_no)
        -> VoidResult;

    /**
     * @brief Cleanup old worklist items
     *
     * Removes worklist items older than the specified age.
     * Only deletes items that are not in SCHEDULED status.
     *
     * @param age Maximum age of items to keep
     * @return Result containing the number of deleted items or error
     */
    [[nodiscard]] auto cleanup_old_worklist_items(std::chrono::hours age)
        -> Result<size_t>;

    /**
     * @brief Cleanup worklist items scheduled before a specific time
     *
     * Removes worklist items with scheduled_datetime before the specified
     * time point. Only deletes items that are not in SCHEDULED status.
     * This provides more precise control than the duration-based cleanup,
     * eliminating timezone conversion ambiguities.
     *
     * @param before Time point; items scheduled before this are eligible for deletion
     * @return Result containing the number of deleted items or error
     *
     * @example
     * @code
     * // Delete items scheduled before 2024-01-01 00:00:00 UTC
     * std::tm tm = {};
     * tm.tm_year = 2024 - 1900;
     * tm.tm_mon = 0;
     * tm.tm_mday = 1;
     * auto time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
     * auto result = db->cleanup_worklist_items_before(time);
     * @endcode
     */
    [[nodiscard]] auto cleanup_worklist_items_before(
        std::chrono::system_clock::time_point before) -> Result<size_t>;

    /**
     * @brief Get total worklist count
     *
     * @return Result containing number of worklist items in the database or error
     */
    [[nodiscard]] auto worklist_count() const -> Result<size_t>;

    /**
     * @brief Get worklist count by status
     *
     * @param status The status to count (SCHEDULED, STARTED, COMPLETED)
     * @return Result containing number of worklist items with the given status or error
     */
    [[nodiscard]] auto worklist_count(std::string_view status) const -> Result<size_t>;

    // ========================================================================
    // Audit Log Operations
    // ========================================================================

    /**
     * @brief Add a new audit log entry
     *
     * Creates a new audit log record for HIPAA compliance and system monitoring.
     *
     * @param record Audit record data (pk field is ignored)
     * @return Result containing the audit log primary key or error
     */
    [[nodiscard]] auto add_audit_log(const audit_record& record)
        -> Result<int64_t>;

    /**
     * @brief Query audit log entries
     *
     * Returns audit log entries matching the query criteria.
     *
     * @param query Query parameters with optional filters
     * @return Result containing vector of matching audit records or error
     */
    [[nodiscard]] auto query_audit_log(const audit_query& query) const
        -> Result<std::vector<audit_record>>;

    /**
     * @brief Find an audit log entry by primary key
     *
     * @param pk The audit log primary key
     * @return Optional containing the audit record if found
     */
    [[nodiscard]] auto find_audit_by_pk(int64_t pk) const
        -> std::optional<audit_record>;

    /**
     * @brief Get total audit log count
     *
     * @return Result containing number of audit log entries in the database or error
     */
    [[nodiscard]] auto audit_count() const -> Result<size_t>;

    /**
     * @brief Cleanup old audit log entries
     *
     * Removes audit log entries older than the specified age.
     *
     * @param age Maximum age of entries to keep
     * @return Result containing the number of deleted entries or error
     */
    [[nodiscard]] auto cleanup_old_audit_logs(std::chrono::hours age)
        -> Result<size_t>;

    // ========================================================================
    // Database Information
    // ========================================================================

    /**
     * @brief Get the database file path
     *
     * @return Path to the database file
     */
    [[nodiscard]] auto path() const -> std::string_view;

    /**
     * @brief Get the current schema version
     *
     * @return Current schema version number
     */
    [[nodiscard]] auto schema_version() const -> int;

    /**
     * @brief Check if the database is open
     *
     * @return true if the database connection is active
     */
    [[nodiscard]] auto is_open() const noexcept -> bool;

    // ========================================================================
    // File Path Lookup Operations
    // ========================================================================

    /**
     * @brief Get file path for a SOP Instance UID
     *
     * Convenience method to quickly look up the file path for a specific
     * DICOM instance without loading the full record.
     *
     * @param sop_instance_uid The SOP Instance UID to look up
     * @return Result containing optional file path (empty if not found) or error
     */
    [[nodiscard]] auto get_file_path(std::string_view sop_instance_uid) const
        -> Result<std::optional<std::string>>;

    /**
     * @brief Get all file paths for a study
     *
     * Returns all DICOM file paths associated with a study.
     * Useful for bulk operations like C-MOVE or study export.
     *
     * @param study_instance_uid The Study Instance UID
     * @return Result containing vector of file paths for all instances in the study or error
     */
    [[nodiscard]] auto get_study_files(std::string_view study_instance_uid) const
        -> Result<std::vector<std::string>>;

    /**
     * @brief Get all file paths for a series
     *
     * Returns all DICOM file paths associated with a series.
     *
     * @param series_instance_uid The Series Instance UID
     * @return Result containing vector of file paths for all instances in the series or error
     */
    [[nodiscard]] auto get_series_files(std::string_view series_instance_uid) const
        -> Result<std::vector<std::string>>;

    // ========================================================================
    // Database Maintenance Operations
    // ========================================================================

    /**
     * @brief Reclaim unused space in the database
     *
     * VACUUM rebuilds the database file, repacking it into a minimal
     * amount of disk space. This can reduce file size after large
     * deletions but may take time for large databases.
     *
     * Note: This operation requires exclusive access and may take
     * significant time for large databases.
     *
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto vacuum() -> VoidResult;

    /**
     * @brief Update database statistics for query optimization
     *
     * ANALYZE collects statistics about tables and indexes, which
     * helps the query planner choose better execution plans.
     * Should be run periodically, especially after bulk insertions.
     *
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto analyze() -> VoidResult;

    /**
     * @brief Verify database integrity
     *
     * Runs SQLite's integrity check to verify the database structure
     * and detect any corruption issues.
     *
     * @return VoidResult indicating success or error with details
     */
    [[nodiscard]] auto verify_integrity() const -> VoidResult;

    /**
     * @brief Get the raw SQLite database handle
     *
     * Returns the underlying SQLite database handle for advanced operations
     * such as creating cursors for streaming queries. The returned handle
     * should not be closed or modified directly.
     *
     * @warning The returned handle is managed by this class. Do not close it.
     * @return Raw SQLite database handle
     */
    [[nodiscard]] auto native_handle() const noexcept -> sqlite3*;

    /**
     * @brief Checkpoint WAL file
     *
     * Forces a WAL checkpoint, writing all WAL content to the main
     * database file. Useful for ensuring durability before backup.
     *
     * @param truncate If true, truncate the WAL file after checkpoint
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto checkpoint(bool truncate = false) -> VoidResult;

    // ========================================================================
    // Storage Statistics
    // ========================================================================

    /**
     * @brief Storage statistics structure
     */
    struct storage_stats {
        size_t total_patients = 0;     ///< Total number of patients
        size_t total_studies = 0;      ///< Total number of studies
        size_t total_series = 0;       ///< Total number of series
        size_t total_instances = 0;    ///< Total number of instances
        int64_t total_file_size = 0;   ///< Total size of all files in bytes
        int64_t database_size = 0;     ///< Size of the database file in bytes
    };

    /**
     * @brief Get storage statistics
     *
     * Returns aggregate statistics about the database contents.
     *
     * @return Result containing storage statistics structure or error
     */
    [[nodiscard]] auto get_storage_stats() const -> Result<storage_stats>;

private:
    /**
     * @brief Private constructor - use open() factory method
     */
    explicit index_database(sqlite3* db, std::string path);

    /**
     * @brief Parse a patient record from a prepared statement
     */
    [[nodiscard]] auto parse_patient_row(void* stmt) const -> patient_record;

    /**
     * @brief Parse a study record from a prepared statement
     */
    [[nodiscard]] auto parse_study_row(void* stmt) const -> study_record;

    /**
     * @brief Parse a series record from a prepared statement
     */
    [[nodiscard]] auto parse_series_row(void* stmt) const -> series_record;

    /**
     * @brief Parse an instance record from a prepared statement
     */
    [[nodiscard]] auto parse_instance_row(void* stmt) const -> instance_record;

    /**
     * @brief Parse an MPPS record from a prepared statement
     */
    [[nodiscard]] auto parse_mpps_row(void* stmt) const -> mpps_record;

    /**
     * @brief Parse a worklist record from a prepared statement
     */
    [[nodiscard]] auto parse_worklist_row(void* stmt) const -> worklist_item;

    /**
     * @brief Parse an audit record from a prepared statement
     */
    [[nodiscard]] auto parse_audit_row(void* stmt) const -> audit_record;

    /**
     * @brief Convert wildcard pattern to SQL LIKE pattern
     *
     * Converts '*' to '%' for SQL LIKE matching
     */
    [[nodiscard]] static auto to_like_pattern(std::string_view pattern)
        -> std::string;

#ifdef PACS_WITH_DATABASE_SYSTEM
    /// Database context for database_system
    std::shared_ptr<database::database_context> db_context_;

    /// Database manager for database_system queries
    /// Marked mutable to allow use in const member functions (queries are read-only)
    mutable std::shared_ptr<database::database_manager> db_manager_;

    /**
     * @brief Initialize database_system connection
     */
    [[nodiscard]] auto initialize_database_system() -> VoidResult;

    /**
     * @brief Parse patient record from database_system result row
     */
    [[nodiscard]] auto parse_patient_from_row(
        const std::map<std::string, database::database_value>& row) const
        -> patient_record;
#endif

    /// SQLite database handle (used for migrations and fallback)
    sqlite3* db_{nullptr};

    /// Database file path
    std::string path_;

    /// Migration runner for schema management
    migration_runner migration_runner_;
};

}  // namespace pacs::storage
