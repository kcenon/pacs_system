// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file mpps_repository.hpp
 * @brief Repository for MPPS lifecycle persistence using base_repository pattern
 *
 * @see Issue #914 - Part 3: Extract MPPS and worklist lifecycle repositories
 */

#pragma once

#include "pacs/storage/mpps_record.hpp"

#include <kcenon/common/patterns/result.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/storage/base_repository.hpp"

namespace kcenon::pacs::storage {

class mpps_repository : public base_repository<mpps_record, int64_t> {
public:
    explicit mpps_repository(std::shared_ptr<pacs_database_adapter> db);
    ~mpps_repository() override = default;

    mpps_repository(const mpps_repository&) = delete;
    auto operator=(const mpps_repository&) -> mpps_repository& = delete;
    mpps_repository(mpps_repository&&) noexcept = default;
    auto operator=(mpps_repository&&) noexcept -> mpps_repository& = default;

    [[nodiscard]] auto create_mpps(std::string_view mpps_uid,
                                   std::string_view station_ae = "",
                                   std::string_view modality = "",
                                   std::string_view study_uid = "",
                                   std::string_view accession_no = "",
                                   std::string_view start_datetime = "")
        -> Result<int64_t>;
    [[nodiscard]] auto create_mpps(const mpps_record& record) -> Result<int64_t>;
    [[nodiscard]] auto update_mpps(std::string_view mpps_uid,
                                   std::string_view new_status,
                                   std::string_view end_datetime = "",
                                   std::string_view performed_series = "")
        -> VoidResult;
    [[nodiscard]] auto update_mpps(const mpps_record& record) -> VoidResult;
    [[nodiscard]] auto find_mpps(std::string_view mpps_uid)
        -> std::optional<mpps_record>;
    [[nodiscard]] auto find_mpps_by_pk(int64_t pk)
        -> std::optional<mpps_record>;
    [[nodiscard]] auto list_active_mpps(std::string_view station_ae)
        -> Result<std::vector<mpps_record>>;
    [[nodiscard]] auto find_mpps_by_study(std::string_view study_uid)
        -> Result<std::vector<mpps_record>>;
    [[nodiscard]] auto search_mpps(const mpps_query& query)
        -> Result<std::vector<mpps_record>>;
    [[nodiscard]] auto delete_mpps(std::string_view mpps_uid) -> VoidResult;
    [[nodiscard]] auto mpps_count() -> Result<size_t>;
    [[nodiscard]] auto mpps_count(std::string_view status) -> Result<size_t>;

protected:
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> mpps_record override;
    [[nodiscard]] auto entity_to_row(const mpps_record& entity) const
        -> std::map<std::string, database_value> override;
    [[nodiscard]] auto get_pk(const mpps_record& entity) const
        -> int64_t override;
    [[nodiscard]] auto has_pk(const mpps_record& entity) const
        -> bool override;
    [[nodiscard]] auto select_columns() const -> std::vector<std::string> override;

private:
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

class mpps_repository {
public:
    explicit mpps_repository(sqlite3* db);
    ~mpps_repository();

    mpps_repository(const mpps_repository&) = delete;
    auto operator=(const mpps_repository&) -> mpps_repository& = delete;
    mpps_repository(mpps_repository&&) noexcept;
    auto operator=(mpps_repository&&) noexcept -> mpps_repository&;

    [[nodiscard]] auto create_mpps(std::string_view mpps_uid,
                                   std::string_view station_ae = "",
                                   std::string_view modality = "",
                                   std::string_view study_uid = "",
                                   std::string_view accession_no = "",
                                   std::string_view start_datetime = "")
        -> Result<int64_t>;
    [[nodiscard]] auto create_mpps(const mpps_record& record) -> Result<int64_t>;
    [[nodiscard]] auto update_mpps(std::string_view mpps_uid,
                                   std::string_view new_status,
                                   std::string_view end_datetime = "",
                                   std::string_view performed_series = "")
        -> VoidResult;
    [[nodiscard]] auto update_mpps(const mpps_record& record) -> VoidResult;
    [[nodiscard]] auto find_mpps(std::string_view mpps_uid) const
        -> std::optional<mpps_record>;
    [[nodiscard]] auto find_mpps_by_pk(int64_t pk) const
        -> std::optional<mpps_record>;
    [[nodiscard]] auto list_active_mpps(std::string_view station_ae) const
        -> Result<std::vector<mpps_record>>;
    [[nodiscard]] auto find_mpps_by_study(std::string_view study_uid) const
        -> Result<std::vector<mpps_record>>;
    [[nodiscard]] auto search_mpps(const mpps_query& query) const
        -> Result<std::vector<mpps_record>>;
    [[nodiscard]] auto delete_mpps(std::string_view mpps_uid) -> VoidResult;
    [[nodiscard]] auto mpps_count() const -> Result<size_t>;
    [[nodiscard]] auto mpps_count(std::string_view status) const
        -> Result<size_t>;

private:
    [[nodiscard]] auto parse_mpps_row(void* stmt) const -> mpps_record;
    [[nodiscard]] static auto parse_timestamp(const std::string& str)
        -> std::chrono::system_clock::time_point;

    sqlite3* db_{nullptr};
};

}  // namespace kcenon::pacs::storage

#endif
