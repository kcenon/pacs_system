// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file ups_repository.hpp
 * @brief Repository for UPS lifecycle and subscription persistence
 *
 * @see Issue #915 - Part 4: Extract UPS and audit lifecycle repositories
 */

#pragma once

#include "pacs/storage/ups_workitem.hpp"

#include <kcenon/common/patterns/result.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/storage/base_repository.hpp"

namespace kcenon::pacs::storage {

class ups_repository : public base_repository<ups_workitem, int64_t> {
public:
    explicit ups_repository(std::shared_ptr<pacs_database_adapter> db);
    ~ups_repository() override = default;

    ups_repository(const ups_repository&) = delete;
    auto operator=(const ups_repository&) -> ups_repository& = delete;
    ups_repository(ups_repository&&) noexcept = default;
    auto operator=(ups_repository&&) noexcept -> ups_repository& = default;

    [[nodiscard]] auto create_ups_workitem(const ups_workitem& workitem)
        -> Result<int64_t>;
    [[nodiscard]] auto update_ups_workitem(const ups_workitem& workitem)
        -> VoidResult;
    [[nodiscard]] auto change_ups_state(std::string_view workitem_uid,
                                        std::string_view new_state,
                                        std::string_view transaction_uid = "")
        -> VoidResult;
    [[nodiscard]] auto find_ups_workitem(std::string_view workitem_uid)
        -> std::optional<ups_workitem>;
    [[nodiscard]] auto search_ups_workitems(const ups_workitem_query& query)
        -> Result<std::vector<ups_workitem>>;
    [[nodiscard]] auto delete_ups_workitem(std::string_view workitem_uid)
        -> VoidResult;
    [[nodiscard]] auto ups_workitem_count() -> Result<size_t>;
    [[nodiscard]] auto ups_workitem_count(std::string_view state)
        -> Result<size_t>;
    [[nodiscard]] auto subscribe_ups(const ups_subscription& subscription)
        -> Result<int64_t>;
    [[nodiscard]] auto unsubscribe_ups(std::string_view subscriber_ae,
                                       std::string_view workitem_uid = "")
        -> VoidResult;
    [[nodiscard]] auto get_ups_subscriptions(std::string_view subscriber_ae)
        -> Result<std::vector<ups_subscription>>;
    [[nodiscard]] auto get_ups_subscribers(std::string_view workitem_uid)
        -> Result<std::vector<std::string>>;

protected:
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> ups_workitem override;
    [[nodiscard]] auto entity_to_row(const ups_workitem& entity) const
        -> std::map<std::string, database_value> override;
    [[nodiscard]] auto get_pk(const ups_workitem& entity) const
        -> int64_t override;
    [[nodiscard]] auto has_pk(const ups_workitem& entity) const
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

class ups_repository {
public:
    explicit ups_repository(sqlite3* db);
    ~ups_repository();

    ups_repository(const ups_repository&) = delete;
    auto operator=(const ups_repository&) -> ups_repository& = delete;
    ups_repository(ups_repository&&) noexcept;
    auto operator=(ups_repository&&) noexcept -> ups_repository&;

    [[nodiscard]] auto create_ups_workitem(const ups_workitem& workitem)
        -> Result<int64_t>;
    [[nodiscard]] auto update_ups_workitem(const ups_workitem& workitem)
        -> VoidResult;
    [[nodiscard]] auto change_ups_state(std::string_view workitem_uid,
                                        std::string_view new_state,
                                        std::string_view transaction_uid = "")
        -> VoidResult;
    [[nodiscard]] auto find_ups_workitem(std::string_view workitem_uid) const
        -> std::optional<ups_workitem>;
    [[nodiscard]] auto search_ups_workitems(const ups_workitem_query& query) const
        -> Result<std::vector<ups_workitem>>;
    [[nodiscard]] auto delete_ups_workitem(std::string_view workitem_uid)
        -> VoidResult;
    [[nodiscard]] auto ups_workitem_count() const -> Result<size_t>;
    [[nodiscard]] auto ups_workitem_count(std::string_view state) const
        -> Result<size_t>;
    [[nodiscard]] auto subscribe_ups(const ups_subscription& subscription)
        -> Result<int64_t>;
    [[nodiscard]] auto unsubscribe_ups(std::string_view subscriber_ae,
                                       std::string_view workitem_uid = "")
        -> VoidResult;
    [[nodiscard]] auto get_ups_subscriptions(std::string_view subscriber_ae) const
        -> Result<std::vector<ups_subscription>>;
    [[nodiscard]] auto get_ups_subscribers(std::string_view workitem_uid) const
        -> Result<std::vector<std::string>>;

private:
    [[nodiscard]] auto parse_ups_workitem_row(void* stmt) const -> ups_workitem;
    [[nodiscard]] static auto to_like_pattern(std::string_view pattern)
        -> std::string;
    [[nodiscard]] static auto parse_timestamp(const std::string& str)
        -> std::chrono::system_clock::time_point;

    sqlite3* db_{nullptr};
};

}  // namespace kcenon::pacs::storage

#endif
