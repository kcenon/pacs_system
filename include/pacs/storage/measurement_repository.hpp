/**
 * @file measurement_repository.hpp
 * @brief Repository for measurement persistence using base_repository pattern
 *
 * This file provides the measurement_repository class for persisting measurement
 * records using the base_repository pattern. Supports CRUD operations and search.
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #581 - Part 1: Data Models and Repositories
 * @see Issue #610 - Phase 4: Repository Migrations
 */

#pragma once

#include "pacs/storage/measurement_record.hpp"

#include <kcenon/common/patterns/result.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/storage/base_repository.hpp"

namespace pacs::storage {

/**
 * @brief Repository for measurement persistence using base_repository pattern
 *
 * Provides database operations for storing and retrieving measurement records.
 * Extends base_repository to inherit standard CRUD operations.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto db = std::make_shared<pacs_database_adapter>("pacs.db");
 * db->connect();
 * auto repo = measurement_repository(db);
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
class measurement_repository
    : public base_repository<measurement_record, std::string> {
public:
    explicit measurement_repository(std::shared_ptr<pacs_database_adapter> db);
    ~measurement_repository() override = default;

    measurement_repository(const measurement_repository&) = delete;
    auto operator=(const measurement_repository&)
        -> measurement_repository& = delete;
    measurement_repository(measurement_repository&&) noexcept = default;
    auto operator=(measurement_repository&&) noexcept
        -> measurement_repository& = default;

    // Bring base class count() into scope
    using base_repository::count;

    // ========================================================================
    // Domain-Specific Operations
    // ========================================================================

    /**
     * @brief Find a measurement by primary key (integer pk)
     *
     * @param pk The primary key (integer)
     * @return Result containing the measurement if found, or error
     */
    [[nodiscard]] auto find_by_pk(int64_t pk) -> result_type;

    /**
     * @brief Find measurements by SOP Instance UID
     *
     * @param sop_instance_uid The SOP Instance UID to filter by
     * @return Result containing vector of measurements on the specified instance
     */
    [[nodiscard]] auto find_by_instance(std::string_view sop_instance_uid)
        -> list_result_type;

    /**
     * @brief Search measurements with query options
     *
     * @param query Query options for filtering and pagination
     * @return Result containing vector of matching measurements or error
     */
    [[nodiscard]] auto search(const measurement_query& query)
        -> list_result_type;

    /**
     * @brief Count measurements matching a query
     *
     * @param query Query options for filtering
     * @return Result containing number of matching measurements or error
     */
    [[nodiscard]] auto count(const measurement_query& query) -> Result<size_t>;

protected:
    // ========================================================================
    // base_repository overrides
    // ========================================================================

    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> measurement_record override;

    [[nodiscard]] auto entity_to_row(const measurement_record& entity) const
        -> std::map<std::string, database_value> override;

    [[nodiscard]] auto get_pk(const measurement_record& entity) const
        -> std::string override;

    [[nodiscard]] auto has_pk(const measurement_record& entity) const
        -> bool override;

    [[nodiscard]] auto select_columns() const
        -> std::vector<std::string> override;

private:
    [[nodiscard]] auto parse_timestamp(const std::string& str) const
        -> std::chrono::system_clock::time_point;

    [[nodiscard]] auto format_timestamp(
        std::chrono::system_clock::time_point tp) const -> std::string;
};

}  // namespace pacs::storage

#else  // !PACS_WITH_DATABASE_SYSTEM

// Legacy interface for builds without database_system
struct sqlite3;

namespace pacs::storage {

template <typename T>
using Result = kcenon::common::Result<T>;

using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Repository for measurement persistence (legacy SQLite interface)
 *
 * This is the legacy interface maintained for builds without database_system.
 * New code should use the base_repository version when PACS_WITH_DATABASE_SYSTEM
 * is defined.
 */
class measurement_repository {
public:
    explicit measurement_repository(sqlite3* db);
    ~measurement_repository();

    measurement_repository(const measurement_repository&) = delete;
    auto operator=(const measurement_repository&)
        -> measurement_repository& = delete;
    measurement_repository(measurement_repository&&) noexcept;
    auto operator=(measurement_repository&&) noexcept
        -> measurement_repository&;

    [[nodiscard]] auto save(const measurement_record& record) -> VoidResult;
    [[nodiscard]] auto find_by_id(std::string_view measurement_id) const
        -> std::optional<measurement_record>;
    [[nodiscard]] auto find_by_pk(int64_t pk) const
        -> std::optional<measurement_record>;
    [[nodiscard]] auto find_by_instance(std::string_view sop_instance_uid) const
        -> std::vector<measurement_record>;
    [[nodiscard]] auto search(const measurement_query& query) const
        -> std::vector<measurement_record>;
    [[nodiscard]] auto remove(std::string_view measurement_id) -> VoidResult;
    [[nodiscard]] auto exists(std::string_view measurement_id) const -> bool;
    [[nodiscard]] auto count() const -> size_t;
    [[nodiscard]] auto count(const measurement_query& query) const -> size_t;
    [[nodiscard]] auto is_valid() const noexcept -> bool;

private:
    [[nodiscard]] auto parse_row(void* stmt) const -> measurement_record;

    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
