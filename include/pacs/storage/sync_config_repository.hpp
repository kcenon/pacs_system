/**
 * @file sync_config_repository.hpp
 * @brief Repository for sync config records using base_repository pattern
 *
 * @see Issue #610 - Phase 4: Repository Migrations
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 */

#pragma once

#include "pacs/storage/base_repository.hpp"
#include "pacs/client/sync_types.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

namespace pacs::storage {

/**
 * @brief Repository for sync config records using base_repository pattern
 *
 * Provides CRUD operations for sync_config entities using the
 * standardized base_repository interface.
 */
class sync_config_repository
    : public base_repository<client::sync_config, std::string> {
public:
    explicit sync_config_repository(std::shared_ptr<pacs_database_adapter> db);

    // =========================================================================
    // Domain-Specific Queries
    // =========================================================================

    /**
     * @brief Find config by config_id
     */
    [[nodiscard]] auto find_by_config_id(std::string_view config_id)
        -> result_type;

    /**
     * @brief List all enabled configs
     */
    [[nodiscard]] auto find_enabled() -> list_result_type;

    /**
     * @brief List configs by source node
     */
    [[nodiscard]] auto find_by_source_node(std::string_view node_id)
        -> list_result_type;

    /**
     * @brief Update config statistics after sync
     */
    [[nodiscard]] auto update_stats(
        std::string_view config_id,
        bool success,
        size_t studies_synced) -> VoidResult;

protected:
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> client::sync_config override;

    [[nodiscard]] auto entity_to_row(const client::sync_config& entity) const
        -> std::map<std::string, database_value> override;

    [[nodiscard]] auto get_pk(const client::sync_config& entity) const
        -> std::string override;

    [[nodiscard]] auto has_pk(const client::sync_config& entity) const
        -> bool override;

    [[nodiscard]] auto select_columns() const
        -> std::vector<std::string> override;

private:
    [[nodiscard]] auto parse_timestamp(const std::string& str) const
        -> std::chrono::system_clock::time_point;

    [[nodiscard]] auto format_timestamp(
        std::chrono::system_clock::time_point tp) const -> std::string;

    [[nodiscard]] static auto serialize_vector(
        const std::vector<std::string>& vec) -> std::string;

    [[nodiscard]] static auto deserialize_vector(
        std::string_view json) -> std::vector<std::string>;
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
