/**
 * @file pacs_database_adapter.hpp
 * @brief Unified database adapter for PACS system
 *
 * This file provides the pacs_database_adapter class that wraps database_system's
 * unified_database_system API. It serves as a single entry point for all database
 * operations and enables incremental migration from direct SQLite access.
 *
 * When compiled with PACS_WITH_DATABASE_SYSTEM, uses database_system's
 * unified_database_system. This adapter is the foundation for migrating
 * scattered SQLite API calls to a centralized, consistent interface.
 *
 * @see Issue #606 - Phase 1: Foundation - PACS Database Adapter
 * @see Epic #605 - Migrate Direct Database Access to database_system Abstraction
 */

#pragma once

#include <kcenon/common/patterns/result.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM
#include <database/database_types.h>
#include <database/integrated/unified_database_system.h>
#include <database/query_builder.h>
#endif

namespace pacs::storage {

/// Result type alias for operations returning a value
template <typename T>
using Result = kcenon::common::Result<T>;

/// Result type alias for void operations
using VoidResult = kcenon::common::VoidResult;

#ifdef PACS_WITH_DATABASE_SYSTEM

/**
 * @brief Database row type alias
 *
 * Represents a single row from query results as key-value pairs
 * where keys are column names and values are string representations.
 */
using database_row = std::map<std::string, std::string>;

/**
 * @brief Query result structure
 *
 * Contains the results of a database query including rows, affected row count,
 * and execution time.
 */
struct database_result {
    /// Result rows from SELECT queries
    std::vector<database_row> rows;

    /// Number of rows affected by INSERT/UPDATE/DELETE
    std::size_t affected_rows{0};

    /// Query execution time
    std::chrono::microseconds execution_time{0};

    /// Check if result is empty
    [[nodiscard]] auto empty() const noexcept -> bool { return rows.empty(); }

    /// Get number of rows
    [[nodiscard]] auto size() const noexcept -> std::size_t {
        return rows.size();
    }

    /// Row access operator
    [[nodiscard]] auto operator[](std::size_t index) const
        -> const database_row& {
        return rows.at(index);
    }

    /// Mutable row access
    [[nodiscard]] auto operator[](std::size_t index) -> database_row& {
        return rows.at(index);
    }

    // Iterator support
    auto begin() { return rows.begin(); }
    auto end() { return rows.end(); }
    [[nodiscard]] auto begin() const { return rows.begin(); }
    [[nodiscard]] auto end() const { return rows.end(); }
};

// Forward declaration
class scoped_transaction;

/**
 * @brief Unified database adapter for PACS system
 *
 * Wraps database_system to provide a consistent interface for all storage
 * operations. This adapter serves as the single entry point for database access
 * in the PACS system.
 *
 * **Key Features:**
 * - Simplified API tailored for PACS use cases
 * - Consistent Result<T> error handling
 * - Transaction support with RAII guard
 * - Query builder integration for type-safe queries
 *
 * **Thread Safety:** This class is NOT thread-safe. External synchronization
 * is required for concurrent access. Consider using a connection pool or
 * mutex for multi-threaded applications.
 *
 * @example
 * @code
 * // Create adapter for SQLite database
 * pacs_database_adapter db("/path/to/pacs.db");
 * auto connect_result = db.connect();
 * if (connect_result.is_err()) {
 *     // Handle error
 * }
 *
 * // Execute query
 * auto result = db.select("SELECT * FROM patients WHERE patient_id = 'P001'");
 * if (result.is_ok()) {
 *     for (const auto& row : result.value()) {
 *         std::cout << row.at("patient_name") << std::endl;
 *     }
 * }
 *
 * // Use transaction
 * {
 *     scoped_transaction tx(db);
 *     db.insert("INSERT INTO patients (patient_id, patient_name) VALUES "
 *               "('P002', 'Doe^John')");
 *     tx.commit();
 * } // auto-rollback if commit() not called
 * @endcode
 */
class pacs_database_adapter {
public:
    /**
     * @brief Construct adapter with SQLite database path
     *
     * Creates an adapter configured for SQLite database at the specified path.
     * The database file is created if it doesn't exist.
     *
     * @param db_path Path to the SQLite database file
     */
    explicit pacs_database_adapter(const std::filesystem::path& db_path);

    /**
     * @brief Construct adapter with database type and connection string
     *
     * Creates an adapter for any supported database backend.
     *
     * @param type Database backend type (sqlite, postgres, mysql, etc.)
     * @param connection_string Backend-specific connection string
     */
    pacs_database_adapter(database::database_types type,
                          const std::string& connection_string);

    /**
     * @brief Destructor - disconnects from database
     */
    ~pacs_database_adapter();

    // Non-copyable
    pacs_database_adapter(const pacs_database_adapter&) = delete;
    auto operator=(const pacs_database_adapter&)
        -> pacs_database_adapter& = delete;

    // Movable
    pacs_database_adapter(pacs_database_adapter&&) noexcept;
    auto operator=(pacs_database_adapter&&) noexcept -> pacs_database_adapter&;

    // ========================================================================
    // Connection Management
    // ========================================================================

    /**
     * @brief Connect to the database
     *
     * Establishes connection using the configured connection string.
     *
     * @return Success or error result
     */
    [[nodiscard]] auto connect() -> VoidResult;

    /**
     * @brief Disconnect from the database
     *
     * Closes the database connection and releases resources.
     *
     * @return Success or error result
     */
    [[nodiscard]] auto disconnect() -> VoidResult;

    /**
     * @brief Check if connected to database
     *
     * @return true if connected, false otherwise
     */
    [[nodiscard]] auto is_connected() const noexcept -> bool;

    // ========================================================================
    // Query Builder Factory
    // ========================================================================

    /**
     * @brief Create a query builder for this database
     *
     * Returns a query builder configured for the current database type.
     * Use this for building type-safe, parameterized queries.
     *
     * @return Query builder instance
     *
     * @example
     * @code
     * auto builder = db.create_query_builder();
     * builder.select({"patient_id", "patient_name"})
     *     .from("patients")
     *     .where("patient_id", "=", "P001")
     *     .limit(10);
     * auto query_str = builder.build();
     * auto result = db.select(query_str);
     * @endcode
     */
    [[nodiscard]] auto create_query_builder() -> database::query_builder;

    // ========================================================================
    // CRUD Operations
    // ========================================================================

    /**
     * @brief Execute a SELECT query
     *
     * @param query SQL SELECT statement
     * @return Result containing query rows or error
     */
    [[nodiscard]] auto select(const std::string& query)
        -> Result<database_result>;

    /**
     * @brief Execute an INSERT query
     *
     * @param query SQL INSERT statement
     * @return Result containing number of inserted rows or error
     */
    [[nodiscard]] auto insert(const std::string& query) -> Result<uint64_t>;

    /**
     * @brief Execute an UPDATE query
     *
     * @param query SQL UPDATE statement
     * @return Result containing number of updated rows or error
     */
    [[nodiscard]] auto update(const std::string& query) -> Result<uint64_t>;

    /**
     * @brief Execute a DELETE query
     *
     * @param query SQL DELETE statement
     * @return Result containing number of deleted rows or error
     */
    [[nodiscard]] auto remove(const std::string& query) -> Result<uint64_t>;

    /**
     * @brief Execute raw SQL (DDL, PRAGMA, etc.)
     *
     * Use for schema changes, PRAGMA statements, and other non-CRUD operations.
     *
     * @param query SQL statement to execute
     * @return Success or error result
     */
    [[nodiscard]] auto execute(const std::string& query) -> VoidResult;

    // ========================================================================
    // Transaction Support
    // ========================================================================

    /**
     * @brief Begin a database transaction
     *
     * @return Success or error result
     */
    [[nodiscard]] auto begin_transaction() -> VoidResult;

    /**
     * @brief Commit the current transaction
     *
     * @return Success or error result
     */
    [[nodiscard]] auto commit() -> VoidResult;

    /**
     * @brief Rollback the current transaction
     *
     * @return Success or error result
     */
    [[nodiscard]] auto rollback() -> VoidResult;

    /**
     * @brief Check if currently in a transaction
     *
     * @return true if in transaction, false otherwise
     */
    [[nodiscard]] auto in_transaction() const noexcept -> bool;

    /**
     * @brief Execute a function within a transaction
     *
     * Automatically begins transaction, executes function, and commits.
     * Rolls back on exception or if function returns error.
     *
     * @tparam Func Function type that takes no args and returns VoidResult
     * @param func Function to execute within transaction
     * @return Success if function succeeds and transaction commits, error
     * otherwise
     *
     * @example
     * @code
     * auto result = db.transaction([&]() -> VoidResult {
     *     auto r1 = db.insert("INSERT INTO patients ...");
     *     if (r1.is_err()) return r1.error();
     *
     *     auto r2 = db.update("UPDATE studies ...");
     *     if (r2.is_err()) return r2.error();
     *
     *     return kcenon::common::ok();
     * });
     * @endcode
     */
    template <typename Func>
    [[nodiscard]] auto transaction(Func&& func) -> VoidResult {
        auto begin_result = begin_transaction();
        if (begin_result.is_err()) {
            return begin_result;
        }

        try {
            auto result = std::forward<Func>(func)();
            if (result.is_err()) {
                (void)rollback();
                return result;
            }

            return commit();
        } catch (const std::exception& e) {
            (void)rollback();
            return VoidResult(kcenon::common::error_info{
                -1, std::string("Transaction failed: ") + e.what(), "storage"});
        }
    }

    // ========================================================================
    // SQLite Compatibility
    // ========================================================================

    /**
     * @brief Get last insert rowid (SQLite specific)
     *
     * Returns the rowid of the most recently inserted row.
     * For non-SQLite backends, returns 0.
     *
     * @return Last insert rowid or 0
     */
    [[nodiscard]] auto last_insert_rowid() const -> int64_t;

    /**
     * @brief Get last error message
     *
     * @return Last error message or empty string
     */
    [[nodiscard]] auto last_error() const -> std::string;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

/**
 * @brief RAII transaction guard
 *
 * Provides automatic transaction management with commit/rollback semantics.
 * If commit() is not called before destruction, the transaction is
 * automatically rolled back.
 *
 * @example
 * @code
 * {
 *     scoped_transaction tx(db);
 *     db.insert("INSERT INTO ...");
 *     db.update("UPDATE ...");
 *
 *     auto result = tx.commit();
 *     if (result.is_err()) {
 *         // Handle commit failure
 *     }
 * } // auto-rollback if commit() not called
 * @endcode
 */
class scoped_transaction {
public:
    /**
     * @brief Construct and begin transaction
     *
     * @param db Database adapter to use
     * @throws No exceptions - check is_active() for success
     */
    explicit scoped_transaction(pacs_database_adapter& db);

    /**
     * @brief Destructor - rollback if not committed
     */
    ~scoped_transaction();

    // Non-copyable, non-movable
    scoped_transaction(const scoped_transaction&) = delete;
    auto operator=(const scoped_transaction&) -> scoped_transaction& = delete;
    scoped_transaction(scoped_transaction&&) = delete;
    auto operator=(scoped_transaction&&) -> scoped_transaction& = delete;

    /**
     * @brief Commit the transaction
     *
     * After successful commit, the destructor will not rollback.
     *
     * @return Success or error result
     */
    [[nodiscard]] auto commit() -> VoidResult;

    /**
     * @brief Explicitly rollback the transaction
     *
     * Call this to rollback before destruction.
     */
    void rollback() noexcept;

    /**
     * @brief Check if transaction is active
     *
     * @return true if transaction is active (not committed/rolled back)
     */
    [[nodiscard]] auto is_active() const noexcept -> bool;

private:
    pacs_database_adapter& db_;
    bool committed_{false};
    bool active_{false};
};

#endif  // PACS_WITH_DATABASE_SYSTEM

}  // namespace pacs::storage
