/**
 * @file sync_history_repository.hpp
 * @brief Repository for sync history records using base_repository pattern
 *
 * @see Issue #610 - Phase 4: Repository Migrations
 */

#pragma once

#include "pacs/storage/base_repository.hpp"
#include "pacs/client/sync_types.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

namespace pacs::storage {

/**
 * @brief Repository for sync history records using base_repository pattern
 */
class sync_history_repository
    : public base_repository<client::sync_history, int64_t> {
public:
    explicit sync_history_repository(std::shared_ptr<pacs_database_adapter> db);

    [[nodiscard]] auto find_by_config(
        std::string_view config_id,
        size_t limit = 100) -> list_result_type;

    [[nodiscard]] auto find_last_for_config(std::string_view config_id)
        -> result_type;

    [[nodiscard]] auto cleanup_old(std::chrono::hours max_age)
        -> Result<size_t>;

protected:
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> client::sync_history override;

    [[nodiscard]] auto entity_to_row(const client::sync_history& entity) const
        -> std::map<std::string, database_value> override;

    [[nodiscard]] auto get_pk(const client::sync_history& entity) const
        -> int64_t override;

    [[nodiscard]] auto has_pk(const client::sync_history& entity) const
        -> bool override;

    [[nodiscard]] auto select_columns() const
        -> std::vector<std::string> override;

private:
    [[nodiscard]] auto parse_timestamp(const std::string& str) const
        -> std::chrono::system_clock::time_point;

    [[nodiscard]] auto format_timestamp(
        std::chrono::system_clock::time_point tp) const -> std::string;

    [[nodiscard]] static auto serialize_errors(
        const std::vector<std::string>& errors) -> std::string;

    [[nodiscard]] static auto deserialize_errors(
        std::string_view json) -> std::vector<std::string>;
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
