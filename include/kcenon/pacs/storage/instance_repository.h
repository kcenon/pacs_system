// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file instance_repository.h
 * @brief Repository for instance metadata persistence using base_repository pattern
 *
 * Extracted from index_database as part of the storage decomposition (Issue #896).
 * Provides focused CRUD operations and file path lookups for instance records.
 *
 * @see Issue #896 - Decompose index_database into metadata and lifecycle stores
 * @see Issue #913 - Part 2: Extract series and instance metadata repositories
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "kcenon/pacs/storage/instance_record.h"

#include <kcenon/common/patterns/result.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "kcenon/pacs/storage/base_repository.h"

namespace kcenon::pacs::storage {

/**
 * @brief Repository for instance metadata persistence using base_repository pattern
 */
class instance_repository : public base_repository<instance_record, int64_t> {
public:
    /**
     * @brief Construct an instance repository with the given database adapter.
     * @param db Shared pointer to the database adapter used for persistence
     */
    explicit instance_repository(std::shared_ptr<pacs_database_adapter> db);
    ~instance_repository() override = default;

    instance_repository(const instance_repository&) = delete;
    auto operator=(const instance_repository&) -> instance_repository& = delete;
    instance_repository(instance_repository&&) noexcept = default;
    auto operator=(instance_repository&&) noexcept -> instance_repository& = default;

    /**
     * @brief Insert or update an instance record by individual fields.
     * @param series_pk Primary key of the parent series
     * @param sop_uid SOP Instance UID (DICOM tag 0008,0018)
     * @param sop_class_uid SOP Class UID (DICOM tag 0008,0016)
     * @param file_path File system path where the instance is stored
     * @param file_size Size of the stored file in bytes
     * @param transfer_syntax Transfer Syntax UID (DICOM tag 0002,0010), empty if unknown
     * @param instance_number Instance Number (DICOM tag 0020,0013), nullopt if absent
     * @return Result containing the primary key of the upserted record, or an error
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
     * @brief Insert or update an instance record from an existing record struct.
     * @param record The instance record to upsert
     * @return Result containing the primary key of the upserted record, or an error
     */
    [[nodiscard]] auto upsert_instance(const instance_record& record)
        -> Result<int64_t>;

    /**
     * @brief Find an instance record by its SOP Instance UID.
     * @param sop_uid SOP Instance UID to search for
     * @return The matching instance record, or std::nullopt if not found
     */
    [[nodiscard]] auto find_instance(std::string_view sop_uid)
        -> std::optional<instance_record>;

    /**
     * @brief Find an instance record by its database primary key.
     * @param pk Primary key of the instance record
     * @return The matching instance record, or std::nullopt if not found
     */
    [[nodiscard]] auto find_instance_by_pk(int64_t pk)
        -> std::optional<instance_record>;

    /**
     * @brief List all instance records belonging to a given series.
     * @param series_uid Series Instance UID to filter by
     * @return Result containing a vector of matching instance records, or an error
     */
    [[nodiscard]] auto list_instances(std::string_view series_uid)
        -> Result<std::vector<instance_record>>;

    /**
     * @brief Search for instance records matching the given query criteria.
     * @param query Query parameters including optional filters and pagination
     * @return Result containing a vector of matching instance records, or an error
     */
    [[nodiscard]] auto search_instances(const instance_query& query)
        -> Result<std::vector<instance_record>>;

    /**
     * @brief Delete an instance record by its SOP Instance UID.
     * @param sop_uid SOP Instance UID of the instance to delete
     * @return VoidResult indicating success or an error
     */
    [[nodiscard]] auto delete_instance(std::string_view sop_uid)
        -> VoidResult;

    /**
     * @brief Get the total number of instance records in the repository.
     * @return Result containing the total instance count, or an error
     */
    [[nodiscard]] auto instance_count() -> Result<size_t>;

    /**
     * @brief Get the number of instance records belonging to a given series.
     * @param series_uid Series Instance UID to count instances for
     * @return Result containing the instance count for the series, or an error
     */
    [[nodiscard]] auto instance_count(std::string_view series_uid)
        -> Result<size_t>;

    /**
     * @brief Retrieve the file system path for a stored instance.
     * @param sop_instance_uid SOP Instance UID to look up
     * @return Result containing the file path if found, std::nullopt if the
     *         instance exists but has no file, or an error
     */
    [[nodiscard]] auto get_file_path(std::string_view sop_instance_uid)
        -> Result<std::optional<std::string>>;

    /**
     * @brief Retrieve all file paths for instances belonging to a study.
     * @param study_instance_uid Study Instance UID to look up
     * @return Result containing a vector of file paths, or an error
     */
    [[nodiscard]] auto get_study_files(std::string_view study_instance_uid)
        -> Result<std::vector<std::string>>;

    /**
     * @brief Retrieve all file paths for instances belonging to a series.
     * @param series_instance_uid Series Instance UID to look up
     * @return Result containing a vector of file paths, or an error
     */
    [[nodiscard]] auto get_series_files(std::string_view series_instance_uid)
        -> Result<std::vector<std::string>>;

protected:
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> instance_record override;
    [[nodiscard]] auto entity_to_row(const instance_record& entity) const
        -> std::map<std::string, database_value> override;
    [[nodiscard]] auto get_pk(const instance_record& entity) const
        -> int64_t override;
    [[nodiscard]] auto has_pk(const instance_record& entity) const
        -> bool override;
    [[nodiscard]] auto select_columns() const -> std::vector<std::string> override;

private:
    [[nodiscard]] auto parse_timestamp(const std::string& str) const
        -> std::chrono::system_clock::time_point;
    [[nodiscard]] auto format_timestamp(
        std::chrono::system_clock::time_point tp) const -> std::string;
};

}  // namespace kcenon::pacs::storage

#else  // !PACS_WITH_DATABASE_SYSTEM

struct sqlite3;

namespace kcenon::pacs::storage {

template <typename T>
using Result = kcenon::common::Result<T>;

using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Repository for instance metadata persistence (legacy SQLite interface)
 */
class instance_repository {
public:
    /**
     * @brief Construct an instance repository with a raw SQLite database handle.
     * @param db Pointer to the SQLite database connection
     */
    explicit instance_repository(sqlite3* db);
    ~instance_repository();

    instance_repository(const instance_repository&) = delete;
    auto operator=(const instance_repository&) -> instance_repository& = delete;
    instance_repository(instance_repository&&) noexcept;
    auto operator=(instance_repository&&) noexcept -> instance_repository&;

    /**
     * @brief Insert or update an instance record by individual fields.
     * @param series_pk Primary key of the parent series
     * @param sop_uid SOP Instance UID (DICOM tag 0008,0018)
     * @param sop_class_uid SOP Class UID (DICOM tag 0008,0016)
     * @param file_path File system path where the instance is stored
     * @param file_size Size of the stored file in bytes
     * @param transfer_syntax Transfer Syntax UID (DICOM tag 0002,0010), empty if unknown
     * @param instance_number Instance Number (DICOM tag 0020,0013), nullopt if absent
     * @return Result containing the primary key of the upserted record, or an error
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
     * @brief Insert or update an instance record from an existing record struct.
     * @param record The instance record to upsert
     * @return Result containing the primary key of the upserted record, or an error
     */
    [[nodiscard]] auto upsert_instance(const instance_record& record)
        -> Result<int64_t>;

    /**
     * @brief Find an instance record by its SOP Instance UID.
     * @param sop_uid SOP Instance UID to search for
     * @return The matching instance record, or std::nullopt if not found
     */
    [[nodiscard]] auto find_instance(std::string_view sop_uid) const
        -> std::optional<instance_record>;

    /**
     * @brief Find an instance record by its database primary key.
     * @param pk Primary key of the instance record
     * @return The matching instance record, or std::nullopt if not found
     */
    [[nodiscard]] auto find_instance_by_pk(int64_t pk) const
        -> std::optional<instance_record>;

    /**
     * @brief List all instance records belonging to a given series.
     * @param series_uid Series Instance UID to filter by
     * @return Result containing a vector of matching instance records, or an error
     */
    [[nodiscard]] auto list_instances(std::string_view series_uid) const
        -> Result<std::vector<instance_record>>;

    /**
     * @brief Search for instance records matching the given query criteria.
     * @param query Query parameters including optional filters and pagination
     * @return Result containing a vector of matching instance records, or an error
     */
    [[nodiscard]] auto search_instances(const instance_query& query) const
        -> Result<std::vector<instance_record>>;

    /**
     * @brief Delete an instance record by its SOP Instance UID.
     * @param sop_uid SOP Instance UID of the instance to delete
     * @return VoidResult indicating success or an error
     */
    [[nodiscard]] auto delete_instance(std::string_view sop_uid)
        -> VoidResult;

    /**
     * @brief Get the total number of instance records in the repository.
     * @return Result containing the total instance count, or an error
     */
    [[nodiscard]] auto instance_count() const -> Result<size_t>;

    /**
     * @brief Get the number of instance records belonging to a given series.
     * @param series_uid Series Instance UID to count instances for
     * @return Result containing the instance count for the series, or an error
     */
    [[nodiscard]] auto instance_count(std::string_view series_uid) const
        -> Result<size_t>;

    /**
     * @brief Retrieve the file system path for a stored instance.
     * @param sop_instance_uid SOP Instance UID to look up
     * @return Result containing the file path if found, std::nullopt if the
     *         instance exists but has no file, or an error
     */
    [[nodiscard]] auto get_file_path(std::string_view sop_instance_uid) const
        -> Result<std::optional<std::string>>;

    /**
     * @brief Retrieve all file paths for instances belonging to a study.
     * @param study_instance_uid Study Instance UID to look up
     * @return Result containing a vector of file paths, or an error
     */
    [[nodiscard]] auto get_study_files(std::string_view study_instance_uid) const
        -> Result<std::vector<std::string>>;

    /**
     * @brief Retrieve all file paths for instances belonging to a series.
     * @param series_instance_uid Series Instance UID to look up
     * @return Result containing a vector of file paths, or an error
     */
    [[nodiscard]] auto get_series_files(std::string_view series_instance_uid) const
        -> Result<std::vector<std::string>>;

private:
    [[nodiscard]] auto parse_instance_row(void* stmt) const -> instance_record;
    [[nodiscard]] static auto parse_timestamp(const std::string& str)
        -> std::chrono::system_clock::time_point;

    sqlite3* db_{nullptr};
};

}  // namespace kcenon::pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
