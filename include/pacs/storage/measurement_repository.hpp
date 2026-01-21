/**
 * @file measurement_repository.hpp
 * @brief Repository for measurement persistence
 *
 * This file provides the measurement_repository class for persisting measurement
 * records in the SQLite database. Supports CRUD operations and search.
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #581 - Part 1: Data Models and Repositories
 */

#pragma once

#include "pacs/storage/measurement_record.hpp"

#include <kcenon/common/patterns/result.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace pacs::storage {

template <typename T>
using Result = kcenon::common::Result<T>;

using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Repository for measurement persistence
 *
 * Provides database operations for storing and retrieving measurement records.
 * Uses SQLite for persistence.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto repo = measurement_repository(db_handle);
 *
 * measurement_record meas;
 * meas.measurement_id = generate_uuid();
 * meas.sop_instance_uid = "1.2.3.4.5.6";
 * meas.type = measurement_type::length;
 * meas.value = 45.5;
 * meas.unit = "mm";
 * repo.save(meas);
 *
 * auto found = repo.find_by_id(meas.measurement_id);
 * @endcode
 */
class measurement_repository {
public:
    explicit measurement_repository(sqlite3* db);
    ~measurement_repository();

    measurement_repository(const measurement_repository&) = delete;
    auto operator=(const measurement_repository&) -> measurement_repository& = delete;
    measurement_repository(measurement_repository&&) noexcept;
    auto operator=(measurement_repository&&) noexcept -> measurement_repository&;

    /**
     * @brief Save a measurement record
     *
     * If the measurement already exists (by measurement_id), updates it.
     * Otherwise, inserts a new record.
     *
     * @param record The measurement record to save
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto save(const measurement_record& record) -> VoidResult;

    /**
     * @brief Find a measurement by its unique ID
     *
     * @param measurement_id The measurement ID to search for
     * @return Optional containing the measurement if found
     */
    [[nodiscard]] auto find_by_id(std::string_view measurement_id) const
        -> std::optional<measurement_record>;

    /**
     * @brief Find a measurement by primary key
     *
     * @param pk The primary key
     * @return Optional containing the measurement if found
     */
    [[nodiscard]] auto find_by_pk(int64_t pk) const
        -> std::optional<measurement_record>;

    /**
     * @brief Find measurements by SOP Instance UID
     *
     * @param sop_instance_uid The SOP Instance UID to filter by
     * @return Vector of measurements on the specified instance
     */
    [[nodiscard]] auto find_by_instance(std::string_view sop_instance_uid) const
        -> std::vector<measurement_record>;

    /**
     * @brief Search measurements with query options
     *
     * @param query Query options for filtering and pagination
     * @return Vector of matching measurements
     */
    [[nodiscard]] auto search(const measurement_query& query) const
        -> std::vector<measurement_record>;

    /**
     * @brief Delete a measurement by ID
     *
     * @param measurement_id The measurement ID to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto remove(std::string_view measurement_id) -> VoidResult;

    /**
     * @brief Check if a measurement exists
     *
     * @param measurement_id The measurement ID to check
     * @return true if the measurement exists
     */
    [[nodiscard]] auto exists(std::string_view measurement_id) const -> bool;

    /**
     * @brief Get total measurement count
     *
     * @return Number of measurements in the repository
     */
    [[nodiscard]] auto count() const -> size_t;

    /**
     * @brief Count measurements matching a query
     *
     * @param query Query options for filtering
     * @return Number of matching measurements
     */
    [[nodiscard]] auto count(const measurement_query& query) const -> size_t;

    /**
     * @brief Check if the database connection is valid
     *
     * @return true if the database handle is valid
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool;

private:
    [[nodiscard]] auto parse_row(void* stmt) const -> measurement_record;

    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage
