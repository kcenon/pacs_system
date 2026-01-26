/**
 * @file base_repository.hpp
 * @brief Generic base repository providing common CRUD operations
 *
 * This file provides the base_repository template class that eliminates code
 * duplication across repository implementations by providing common CRUD
 * operations using pacs_database_adapter.
 *
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 * @see Epic #605 - Migrate Direct Database Access to database_system Abstraction
 */

#pragma once

#include "pacs/storage/pacs_database_adapter.hpp"

#include <kcenon/common/patterns/result.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM
#include <database/database_types.h>
#include <database/query_builder.h>
#endif

namespace pacs::storage {

/**
 * @brief Generic base repository providing common CRUD operations
 *
 * This template class provides a foundation for repository implementations,
 * eliminating code duplication by providing common database operations.
 * Subclasses implement entity-specific mapping logic.
 *
 * **Key Features:**
 * - Type-safe CRUD operations with Result<T>
 * - Transaction support via pacs_database_adapter
 * - Query builder integration for safe SQL generation
 * - Batch operations support
 * - Consistent error handling
 *
 * **Thread Safety:** This class is NOT thread-safe. External synchronization
 * is required for concurrent access.
 *
 * @tparam Entity The domain entity type (e.g., job_record, annotation_record)
 * @tparam PrimaryKey The primary key type (default: int64_t for auto-increment)
 *
 * @example
 * @code
 * // Define entity-specific repository
 * class job_repository : public base_repository<job_record, std::string> {
 * public:
 *     explicit job_repository(std::shared_ptr<pacs_database_adapter> db)
 *         : base_repository(db, "jobs", "job_id") {}
 *
 * protected:
 *     job_record map_row_to_entity(const database_row& row) const override {
 *         job_record job;
 *         job.job_id = row.at("job_id");
 *         job.status = parse_status(row.at("status"));
 *         return job;
 *     }
 *
 *     std::map<std::string, std::string> entity_to_row(
 *         const job_record& job) const override {
 *         return {
 *             {"job_id", job.job_id},
 *             {"status", to_string(job.status)},
 *             {"created_at", format_timestamp(job.created_at)}
 *         };
 *     }
 *
 *     std::string get_pk(const job_record& job) const override {
 *         return job.job_id;
 *     }
 *
 *     bool has_pk(const job_record& job) const override {
 *         return !job.job_id.empty();
 *     }
 * };
 *
 * // Usage
 * auto db = std::make_shared<pacs_database_adapter>("pacs.db");
 * db->connect();
 * auto repo = job_repository(db);
 *
 * // Find by primary key
 * auto result = repo.find_by_id("job-123");
 * if (result.is_ok()) {
 *     auto job = result.value();
 * }
 *
 * // Save entity
 * job_record new_job;
 * new_job.job_id = "job-456";
 * auto save_result = repo.save(new_job);
 * @endcode
 */
template <typename Entity, typename PrimaryKey = int64_t>
class base_repository {
public:
    using entity_type = Entity;
    using pk_type = PrimaryKey;
    using result_type = Result<Entity>;
    using list_result_type = Result<std::vector<Entity>>;

    /**
     * @brief Construct repository with database adapter and table name
     *
     * @param db Shared pointer to database adapter (must remain valid)
     * @param table_name Name of the database table
     * @param pk_column Name of the primary key column (default: "id")
     */
    explicit base_repository(std::shared_ptr<pacs_database_adapter> db,
                             std::string table_name,
                             std::string pk_column = "id")
        : db_(std::move(db))
        , table_name_(std::move(table_name))
        , pk_column_(std::move(pk_column)) {}

    virtual ~base_repository() = default;

    // Non-copyable
    base_repository(const base_repository&) = delete;
    auto operator=(const base_repository&) -> base_repository& = delete;

    // Movable
    base_repository(base_repository&&) noexcept = default;
    auto operator=(base_repository&&) noexcept -> base_repository& = default;

    // =========================================================================
    // Basic CRUD Operations
    // =========================================================================

    /**
     * @brief Find entity by primary key
     *
     * @param id Primary key value to search for
     * @return Result containing the entity if found, error otherwise
     */
    [[nodiscard]] virtual auto find_by_id(PrimaryKey id) -> result_type {
#ifdef PACS_WITH_DATABASE_SYSTEM
        auto builder = db_->create_query_builder();
        builder.select(select_columns())
            .from(table_name_)
            .where(pk_column_, "=", to_db_value(id))
            .limit(1);

        auto query = builder.build();
        auto result = db_->select(query);

        if (result.is_err()) {
            return result_type(result.error());
        }

        const auto& db_result = result.value();
        if (db_result.empty()) {
            return result_type(
                kcenon::common::error_info{-1, "Entity not found", "storage"});
        }

        return result_type(map_row_to_entity(db_result[0]));
#else
        return result_type(kcenon::common::error_info{
            -1, "Database system not available", "storage"});
#endif
    }

    /**
     * @brief Find all entities (with optional limit)
     *
     * @param limit Optional maximum number of results
     * @return Result containing vector of entities or error
     */
    [[nodiscard]] virtual auto find_all(std::optional<size_t> limit = std::nullopt)
        -> list_result_type {
#ifdef PACS_WITH_DATABASE_SYSTEM
        auto builder = db_->create_query_builder();
        builder.select(select_columns()).from(table_name_);

        if (limit.has_value()) {
            builder.limit(limit.value());
        }

        auto query = builder.build();
        auto result = db_->select(query);

        if (result.is_err()) {
            return list_result_type(result.error());
        }

        std::vector<Entity> entities;
        entities.reserve(result.value().size());

        for (const auto& row : result.value()) {
            entities.push_back(map_row_to_entity(row));
        }

        return list_result_type(std::move(entities));
#else
        return list_result_type(kcenon::common::error_info{
            -1, "Database system not available", "storage"});
#endif
    }

    /**
     * @brief Find entities matching condition
     *
     * @param column Column name to filter on
     * @param op Comparison operator ("=", "<", ">", "LIKE", etc.)
     * @param value Value to compare against
     * @return Result containing vector of matching entities or error
     */
    [[nodiscard]] virtual auto find_where(const std::string& column,
                                          const std::string& op,
                                          const std::string& value)
        -> list_result_type {
#ifdef PACS_WITH_DATABASE_SYSTEM
        auto builder = db_->create_query_builder();
        builder.select(select_columns())
            .from(table_name_)
            .where(column, op, value);

        auto query = builder.build();
        auto result = db_->select(query);

        if (result.is_err()) {
            return list_result_type(result.error());
        }

        std::vector<Entity> entities;
        entities.reserve(result.value().size());

        for (const auto& row : result.value()) {
            entities.push_back(map_row_to_entity(row));
        }

        return list_result_type(std::move(entities));
#else
        return list_result_type(kcenon::common::error_info{
            -1, "Database system not available", "storage"});
#endif
    }

    /**
     * @brief Check if entity exists
     *
     * @param id Primary key value to check
     * @return Result containing true if exists, false otherwise, or error
     */
    [[nodiscard]] virtual auto exists(PrimaryKey id) -> Result<bool> {
#ifdef PACS_WITH_DATABASE_SYSTEM
        auto builder = db_->create_query_builder();
        builder.select({"COUNT(*) as count"})
            .from(table_name_)
            .where(pk_column_, "=", to_db_value(id));

        auto query = builder.build();
        auto result = db_->select(query);

        if (result.is_err()) {
            return Result<bool>(result.error());
        }

        const auto& db_result = result.value();
        if (db_result.empty()) {
            return Result<bool>(false);
        }

        auto count = std::stoi(db_result[0].at("count"));
        return Result<bool>(count > 0);
#else
        return Result<bool>(kcenon::common::error_info{
            -1, "Database system not available", "storage"});
#endif
    }

    /**
     * @brief Count all entities
     *
     * @return Result containing count or error
     */
    [[nodiscard]] virtual auto count() -> Result<size_t> {
#ifdef PACS_WITH_DATABASE_SYSTEM
        auto builder = db_->create_query_builder();
        builder.select({"COUNT(*) as count"}).from(table_name_);

        auto query = builder.build();
        auto result = db_->select(query);

        if (result.is_err()) {
            return Result<size_t>(result.error());
        }

        const auto& db_result = result.value();
        if (db_result.empty()) {
            return Result<size_t>(0);
        }

        auto count = std::stoul(db_result[0].at("count"));
        return Result<size_t>(count);
#else
        return Result<size_t>(kcenon::common::error_info{
            -1, "Database system not available", "storage"});
#endif
    }

    /**
     * @brief Save entity (insert or update based on PK)
     *
     * If the entity has a valid primary key and exists in database, updates it.
     * Otherwise, inserts a new record.
     *
     * @param entity Entity to save
     * @return Result containing the primary key or error
     */
    [[nodiscard]] virtual auto save(const Entity& entity) -> Result<PrimaryKey> {
        if (has_pk(entity)) {
            auto pk = get_pk(entity);
            auto exists_result = exists(pk);

            if (exists_result.is_err()) {
                return Result<PrimaryKey>(exists_result.error());
            }

            if (exists_result.value()) {
                // Entity exists, update it
                auto update_result = update(entity);
                if (update_result.is_err()) {
                    return Result<PrimaryKey>(update_result.error());
                }
                return Result<PrimaryKey>(pk);
            }
        }

        // Entity doesn't exist or has no PK, insert it
        return insert(entity);
    }

    /**
     * @brief Insert new entity
     *
     * @param entity Entity to insert
     * @return Result containing the primary key of inserted entity or error
     */
    [[nodiscard]] virtual auto insert(const Entity& entity) -> Result<PrimaryKey> {
#ifdef PACS_WITH_DATABASE_SYSTEM
        auto row = entity_to_row(entity);

        // Build column names and values
        std::vector<std::string> columns;
        std::vector<std::string> values;
        columns.reserve(row.size());
        values.reserve(row.size());

        for (const auto& [col, val] : row) {
            columns.push_back(col);
            values.push_back("'" + val + "'");
        }

        // Construct INSERT query manually
        std::string query = "INSERT INTO " + table_name_ + " (";
        for (size_t i = 0; i < columns.size(); ++i) {
            if (i > 0) query += ", ";
            query += columns[i];
        }
        query += ") VALUES (";
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) query += ", ";
            query += values[i];
        }
        query += ")";

        auto result = db_->insert(query);

        if (result.is_err()) {
            return Result<PrimaryKey>(result.error());
        }

        // For string PKs, return the entity's PK
        // For numeric PKs, return last_insert_rowid() if available
        if constexpr (std::is_same_v<PrimaryKey, std::string>) {
            return Result<PrimaryKey>(get_pk(entity));
        } else {
            auto rowid = db_->last_insert_rowid();
            return Result<PrimaryKey>(static_cast<PrimaryKey>(rowid));
        }
#else
        return Result<PrimaryKey>(kcenon::common::error_info{
            -1, "Database system not available", "storage"});
#endif
    }

    /**
     * @brief Update existing entity
     *
     * @param entity Entity with updated values
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] virtual auto update(const Entity& entity) -> VoidResult {
#ifdef PACS_WITH_DATABASE_SYSTEM
        auto row = entity_to_row(entity);
        auto pk = get_pk(entity);

        // Build UPDATE query manually
        std::string query = "UPDATE " + table_name_ + " SET ";
        bool first = true;
        for (const auto& [col, val] : row) {
            if (!first) query += ", ";
            first = false;
            query += col + " = '" + val + "'";
        }
        query += " WHERE " + pk_column_ + " = '" + to_db_value(pk) + "'";

        auto result = db_->update(query);

        if (result.is_err()) {
            return VoidResult(result.error());
        }

        return kcenon::common::ok();
#else
        return VoidResult(kcenon::common::error_info{
            -1, "Database system not available", "storage"});
#endif
    }

    /**
     * @brief Delete entity by primary key
     *
     * @param id Primary key of entity to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] virtual auto remove(PrimaryKey id) -> VoidResult {
#ifdef PACS_WITH_DATABASE_SYSTEM
        auto builder = db_->create_query_builder();
        builder.delete_from(table_name_).where(pk_column_, "=", to_db_value(id));

        auto query = builder.build();
        auto result = db_->remove(query);

        if (result.is_err()) {
            return VoidResult(result.error());
        }

        return kcenon::common::ok();
#else
        return VoidResult(kcenon::common::error_info{
            -1, "Database system not available", "storage"});
#endif
    }

    /**
     * @brief Delete entities matching condition
     *
     * @param column Column name to filter on
     * @param op Comparison operator
     * @param value Value to compare against
     * @return Result containing number of deleted entities or error
     */
    [[nodiscard]] virtual auto remove_where(const std::string& column,
                                            const std::string& op,
                                            const std::string& value)
        -> Result<size_t> {
#ifdef PACS_WITH_DATABASE_SYSTEM
        auto builder = db_->create_query_builder();
        builder.delete_from(table_name_).where(column, op, value);

        auto query = builder.build();
        auto result = db_->remove(query);

        if (result.is_err()) {
            return Result<size_t>(result.error());
        }

        return Result<size_t>(result.value());
#else
        return Result<size_t>(kcenon::common::error_info{
            -1, "Database system not available", "storage"});
#endif
    }

    // =========================================================================
    // Batch Operations
    // =========================================================================

    /**
     * @brief Insert multiple entities in a transaction
     *
     * All inserts succeed together or all fail (atomic operation).
     *
     * @param entities Vector of entities to insert
     * @return Result containing vector of primary keys or error
     */
    [[nodiscard]] virtual auto insert_batch(const std::vector<Entity>& entities)
        -> Result<std::vector<PrimaryKey>> {
        std::vector<PrimaryKey> pks;
        pks.reserve(entities.size());

        auto tx_result = db_->transaction([&]() -> VoidResult {
            for (const auto& entity : entities) {
                auto insert_result = insert(entity);
                if (insert_result.is_err()) {
                    return VoidResult(insert_result.error());
                }
                pks.push_back(insert_result.value());
            }
            return kcenon::common::ok();
        });

        if (tx_result.is_err()) {
            return Result<std::vector<PrimaryKey>>(tx_result.error());
        }

        return Result<std::vector<PrimaryKey>>(std::move(pks));
    }

    // =========================================================================
    // Transaction Support
    // =========================================================================

    /**
     * @brief Execute callback within a transaction
     *
     * Automatically begins transaction, executes function, and commits.
     * Rolls back on exception or if function returns error.
     *
     * @tparam Func Function type returning Result<R> or VoidResult
     * @param func Function to execute within transaction
     * @return Result from the function or error
     */
    template <typename Func>
    [[nodiscard]] auto in_transaction(Func&& func)
        -> std::invoke_result_t<Func> {
        using ReturnType = std::invoke_result_t<Func>;

        auto begin_result = db_->begin_transaction();
        if (begin_result.is_err()) {
            if constexpr (std::is_same_v<ReturnType, VoidResult>) {
                return begin_result;
            } else {
                return ReturnType(begin_result.error());
            }
        }

        try {
            auto result = std::forward<Func>(func)();

            if (result.is_err()) {
                (void)db_->rollback();
                return result;
            }

            auto commit_result = db_->commit();
            if (commit_result.is_err()) {
                if constexpr (std::is_same_v<ReturnType, VoidResult>) {
                    return commit_result;
                } else {
                    return ReturnType(commit_result.error());
                }
            }

            return result;
        } catch (const std::exception& e) {
            (void)db_->rollback();

            if constexpr (std::is_same_v<ReturnType, VoidResult>) {
                return VoidResult(kcenon::common::error_info{
                    -1, std::string("Transaction failed: ") + e.what(), "storage"});
            } else {
                return ReturnType(kcenon::common::error_info{
                    -1, std::string("Transaction failed: ") + e.what(), "storage"});
            }
        }
    }

protected:
    // =========================================================================
    // Subclass Interface (Must Implement)
    // =========================================================================

    /**
     * @brief Map database row to entity
     *
     * Subclasses must implement this to convert a database row into an entity.
     *
     * @param row Database row with column name to value mappings
     * @return Entity constructed from the row
     */
    [[nodiscard]] virtual auto map_row_to_entity(const database_row& row) const
        -> Entity = 0;

    /**
     * @brief Map entity to column-value pairs for insert/update
     *
     * Subclasses must implement this to convert an entity into database row.
     *
     * @param entity Entity to convert
     * @return Map of column names to values
     */
    [[nodiscard]] virtual auto entity_to_row(const Entity& entity) const
        -> std::map<std::string, std::string> = 0;

    /**
     * @brief Get primary key value from entity
     *
     * @param entity Entity to extract PK from
     * @return Primary key value
     */
    [[nodiscard]] virtual auto get_pk(const Entity& entity) const
        -> PrimaryKey = 0;

    /**
     * @brief Check if entity has a valid (non-default) primary key
     *
     * @param entity Entity to check
     * @return true if entity has valid PK
     */
    [[nodiscard]] virtual auto has_pk(const Entity& entity) const -> bool = 0;

    /**
     * @brief Get columns for SELECT queries
     *
     * Default implementation returns "*" to select all columns.
     * Subclasses can override to specify explicit column list.
     *
     * @return Vector of column names to select
     */
    [[nodiscard]] virtual auto select_columns() const -> std::vector<std::string> {
        return {"*"};
    }

    // =========================================================================
    // Utility Methods for Subclasses
    // =========================================================================

    /**
     * @brief Get query builder instance
     *
     * @return Query builder configured for current database
     */
    [[nodiscard]] auto create_query_builder() {
#ifdef PACS_WITH_DATABASE_SYSTEM
        return db_->create_query_builder();
#else
        // Return dummy query builder when database_system not available
        return database::query_builder{database::database_types::sqlite};
#endif
    }

    /**
     * @brief Get database adapter
     *
     * @return Shared pointer to database adapter
     */
    [[nodiscard]] auto db() -> std::shared_ptr<pacs_database_adapter> {
        return db_;
    }

    /**
     * @brief Get table name
     *
     * @return Table name
     */
    [[nodiscard]] auto table_name() const -> const std::string& {
        return table_name_;
    }

    /**
     * @brief Get primary key column name
     *
     * @return Primary key column name
     */
    [[nodiscard]] auto pk_column() const -> const std::string& {
        return pk_column_;
    }

private:
    /**
     * @brief Convert primary key to database value string
     *
     * Helper to convert PK of any type to string for query building.
     */
    [[nodiscard]] static auto to_db_value(const PrimaryKey& pk) -> std::string {
        if constexpr (std::is_same_v<PrimaryKey, std::string>) {
            return pk;
        } else {
            return std::to_string(pk);
        }
    }

    std::shared_ptr<pacs_database_adapter> db_;
    std::string table_name_;
    std::string pk_column_;
};

}  // namespace pacs::storage
