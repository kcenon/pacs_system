/**
 * @file base_repository.hpp
 * @brief Generic base repository for CRUD operations
 *
 * This file provides a template base class for repositories that implement
 * common CRUD patterns using the pacs_database_adapter. It eliminates code
 * duplication across repository implementations and provides consistent
 * data access patterns.
 *
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 * @see Epic #605 - Migrate Direct Database Access to database_system
 * Abstraction
 */

#pragma once

#define PACS_STORAGE_BASE_REPOSITORY_HPP_INCLUDED

#include <pacs/storage/pacs_database_adapter.hpp>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <database/core/database_backend.h>
#include <database/database_types.h>

namespace pacs::storage {

/// Database value type alias
using database_value = database::core::database_value;

/**
 * @brief Generic base repository providing common CRUD operations
 *
 * This template class provides standard database operations (Create, Read,
 * Update, Delete) for domain entities. Subclasses must implement the abstract
 * mapping methods to convert between database rows and domain entities.
 *
 * **Features:**
 * - Type-safe CRUD operations with Result<T> error handling
 * - Transaction support via pacs_database_adapter
 * - Batch operations for efficient bulk inserts
 * - Query builder integration for complex queries
 * - Extensible design for domain-specific operations
 *
 * **Thread Safety:** This class is NOT thread-safe. External synchronization
 * is required for concurrent access.
 *
 * @tparam Entity The domain entity type to be persisted
 * @tparam PrimaryKey The primary key type (default: int64_t)
 *
 * @example
 * @code
 * // Define a domain entity
 * struct Patient {
 *     int64_t id{0};
 *     std::string patient_id;
 *     std::string patient_name;
 * };
 *
 * // Create repository subclass
 * class patient_repository : public base_repository<Patient> {
 * public:
 *     explicit patient_repository(std::shared_ptr<pacs_database_adapter> db)
 *         : base_repository(db, "patients", "id") {}
 *
 * protected:
 *     Patient map_row_to_entity(const database_row& row) const override {
 *         return Patient{
 *             std::stoll(row.at("id")),
 *             row.at("patient_id"),
 *             row.at("patient_name")
 *         };
 *     }
 *
 *     std::map<std::string, database::database_value>
 *     entity_to_row(const Patient& p) const override {
 *         return {
 *             {"patient_id", p.patient_id},
 *             {"patient_name", p.patient_name}
 *         };
 *     }
 *
 *     int64_t get_pk(const Patient& p) const override {
 *         return p.id;
 *     }
 *
 *     bool has_pk(const Patient& p) const override {
 *         return p.id > 0;
 *     }
 * };
 *
 * // Usage
 * auto db = std::make_shared<pacs_database_adapter>("pacs.db");
 * db->connect();
 * patient_repository repo(db);
 *
 * // Find by ID
 * auto result = repo.find_by_id(1);
 * if (result.is_ok()) {
 *     auto patient = result.value();
 *     // use patient...
 * }
 *
 * // Save entity
 * Patient new_patient{0, "P001", "Doe^John"};
 * auto save_result = repo.save(new_patient);
 * @endcode
 */
template <typename Entity, typename PrimaryKey = int64_t>
class base_repository {
public:
    /// Entity type alias
    using entity_type = Entity;

    /// Primary key type alias
    using pk_type = PrimaryKey;

    /// Result type for single entity operations
    using result_type = Result<Entity>;

    /// Result type for list operations
    using list_result_type = Result<std::vector<Entity>>;

    /**
     * @brief Construct repository with database adapter and table name
     *
     * @param db Shared pointer to database adapter
     * @param table_name Name of the database table
     * @param pk_column Name of the primary key column (default: "id")
     */
    explicit base_repository(std::shared_ptr<pacs_database_adapter> db,
                             std::string table_name,
                             std::string pk_column = "id");

    /// Virtual destructor for proper cleanup in derived classes
    virtual ~base_repository() = default;

    // Non-copyable
    base_repository(const base_repository&) = delete;
    auto operator=(const base_repository&) -> base_repository& = delete;

    // Movable
    base_repository(base_repository&&) noexcept = default;
    auto operator=(base_repository&&) noexcept -> base_repository& = default;

    // ========================================================================
    // Read Operations
    // ========================================================================

    /**
     * @brief Find entity by primary key
     *
     * Executes a SELECT query to find the entity with the specified primary
     * key.
     *
     * @param id Primary key value
     * @return Result containing the entity if found, or error
     */
    [[nodiscard]] virtual auto find_by_id(PrimaryKey id) -> result_type;

    /**
     * @brief Find all entities in the table
     *
     * Retrieves all rows from the table. Use with caution on large tables.
     *
     * @param limit Optional maximum number of rows to return
     * @return Result containing vector of entities or error
     */
    [[nodiscard]] virtual auto find_all(std::optional<size_t> limit = std::nullopt)
        -> list_result_type;

    /**
     * @brief Find entities matching a condition
     *
     * Executes a SELECT query with a WHERE clause.
     *
     * @param column Column name to filter on
     * @param op Comparison operator (=, !=, <, >, <=, >=, LIKE, etc.)
     * @param value Value to compare against
     * @return Result containing matching entities or error
     *
     * @example
     * @code
     * // Find patients with specific patient_id
     * auto result = repo.find_where("patient_id", "=", "P001");
     * @endcode
     */
    [[nodiscard]] virtual auto find_where(const std::string& column,
                                          const std::string& op,
                                          const database_value& value)
        -> list_result_type;

    /**
     * @brief Check if an entity with given ID exists
     *
     * @param id Primary key value
     * @return Result containing true if exists, false otherwise
     */
    [[nodiscard]] virtual auto exists(PrimaryKey id) -> Result<bool>;

    /**
     * @brief Count total number of entities
     *
     * @return Result containing count or error
     */
    [[nodiscard]] virtual auto count() -> Result<size_t>;

    // ========================================================================
    // Write Operations
    // ========================================================================

    /**
     * @brief Save entity (insert or update)
     *
     * If the entity has a valid primary key (has_pk() returns true), performs
     * an UPDATE. Otherwise, performs an INSERT and returns the new primary
     * key.
     *
     * @param entity Entity to save
     * @return Result containing the primary key (existing or new)
     */
    [[nodiscard]] virtual auto save(const Entity& entity)
        -> Result<PrimaryKey>;

    /**
     * @brief Insert new entity
     *
     * Inserts a new row into the database.
     *
     * @param entity Entity to insert
     * @return Result containing the new primary key or error
     */
    [[nodiscard]] virtual auto insert(const Entity& entity)
        -> Result<PrimaryKey>;

    /**
     * @brief Update existing entity
     *
     * Updates an existing row. The entity must have a valid primary key.
     *
     * @param entity Entity to update
     * @return Result indicating success or error
     */
    [[nodiscard]] virtual auto update(const Entity& entity) -> VoidResult;

    /**
     * @brief Delete entity by primary key
     *
     * @param id Primary key of entity to delete
     * @return Result indicating success or error
     */
    [[nodiscard]] virtual auto remove(PrimaryKey id) -> VoidResult;

    /**
     * @brief Delete entities matching condition
     *
     * @param column Column name to filter on
     * @param op Comparison operator
     * @param value Value to compare against
     * @return Result containing number of deleted rows or error
     */
    [[nodiscard]] virtual auto remove_where(const std::string& column,
                                            const std::string& op,
                                            const database_value& value)
        -> Result<size_t>;

    // ========================================================================
    // Batch Operations
    // ========================================================================

    /**
     * @brief Insert multiple entities in a transaction
     *
     * Inserts all entities within a single transaction for efficiency.
     * If any insert fails, the entire batch is rolled back.
     *
     * @param entities Vector of entities to insert
     * @return Result containing vector of new primary keys or error
     */
    [[nodiscard]] virtual auto insert_batch(const std::vector<Entity>& entities)
        -> Result<std::vector<PrimaryKey>>;

    /**
     * @brief Execute callback within a transaction
     *
     * Automatically begins transaction, executes function, and commits.
     * Rolls back on exception or if function returns error.
     *
     * @tparam Func Function type that takes no args and returns a Result
     * @param func Function to execute within transaction
     * @return Result from the function or transaction error
     *
     * @example
     * @code
     * auto result = repo.in_transaction([&]() -> VoidResult {
     *     auto r1 = repo.insert(entity1);
     *     if (r1.is_err()) return r1.error();
     *
     *     auto r2 = repo.insert(entity2);
     *     if (r2.is_err()) return r2.error();
     *
     *     return kcenon::common::ok();
     * });
     * @endcode
     */
    template <typename Func>
    [[nodiscard]] auto in_transaction(Func&& func)
        -> std::invoke_result_t<Func>;

protected:
    // ========================================================================
    // Abstract Methods - Subclass Must Implement
    // ========================================================================

    /**
     * @brief Map database row to entity
     *
     * Subclasses must implement this to convert a database row into a domain
     * entity.
     *
     * @param row Database row with column-value pairs
     * @return Entity constructed from row data
     */
    [[nodiscard]] virtual auto map_row_to_entity(const database_row& row) const
        -> Entity = 0;

    /**
     * @brief Map entity to column-value pairs
     *
     * Subclasses must implement this to convert an entity into a map of
     * column names to values for INSERT/UPDATE operations.
     *
     * Note: Do not include the primary key column in the returned map for
     * INSERT operations (it will be auto-generated).
     *
     * @param entity Entity to map
     * @return Map of column names to database values
     */
    [[nodiscard]] virtual auto entity_to_row(const Entity& entity) const
        -> std::map<std::string, database_value> = 0;

    /**
     * @brief Get primary key value from entity
     *
     * @param entity Entity to extract primary key from
     * @return Primary key value
     */
    [[nodiscard]] virtual auto get_pk(const Entity& entity) const
        -> PrimaryKey = 0;

    /**
     * @brief Check if entity has a valid primary key
     *
     * This is used by save() to determine whether to INSERT or UPDATE.
     *
     * @param entity Entity to check
     * @return true if entity has a valid (non-default) primary key
     */
    [[nodiscard]] virtual auto has_pk(const Entity& entity) const -> bool = 0;

    /**
     * @brief Get columns for SELECT queries
     *
     * Override this if you need to customize which columns are selected.
     * Default implementation returns "*".
     *
     * @return Vector of column names to select
     */
    [[nodiscard]] virtual auto select_columns() const
        -> std::vector<std::string>;

    // ========================================================================
    // Utility Methods for Subclasses
    // ========================================================================

    /**
     * @brief Create a query builder for this database
     *
     * @return Query builder instance configured for the database
     */
    [[nodiscard]] auto query_builder() -> database::query_builder;

    /**
     * @brief Get the database adapter
     *
     * @return Shared pointer to database adapter
     */
    [[nodiscard]] auto db() -> std::shared_ptr<pacs_database_adapter>;

    /**
     * @brief Get the table name
     *
     * @return Table name
     */
    [[nodiscard]] auto table_name() const -> const std::string&;

    /**
     * @brief Get the primary key column name
     *
     * @return Primary key column name
     */
    [[nodiscard]] auto pk_column() const -> const std::string&;

private:
    /// Database adapter for executing queries
    std::shared_ptr<pacs_database_adapter> db_;

    /// Name of the database table
    std::string table_name_;

    /// Name of the primary key column
    std::string pk_column_;
};

}  // namespace pacs::storage

// Include template implementation
#include "base_repository_impl.hpp"

#endif  // PACS_WITH_DATABASE_SYSTEM
