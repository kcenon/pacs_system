/**
 * @file index_database.hpp
 * @brief PACS index database for metadata storage and retrieval
 *
 * This file provides the index_database class for managing DICOM metadata
 * in a SQLite database. Supports CRUD operations for patients, studies,
 * series, and instances.
 *
 * @see SRS-STOR-003, FR-4.2
 */

#pragma once

#include "migration_runner.hpp"
#include "patient_record.hpp"
#include "study_record.hpp"

#include <kcenon/common/patterns/result.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Forward declaration of SQLite handle
struct sqlite3;

namespace pacs::storage {

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
     * @brief Open or create a database
     *
     * Opens an existing database or creates a new one at the specified path.
     * Automatically runs pending migrations.
     *
     * @param db_path Path to the database file, or ":memory:" for in-memory DB
     * @return Result containing the database instance or error
     */
    [[nodiscard]] static auto open(std::string_view db_path)
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
     * @return Vector of matching patient records
     */
    [[nodiscard]] auto search_patients(const patient_query& query) const
        -> std::vector<patient_record>;

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
     * @return Number of patients in the database
     */
    [[nodiscard]] auto patient_count() const -> size_t;

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
     * @return Vector of study records for the patient
     */
    [[nodiscard]] auto list_studies(std::string_view patient_id) const
        -> std::vector<study_record>;

    /**
     * @brief Search studies with query criteria
     *
     * Supports wildcard matching using '*' character.
     * Can filter by patient attributes, study attributes, and date ranges.
     *
     * @param query Query parameters with optional filters
     * @return Vector of matching study records
     */
    [[nodiscard]] auto search_studies(const study_query& query) const
        -> std::vector<study_record>;

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
     * @return Number of studies in the database
     */
    [[nodiscard]] auto study_count() const -> size_t;

    /**
     * @brief Get study count for a specific patient
     *
     * @param patient_id The patient ID
     * @return Number of studies for the patient
     */
    [[nodiscard]] auto study_count(std::string_view patient_id) const -> size_t;

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
     * @brief Convert wildcard pattern to SQL LIKE pattern
     *
     * Converts '*' to '%' for SQL LIKE matching
     */
    [[nodiscard]] static auto to_like_pattern(std::string_view pattern)
        -> std::string;

    /// SQLite database handle
    sqlite3* db_{nullptr};

    /// Database file path
    std::string path_;

    /// Migration runner for schema management
    migration_runner migration_runner_;
};

}  // namespace pacs::storage
