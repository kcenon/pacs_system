// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file audit_repository.h
 * @brief Repository for audit log persistence
 *
 * Persists ATNA-style audit records used for regulatory traceability.
 * Supports append, structured query, and retention-based cleanup in a
 * single shared table.
 *
 * @see Issue #915 - Part 4: Extract UPS and audit lifecycle repositories
 * @see IHE ATNA - Audit Trail and Node Authentication
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "kcenon/pacs/storage/audit_record.h"

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
 * @brief Persistence for ATNA audit log records
 *
 * Backed by the database_system abstraction. Audit entries are
 * append-only at the API level; updates are not supported to preserve
 * the integrity of the trail.
 *
 * **Thread Safety:** Not thread-safe. External synchronization is required.
 */
class audit_repository : public base_repository<audit_record, int64_t> {
public:
    /**
     * @brief Construct an audit repository bound to a database adapter
     * @param db Shared database adapter; must be connected before calls
     */
    explicit audit_repository(std::shared_ptr<pacs_database_adapter> db);
    ~audit_repository() override = default;

    audit_repository(const audit_repository&) = delete;
    auto operator=(const audit_repository&) -> audit_repository& = delete;
    audit_repository(audit_repository&&) noexcept = default;
    auto operator=(audit_repository&&) noexcept -> audit_repository& = default;

    /**
     * @brief Append a new audit record
     * @param record Fully populated audit record
     * @return Result with the database primary key, or error
     */
    [[nodiscard]] auto add_audit_log(const audit_record& record)
        -> Result<int64_t>;

    /**
     * @brief Query audit records using a structured filter
     *
     * Supports filtering by event ID, user AE/ID, object UID, outcome,
     * and a time range.
     *
     * @param query Structured query parameters
     * @return Result containing matching records or a database error
     */
    [[nodiscard]] auto query_audit_log(const audit_query& query)
        -> Result<std::vector<audit_record>>;

    /**
     * @brief Look up a single audit record by primary key
     * @param pk Database primary key
     * @return Record if present, std::nullopt otherwise
     */
    [[nodiscard]] auto find_audit_by_pk(int64_t pk)
        -> std::optional<audit_record>;

    /**
     * @brief Total number of audit records currently stored
     * @return Result containing the row count
     */
    [[nodiscard]] auto audit_count() -> Result<size_t>;

    /**
     * @brief Delete audit records older than the specified age
     *
     * Enforces data retention policy. Age is measured from the record
     * event timestamp.
     *
     * @param age Maximum retention window
     * @return Result with the number of rows removed
     */
    [[nodiscard]] auto cleanup_old_audit_logs(std::chrono::hours age)
        -> Result<size_t>;

protected:
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> audit_record override;
    [[nodiscard]] auto entity_to_row(const audit_record& entity) const
        -> std::map<std::string, database_value> override;
    [[nodiscard]] auto get_pk(const audit_record& entity) const
        -> int64_t override;
    [[nodiscard]] auto has_pk(const audit_record& entity) const
        -> bool override;
    [[nodiscard]] auto select_columns() const -> std::vector<std::string> override;

private:
    [[nodiscard]] static auto to_like_pattern(std::string_view pattern)
        -> std::string;
    [[nodiscard]] auto parse_timestamp(const std::string& str) const
        -> std::chrono::system_clock::time_point;
    [[nodiscard]] auto format_timestamp(
        std::chrono::system_clock::time_point tp) const -> std::string;
};

}  // namespace kcenon::pacs::storage

#else

struct sqlite3;

namespace kcenon::pacs::storage {

template <typename T>
using Result = kcenon::common::Result<T>;

using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Legacy direct-SQLite fallback when database_system is disabled
 *
 * Exposes the same API surface as the database_system-backed variant.
 * Used only when PACS_WITH_DATABASE_SYSTEM is not defined at build time.
 */
class audit_repository {
public:
    /**
     * @brief Construct repository bound to a raw sqlite3 handle
     * @param db Non-owning pointer to an open sqlite3 connection
     */
    explicit audit_repository(sqlite3* db);
    ~audit_repository();

    audit_repository(const audit_repository&) = delete;
    auto operator=(const audit_repository&) -> audit_repository& = delete;
    audit_repository(audit_repository&&) noexcept;
    auto operator=(audit_repository&&) noexcept -> audit_repository&;

    /// @copydoc kcenon::pacs::storage::audit_repository::add_audit_log
    [[nodiscard]] auto add_audit_log(const audit_record& record)
        -> Result<int64_t>;
    /// @copydoc kcenon::pacs::storage::audit_repository::query_audit_log
    [[nodiscard]] auto query_audit_log(const audit_query& query) const
        -> Result<std::vector<audit_record>>;
    /// @copydoc kcenon::pacs::storage::audit_repository::find_audit_by_pk
    [[nodiscard]] auto find_audit_by_pk(int64_t pk) const
        -> std::optional<audit_record>;
    /// @copydoc kcenon::pacs::storage::audit_repository::audit_count
    [[nodiscard]] auto audit_count() const -> Result<size_t>;
    /// @copydoc kcenon::pacs::storage::audit_repository::cleanup_old_audit_logs
    [[nodiscard]] auto cleanup_old_audit_logs(std::chrono::hours age)
        -> Result<size_t>;

private:
    [[nodiscard]] auto parse_audit_row(void* stmt) const -> audit_record;
    [[nodiscard]] static auto to_like_pattern(std::string_view pattern)
        -> std::string;
    [[nodiscard]] static auto parse_timestamp(const std::string& str)
        -> std::chrono::system_clock::time_point;

    sqlite3* db_{nullptr};
};

}  // namespace kcenon::pacs::storage

#endif
