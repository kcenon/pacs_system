/**
 * @file migration_runner.hpp
 * @brief Database schema migration runner
 *
 * This file provides the migration_runner class for managing database
 * schema evolution through versioned migrations.
 *
 * When compiled with PACS_WITH_DATABASE_SYSTEM, uses pacs_database_adapter
 * for consistent database abstraction. Otherwise, uses direct SQLite
 * prepared statements.
 *
 * @see SRS-STOR-003
 * @see Issue #643 - Update migration_runner to use pacs_database_adapter
 */

#pragma once

#include "migration_record.hpp"

#include <kcenon/common/patterns/result.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// Forward declaration for pacs_database_adapter
#ifdef PACS_WITH_DATABASE_SYSTEM
namespace pacs::storage {
class pacs_database_adapter;
}
#endif

// Forward declaration of SQLite handle
struct sqlite3;

namespace pacs::storage {

/// Result type alias for void operations
using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Function type for migration implementations (SQLite)
 *
 * Each migration function receives a database handle and should execute
 * the necessary SQL to upgrade the schema to its target version.
 *
 * @param db The SQLite database handle
 * @return VoidResult Success or error information
 */
using migration_function = std::function<VoidResult(sqlite3* db)>;

#ifdef PACS_WITH_DATABASE_SYSTEM
/**
 * @brief Function type for migration implementations (pacs_database_adapter)
 *
 * Each migration function receives a database adapter and should execute
 * the necessary SQL to upgrade the schema to its target version.
 *
 * @param db The pacs_database_adapter instance
 * @return VoidResult Success or error information
 */
using adapter_migration_function =
    std::function<VoidResult(pacs_database_adapter&)>;
#endif

/**
 * @brief Manages database schema migrations
 *
 * The migration_runner is responsible for:
 * - Tracking the current schema version via schema_version table
 * - Applying pending migrations in order
 * - Ensuring atomic migrations using transactions
 * - Rolling back failed migrations
 *
 * Thread Safety: This class is NOT thread-safe. External synchronization
 * is required for concurrent access to the same database.
 *
 * @example
 * @code
 * sqlite3* db = ...;
 * migration_runner runner;
 *
 * // Check if migration is needed
 * if (runner.needs_migration(db)) {
 *     auto result = runner.run_migrations(db);
 *     if (result.is_err()) {
 *         // Handle error
 *     }
 * }
 *
 * // Get current version
 * int version = runner.get_current_version(db);
 * @endcode
 */
class migration_runner {
public:
    /**
     * @brief Default constructor
     */
    migration_runner();

    /**
     * @brief Default destructor
     */
    ~migration_runner() = default;

    // Non-copyable, non-movable (stateless service)
    migration_runner(const migration_runner&) = delete;
    auto operator=(const migration_runner&) -> migration_runner& = delete;
    migration_runner(migration_runner&&) = delete;
    auto operator=(migration_runner&&) -> migration_runner& = delete;

    // ========================================================================
    // Migration Operations
    // ========================================================================

    /**
     * @brief Run all pending migrations
     *
     * Executes all migrations from the current version up to LATEST_VERSION.
     * Each migration is run within a transaction for atomicity.
     *
     * @param db The SQLite database handle
     * @return VoidResult Success or error information
     *
     * @note If any migration fails, the database will be rolled back to its
     *       state before that migration started.
     */
    [[nodiscard]] auto run_migrations(sqlite3* db) -> VoidResult;

    /**
     * @brief Run migrations up to a specific version
     *
     * @param db The SQLite database handle
     * @param target_version The version to migrate to
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto run_migrations_to(sqlite3* db, int target_version)
        -> VoidResult;

#ifdef PACS_WITH_DATABASE_SYSTEM
    // ========================================================================
    // Migration Operations (pacs_database_adapter)
    // ========================================================================

    /**
     * @brief Run all pending migrations using pacs_database_adapter
     *
     * Executes all migrations from the current version up to LATEST_VERSION.
     * Each migration is run within a transaction for atomicity.
     *
     * @param db The pacs_database_adapter instance
     * @return VoidResult Success or error information
     *
     * @note If any migration fails, the database will be rolled back to its
     *       state before that migration started.
     */
    [[nodiscard]] auto run_migrations(pacs_database_adapter& db) -> VoidResult;

    /**
     * @brief Run migrations up to a specific version using pacs_database_adapter
     *
     * @param db The pacs_database_adapter instance
     * @param target_version The version to migrate to
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto run_migrations_to(pacs_database_adapter& db,
                                         int target_version) -> VoidResult;

    /**
     * @brief Get the current schema version using pacs_database_adapter
     *
     * @param db The pacs_database_adapter instance
     * @return Current schema version number
     */
    [[nodiscard]] auto get_current_version(pacs_database_adapter& db) const
        -> int;

    /**
     * @brief Check if migration is needed using pacs_database_adapter
     *
     * @param db The pacs_database_adapter instance
     * @return true if current version is less than LATEST_VERSION
     */
    [[nodiscard]] auto needs_migration(pacs_database_adapter& db) const -> bool;

    /**
     * @brief Get the migration history using pacs_database_adapter
     *
     * @param db The pacs_database_adapter instance
     * @return Vector of migration records
     */
    [[nodiscard]] auto get_history(pacs_database_adapter& db) const
        -> std::vector<migration_record>;
#endif

    // ========================================================================
    // Version Information
    // ========================================================================

    /**
     * @brief Get the current schema version
     *
     * Returns 0 if no migrations have been applied (schema_version table
     * doesn't exist or is empty).
     *
     * @param db The SQLite database handle
     * @return Current schema version number
     */
    [[nodiscard]] auto get_current_version(sqlite3* db) const -> int;

    /**
     * @brief Get the latest available schema version
     *
     * @return The highest version number available for migration
     */
    [[nodiscard]] auto get_latest_version() const noexcept -> int;

    /**
     * @brief Check if migration is needed
     *
     * @param db The SQLite database handle
     * @return true if current version is less than LATEST_VERSION
     */
    [[nodiscard]] auto needs_migration(sqlite3* db) const -> bool;

    // ========================================================================
    // Migration History
    // ========================================================================

    /**
     * @brief Get the migration history
     *
     * Returns all applied migrations in chronological order.
     *
     * @param db The SQLite database handle
     * @return Vector of migration records, empty if no migrations applied
     */
    [[nodiscard]] auto get_history(sqlite3* db) const
        -> std::vector<migration_record>;

private:
    // ========================================================================
    // Internal Implementation
    // ========================================================================

    /**
     * @brief Create the schema_version table if it doesn't exist
     *
     * @param db The SQLite database handle
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto ensure_schema_version_table(sqlite3* db) -> VoidResult;

    /**
     * @brief Apply a single migration
     *
     * @param db The SQLite database handle
     * @param version The migration version to apply
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto apply_migration(sqlite3* db, int version) -> VoidResult;

    /**
     * @brief Record a migration in the schema_version table
     *
     * @param db The SQLite database handle
     * @param version The version that was applied
     * @param description Description of the migration
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto record_migration(sqlite3* db, int version,
                                        std::string_view description)
        -> VoidResult;

    /**
     * @brief Execute SQL statement and handle errors
     *
     * @param db The SQLite database handle
     * @param sql The SQL statement to execute
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto execute_sql(sqlite3* db, std::string_view sql)
        -> VoidResult;

    // Migration implementations (SQLite)
    [[nodiscard]] auto migrate_v1(sqlite3* db) -> VoidResult;
    [[nodiscard]] auto migrate_v2(sqlite3* db) -> VoidResult;
    [[nodiscard]] auto migrate_v3(sqlite3* db) -> VoidResult;
    [[nodiscard]] auto migrate_v4(sqlite3* db) -> VoidResult;
    [[nodiscard]] auto migrate_v5(sqlite3* db) -> VoidResult;
    [[nodiscard]] auto migrate_v6(sqlite3* db) -> VoidResult;
    [[nodiscard]] auto migrate_v7(sqlite3* db) -> VoidResult;

#ifdef PACS_WITH_DATABASE_SYSTEM
    // ========================================================================
    // Internal Implementation (pacs_database_adapter)
    // ========================================================================

    /**
     * @brief Create the schema_version table if it doesn't exist
     *
     * @param db The pacs_database_adapter instance
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto ensure_schema_version_table(pacs_database_adapter& db)
        -> VoidResult;

    /**
     * @brief Apply a single migration using pacs_database_adapter
     *
     * @param db The pacs_database_adapter instance
     * @param version The migration version to apply
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto apply_migration(pacs_database_adapter& db, int version)
        -> VoidResult;

    /**
     * @brief Record a migration in the schema_version table
     *
     * @param db The pacs_database_adapter instance
     * @param version The version that was applied
     * @param description Description of the migration
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto record_migration(pacs_database_adapter& db, int version,
                                        std::string_view description)
        -> VoidResult;

    /**
     * @brief Execute SQL statement using pacs_database_adapter
     *
     * @param db The pacs_database_adapter instance
     * @param sql The SQL statement to execute
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto execute_sql(pacs_database_adapter& db,
                                   std::string_view sql) -> VoidResult;

    // Migration implementations (pacs_database_adapter)
    [[nodiscard]] auto migrate_v1(pacs_database_adapter& db) -> VoidResult;
    [[nodiscard]] auto migrate_v2(pacs_database_adapter& db) -> VoidResult;
    [[nodiscard]] auto migrate_v3(pacs_database_adapter& db) -> VoidResult;
    [[nodiscard]] auto migrate_v4(pacs_database_adapter& db) -> VoidResult;
    [[nodiscard]] auto migrate_v5(pacs_database_adapter& db) -> VoidResult;
    [[nodiscard]] auto migrate_v6(pacs_database_adapter& db) -> VoidResult;
    [[nodiscard]] auto migrate_v7(pacs_database_adapter& db) -> VoidResult;

    /// Migration function registry (pacs_database_adapter)
    std::vector<std::pair<int, adapter_migration_function>> adapter_migrations_;
#endif

    /// Latest schema version (increment when adding migrations)
    static constexpr int LATEST_VERSION = 7;

    /// Migration function registry
    std::vector<std::pair<int, migration_function>> migrations_;
};

}  // namespace pacs::storage
