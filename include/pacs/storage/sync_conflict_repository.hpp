/**
 * @file sync_conflict_repository.hpp
 * @brief Repository for sync conflict records using base_repository pattern
 *
 * @see Issue #610 - Phase 4: Repository Migrations
 */

#pragma once

#include "pacs/storage/base_repository.hpp"
#include "pacs/client/sync_types.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

namespace pacs::storage {

/**
 * @brief Repository for sync conflict records using base_repository pattern
 */
class sync_conflict_repository
    : public base_repository<client::sync_conflict, std::string> {
public:
    explicit sync_conflict_repository(
        std::shared_ptr<pacs_database_adapter> db);

    [[nodiscard]] auto find_by_study_uid(std::string_view study_uid)
        -> result_type;

    [[nodiscard]] auto find_by_config(std::string_view config_id)
        -> list_result_type;

    [[nodiscard]] auto find_unresolved() -> list_result_type;

    [[nodiscard]] auto resolve(
        std::string_view study_uid,
        client::conflict_resolution resolution) -> VoidResult;

    [[nodiscard]] auto cleanup_old(std::chrono::hours max_age)
        -> Result<size_t>;

protected:
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> client::sync_conflict override;

    [[nodiscard]] auto entity_to_row(const client::sync_conflict& entity) const
        -> std::map<std::string, database_value> override;

    [[nodiscard]] auto get_pk(const client::sync_conflict& entity) const
        -> std::string override;

    [[nodiscard]] auto has_pk(const client::sync_conflict& entity) const
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

#endif  // PACS_WITH_DATABASE_SYSTEM
