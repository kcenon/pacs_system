/**
 * @file base_repository_impl.hpp
 * @brief Template implementation of base_repository
 *
 * This file contains the template method implementations for base_repository.
 * It is automatically included by base_repository.hpp and should not be
 * included directly.
 *
 * @see base_repository.hpp
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 */

#pragma once

#ifndef PACS_STORAGE_BASE_REPOSITORY_HPP_INCLUDED
#error "Do not include base_repository_impl.hpp directly. Include base_repository.hpp instead."
#endif

#include <sstream>

namespace pacs::storage {

// =============================================================================
// Constructor
// =============================================================================

template <typename Entity, typename PrimaryKey>
base_repository<Entity, PrimaryKey>::base_repository(
    std::shared_ptr<pacs_database_adapter> db,
    std::string table_name,
    std::string pk_column)
    : db_(std::move(db)),
      table_name_(std::move(table_name)),
      pk_column_(std::move(pk_column)) {}

// =============================================================================
// Read Operations
// =============================================================================

template <typename Entity, typename PrimaryKey>
auto base_repository<Entity, PrimaryKey>::find_by_id(PrimaryKey id)
    -> result_type {
    if (!db_ || !db_->is_connected()) {
        return Result<Entity>(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = db_->create_query_builder();
    auto columns = select_columns();

    if (columns.empty() || (columns.size() == 1 && columns[0] == "*")) {
        builder.select({"*"});
    } else {
        builder.select(columns);
    }

    builder.from(table_name_);

    // Convert primary key to database_value
    database_value pk_value;
    if constexpr (std::is_integral_v<PrimaryKey>) {
        pk_value = static_cast<int64_t>(id);
    } else if constexpr (std::is_same_v<PrimaryKey, std::string>) {
        pk_value = id;
    } else {
        std::ostringstream oss;
        oss << id;
        pk_value = oss.str();
    }

    builder.where(pk_column_, "=", pk_value);
    builder.limit(1);

    auto query = builder.build();
    auto result = db_->select(query);

    if (result.is_err()) {
        return Result<Entity>(result.error());
    }

    const auto& db_result = result.value();
    if (db_result.empty()) {
        std::string id_str;
        if constexpr (std::is_same_v<PrimaryKey, std::string>) {
            id_str = id;
        } else {
            id_str = std::to_string(id);
        }
        return Result<Entity>(kcenon::common::error_info{
            -1, "Entity not found with id=" + id_str, "storage"});
    }

    try {
        auto entity = map_row_to_entity(db_result[0]);
        return Result<Entity>(std::move(entity));
    } catch (const std::exception& e) {
        return Result<Entity>(kcenon::common::error_info{
            -1, std::string("Failed to map row to entity: ") + e.what(),
            "storage"});
    }
}

template <typename Entity, typename PrimaryKey>
auto base_repository<Entity, PrimaryKey>::find_all(
    std::optional<size_t> limit) -> list_result_type {
    if (!db_ || !db_->is_connected()) {
        return list_result_type(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = db_->create_query_builder();
    auto columns = select_columns();

    if (columns.empty() || (columns.size() == 1 && columns[0] == "*")) {
        builder.select({"*"});
    } else {
        builder.select(columns);
    }

    builder.from(table_name_);

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

    try {
        for (const auto& row : result.value()) {
            entities.push_back(map_row_to_entity(row));
        }
        return list_result_type(std::move(entities));
    } catch (const std::exception& e) {
        return list_result_type(kcenon::common::error_info{
            -1, std::string("Failed to map rows to entities: ") + e.what(),
            "storage"});
    }
}

template <typename Entity, typename PrimaryKey>
auto base_repository<Entity, PrimaryKey>::find_where(
    const std::string& column,
    const std::string& op,
    const database_value& value) -> list_result_type {
    if (!db_ || !db_->is_connected()) {
        return list_result_type(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = db_->create_query_builder();
    auto columns = select_columns();

    if (columns.empty() || (columns.size() == 1 && columns[0] == "*")) {
        builder.select({"*"});
    } else {
        builder.select(columns);
    }

    builder.from(table_name_).where(column, op, value);

    auto query = builder.build();
    auto result = db_->select(query);

    if (result.is_err()) {
        return list_result_type(result.error());
    }

    std::vector<Entity> entities;
    entities.reserve(result.value().size());

    try {
        for (const auto& row : result.value()) {
            entities.push_back(map_row_to_entity(row));
        }
        return list_result_type(std::move(entities));
    } catch (const std::exception& e) {
        return list_result_type(kcenon::common::error_info{
            -1, std::string("Failed to map rows to entities: ") + e.what(),
            "storage"});
    }
}

template <typename Entity, typename PrimaryKey>
auto base_repository<Entity, PrimaryKey>::exists(PrimaryKey id)
    -> Result<bool> {
    if (!db_ || !db_->is_connected()) {
        return Result<bool>(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = db_->create_query_builder();

    // Convert primary key to database_value
    database_value pk_value;
    if constexpr (std::is_integral_v<PrimaryKey>) {
        pk_value = static_cast<int64_t>(id);
    } else if constexpr (std::is_same_v<PrimaryKey, std::string>) {
        pk_value = id;
    } else {
        std::ostringstream oss;
        oss << id;
        pk_value = oss.str();
    }

    builder.select({"COUNT(*) as count"})
        .from(table_name_)
        .where(pk_column_, "=", pk_value);

    auto query = builder.build();
    auto result = db_->select(query);

    if (result.is_err()) {
        return Result<bool>(result.error());
    }

    if (result.value().empty()) {
        return Result<bool>(false);
    }

    try {
        int count = std::stoi(result.value()[0].at("count"));
        return Result<bool>(count > 0);
    } catch (const std::exception& e) {
        return Result<bool>(kcenon::common::error_info{
            -1, std::string("Failed to parse count: ") + e.what(),
            "storage"});
    }
}

template <typename Entity, typename PrimaryKey>
auto base_repository<Entity, PrimaryKey>::count() -> Result<size_t> {
    if (!db_ || !db_->is_connected()) {
        return Result<size_t>(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = db_->create_query_builder();
    builder.select({"COUNT(*) as count"}).from(table_name_);

    auto query = builder.build();
    auto result = db_->select(query);

    if (result.is_err()) {
        return Result<size_t>(result.error());
    }

    if (result.value().empty()) {
        return Result<size_t>(static_cast<size_t>(0));
    }

    try {
        size_t count = std::stoull(result.value()[0].at("count"));
        return Result<size_t>(count);
    } catch (const std::exception& e) {
        return Result<size_t>(kcenon::common::error_info{
            -1, std::string("Failed to parse count: ") + e.what(),
            "storage"});
    }
}

// =============================================================================
// Write Operations
// =============================================================================

template <typename Entity, typename PrimaryKey>
auto base_repository<Entity, PrimaryKey>::save(const Entity& entity)
    -> Result<PrimaryKey> {
    if (has_pk(entity)) {
        auto update_result = update(entity);
        if (update_result.is_err()) {
            return Result<PrimaryKey>(update_result.error());
        }
        return Result<PrimaryKey>(get_pk(entity));
    } else {
        return insert(entity);
    }
}

template <typename Entity, typename PrimaryKey>
auto base_repository<Entity, PrimaryKey>::insert(const Entity& entity)
    -> Result<PrimaryKey> {
    if (!db_ || !db_->is_connected()) {
        return Result<PrimaryKey>(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    try {
        auto row = entity_to_row(entity);
        auto builder = db_->create_query_builder();
        builder.insert_into(table_name_).values(row);

        auto query = builder.build();
        auto result = db_->insert(query);

        if (result.is_err()) {
            return Result<PrimaryKey>(result.error());
        }

        // Get the last insert rowid
        auto rowid = db_->last_insert_rowid();

        if constexpr (std::is_integral_v<PrimaryKey>) {
            return Result<PrimaryKey>(static_cast<PrimaryKey>(rowid));
        } else if constexpr (std::is_same_v<PrimaryKey, std::string>) {
            return Result<PrimaryKey>(std::to_string(rowid));
        } else {
            return Result<PrimaryKey>(kcenon::common::error_info{
                -1, "Unsupported primary key type", "storage"});
        }
    } catch (const std::exception& e) {
        return Result<PrimaryKey>(kcenon::common::error_info{
            -1, std::string("Failed to insert entity: ") + e.what(),
            "storage"});
    }
}

template <typename Entity, typename PrimaryKey>
auto base_repository<Entity, PrimaryKey>::update(const Entity& entity)
    -> VoidResult {
    if (!db_ || !db_->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    if (!has_pk(entity)) {
        return VoidResult(kcenon::common::error_info{
            -1, "Entity does not have a valid primary key for update",
            "storage"});
    }

    try {
        auto row = entity_to_row(entity);
        auto pk = get_pk(entity);

        auto builder = db_->create_query_builder();
        builder.update(table_name_).set(row);

        // Convert primary key to database_value
        database_value pk_value;
        if constexpr (std::is_integral_v<PrimaryKey>) {
            pk_value = static_cast<int64_t>(pk);
        } else if constexpr (std::is_same_v<PrimaryKey, std::string>) {
            pk_value = pk;
        } else {
            std::ostringstream oss;
            oss << pk;
            pk_value = oss.str();
        }

        builder.where(pk_column_, "=", pk_value);

        auto query = builder.build();
        auto result = db_->update(query);

        if (result.is_err()) {
            return VoidResult(result.error());
        }

        if (result.value() == 0) {
            return VoidResult(kcenon::common::error_info{
                -1, "No rows were updated - entity may not exist", "storage"});
        }

        return kcenon::common::ok();
    } catch (const std::exception& e) {
        return VoidResult(kcenon::common::error_info{
            -1, std::string("Failed to update entity: ") + e.what(),
            "storage"});
    }
}

template <typename Entity, typename PrimaryKey>
auto base_repository<Entity, PrimaryKey>::remove(PrimaryKey id)
    -> VoidResult {
    if (!db_ || !db_->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = db_->create_query_builder();

    // Convert primary key to database_value
    database_value pk_value;
    if constexpr (std::is_integral_v<PrimaryKey>) {
        pk_value = static_cast<int64_t>(id);
    } else if constexpr (std::is_same_v<PrimaryKey, std::string>) {
        pk_value = id;
    } else {
        std::ostringstream oss;
        oss << id;
        pk_value = oss.str();
    }

    builder.delete_from(table_name_).where(pk_column_, "=", pk_value);

    auto query = builder.build();
    auto result = db_->remove(query);

    if (result.is_err()) {
        return VoidResult(result.error());
    }

    if (result.value() == 0) {
        return VoidResult(kcenon::common::error_info{
            -1, "No rows were deleted - entity may not exist", "storage"});
    }

    return kcenon::common::ok();
}

template <typename Entity, typename PrimaryKey>
auto base_repository<Entity, PrimaryKey>::remove_where(
    const std::string& column,
    const std::string& op,
    const database_value& value) -> Result<size_t> {
    if (!db_ || !db_->is_connected()) {
        return Result<size_t>(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = db_->create_query_builder();
    builder.delete_from(table_name_).where(column, op, value);

    auto query = builder.build();
    auto result = db_->remove(query);

    if (result.is_err()) {
        return Result<size_t>(result.error());
    }

    return Result<size_t>(static_cast<size_t>(result.value()));
}

// =============================================================================
// Batch Operations
// =============================================================================

template <typename Entity, typename PrimaryKey>
auto base_repository<Entity, PrimaryKey>::insert_batch(
    const std::vector<Entity>& entities) -> Result<std::vector<PrimaryKey>> {
    if (!db_ || !db_->is_connected()) {
        return Result<std::vector<PrimaryKey>>(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    std::vector<PrimaryKey> ids;
    ids.reserve(entities.size());

    auto tx_result = db_->transaction([&]() -> VoidResult {
        for (const auto& entity : entities) {
            auto insert_result = insert(entity);
            if (insert_result.is_err()) {
                return VoidResult(insert_result.error());
            }
            ids.push_back(insert_result.value());
        }
        return kcenon::common::ok();
    });

    if (tx_result.is_err()) {
        return Result<std::vector<PrimaryKey>>(tx_result.error());
    }

    return Result<std::vector<PrimaryKey>>(std::move(ids));
}

template <typename Entity, typename PrimaryKey>
template <typename Func>
auto base_repository<Entity, PrimaryKey>::in_transaction(Func&& func)
    -> std::invoke_result_t<Func> {
    if (!db_ || !db_->is_connected()) {
        using ResultType = std::invoke_result_t<Func>;
        return ResultType(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    return db_->transaction(std::forward<Func>(func));
}

// =============================================================================
// Protected Utility Methods
// =============================================================================

template <typename Entity, typename PrimaryKey>
auto base_repository<Entity, PrimaryKey>::select_columns() const
    -> std::vector<std::string> {
    return {"*"};
}

template <typename Entity, typename PrimaryKey>
auto base_repository<Entity, PrimaryKey>::query_builder()
    -> database::query_builder {
    return db_->create_query_builder();
}

template <typename Entity, typename PrimaryKey>
auto base_repository<Entity, PrimaryKey>::db()
    -> std::shared_ptr<pacs_database_adapter> {
    return db_;
}

template <typename Entity, typename PrimaryKey>
auto base_repository<Entity, PrimaryKey>::table_name() const
    -> const std::string& {
    return table_name_;
}

template <typename Entity, typename PrimaryKey>
auto base_repository<Entity, PrimaryKey>::pk_column() const
    -> const std::string& {
    return pk_column_;
}

}  // namespace pacs::storage
