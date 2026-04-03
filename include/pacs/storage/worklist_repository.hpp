// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file worklist_repository.hpp
 * @brief Repository for modality worklist persistence using base_repository pattern
 *
 * @see Issue #914 - Part 3: Extract MPPS and worklist lifecycle repositories
 */

#pragma once

#include "pacs/storage/worklist_record.hpp"

#include <kcenon/common/patterns/result.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/storage/base_repository.hpp"

namespace kcenon::pacs::storage {

class worklist_repository : public base_repository<worklist_item, int64_t> {
public:
    explicit worklist_repository(std::shared_ptr<pacs_database_adapter> db);
    ~worklist_repository() override = default;

    worklist_repository(const worklist_repository&) = delete;
    auto operator=(const worklist_repository&) -> worklist_repository& = delete;
    worklist_repository(worklist_repository&&) noexcept = default;
    auto operator=(worklist_repository&&) noexcept
        -> worklist_repository& = default;

    [[nodiscard]] auto add_worklist_item(const worklist_item& item)
        -> Result<int64_t>;
    [[nodiscard]] auto update_worklist_status(std::string_view step_id,
                                              std::string_view accession_no,
                                              std::string_view new_status)
        -> VoidResult;
    [[nodiscard]] auto query_worklist(const worklist_query& query)
        -> Result<std::vector<worklist_item>>;
    [[nodiscard]] auto find_worklist_item(std::string_view step_id,
                                          std::string_view accession_no)
        -> std::optional<worklist_item>;
    [[nodiscard]] auto find_worklist_by_pk(int64_t pk)
        -> std::optional<worklist_item>;
    [[nodiscard]] auto delete_worklist_item(std::string_view step_id,
                                            std::string_view accession_no)
        -> VoidResult;
    [[nodiscard]] auto cleanup_old_worklist_items(std::chrono::hours age)
        -> Result<size_t>;
    [[nodiscard]] auto cleanup_worklist_items_before(
        std::chrono::system_clock::time_point before) -> Result<size_t>;
    [[nodiscard]] auto worklist_count() -> Result<size_t>;
    [[nodiscard]] auto worklist_count(std::string_view status)
        -> Result<size_t>;

protected:
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> worklist_item override;
    [[nodiscard]] auto entity_to_row(const worklist_item& entity) const
        -> std::map<std::string, database_value> override;
    [[nodiscard]] auto get_pk(const worklist_item& entity) const
        -> int64_t override;
    [[nodiscard]] auto has_pk(const worklist_item& entity) const
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

class worklist_repository {
public:
    explicit worklist_repository(sqlite3* db);
    ~worklist_repository();

    worklist_repository(const worklist_repository&) = delete;
    auto operator=(const worklist_repository&) -> worklist_repository& = delete;
    worklist_repository(worklist_repository&&) noexcept;
    auto operator=(worklist_repository&&) noexcept -> worklist_repository&;

    [[nodiscard]] auto add_worklist_item(const worklist_item& item)
        -> Result<int64_t>;
    [[nodiscard]] auto update_worklist_status(std::string_view step_id,
                                              std::string_view accession_no,
                                              std::string_view new_status)
        -> VoidResult;
    [[nodiscard]] auto query_worklist(const worklist_query& query) const
        -> Result<std::vector<worklist_item>>;
    [[nodiscard]] auto find_worklist_item(std::string_view step_id,
                                          std::string_view accession_no) const
        -> std::optional<worklist_item>;
    [[nodiscard]] auto find_worklist_by_pk(int64_t pk) const
        -> std::optional<worklist_item>;
    [[nodiscard]] auto delete_worklist_item(std::string_view step_id,
                                            std::string_view accession_no)
        -> VoidResult;
    [[nodiscard]] auto cleanup_old_worklist_items(std::chrono::hours age)
        -> Result<size_t>;
    [[nodiscard]] auto cleanup_worklist_items_before(
        std::chrono::system_clock::time_point before) -> Result<size_t>;
    [[nodiscard]] auto worklist_count() const -> Result<size_t>;
    [[nodiscard]] auto worklist_count(std::string_view status) const
        -> Result<size_t>;

private:
    [[nodiscard]] auto parse_worklist_row(void* stmt) const -> worklist_item;
    [[nodiscard]] static auto to_like_pattern(std::string_view pattern)
        -> std::string;
    [[nodiscard]] static auto parse_timestamp(const std::string& str)
        -> std::chrono::system_clock::time_point;

    sqlite3* db_{nullptr};
};

}  // namespace kcenon::pacs::storage

#endif
