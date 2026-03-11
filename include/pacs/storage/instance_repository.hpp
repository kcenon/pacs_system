// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file instance_repository.hpp
 * @brief Repository for instance metadata persistence using base_repository pattern
 *
 * Extracted from index_database as part of the storage decomposition (Issue #896).
 * Provides focused CRUD operations and file path lookups for instance records.
 *
 * @see Issue #896 - Decompose index_database into metadata and lifecycle stores
 * @see Issue #913 - Part 2: Extract series and instance metadata repositories
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "pacs/storage/instance_record.hpp"

#include <kcenon/common/patterns/result.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/storage/base_repository.hpp"

namespace pacs::storage {

/**
 * @brief Repository for instance metadata persistence using base_repository pattern
 */
class instance_repository : public base_repository<instance_record, int64_t> {
public:
    explicit instance_repository(std::shared_ptr<pacs_database_adapter> db);
    ~instance_repository() override = default;

    instance_repository(const instance_repository&) = delete;
    auto operator=(const instance_repository&) -> instance_repository& = delete;
    instance_repository(instance_repository&&) noexcept = default;
    auto operator=(instance_repository&&) noexcept -> instance_repository& = default;

    [[nodiscard]] auto upsert_instance(int64_t series_pk,
                                       std::string_view sop_uid,
                                       std::string_view sop_class_uid,
                                       std::string_view file_path,
                                       int64_t file_size,
                                       std::string_view transfer_syntax = "",
                                       std::optional<int> instance_number = std::nullopt)
        -> Result<int64_t>;
    [[nodiscard]] auto upsert_instance(const instance_record& record)
        -> Result<int64_t>;
    [[nodiscard]] auto find_instance(std::string_view sop_uid)
        -> std::optional<instance_record>;
    [[nodiscard]] auto find_instance_by_pk(int64_t pk)
        -> std::optional<instance_record>;
    [[nodiscard]] auto list_instances(std::string_view series_uid)
        -> Result<std::vector<instance_record>>;
    [[nodiscard]] auto search_instances(const instance_query& query)
        -> Result<std::vector<instance_record>>;
    [[nodiscard]] auto delete_instance(std::string_view sop_uid)
        -> VoidResult;
    [[nodiscard]] auto instance_count() -> Result<size_t>;
    [[nodiscard]] auto instance_count(std::string_view series_uid)
        -> Result<size_t>;
    [[nodiscard]] auto get_file_path(std::string_view sop_instance_uid)
        -> Result<std::optional<std::string>>;
    [[nodiscard]] auto get_study_files(std::string_view study_instance_uid)
        -> Result<std::vector<std::string>>;
    [[nodiscard]] auto get_series_files(std::string_view series_instance_uid)
        -> Result<std::vector<std::string>>;

protected:
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> instance_record override;
    [[nodiscard]] auto entity_to_row(const instance_record& entity) const
        -> std::map<std::string, database_value> override;
    [[nodiscard]] auto get_pk(const instance_record& entity) const
        -> int64_t override;
    [[nodiscard]] auto has_pk(const instance_record& entity) const
        -> bool override;
    [[nodiscard]] auto select_columns() const -> std::vector<std::string> override;

private:
    [[nodiscard]] auto parse_timestamp(const std::string& str) const
        -> std::chrono::system_clock::time_point;
    [[nodiscard]] auto format_timestamp(
        std::chrono::system_clock::time_point tp) const -> std::string;
};

}  // namespace pacs::storage

#else  // !PACS_WITH_DATABASE_SYSTEM

struct sqlite3;

namespace pacs::storage {

template <typename T>
using Result = kcenon::common::Result<T>;

using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Repository for instance metadata persistence (legacy SQLite interface)
 */
class instance_repository {
public:
    explicit instance_repository(sqlite3* db);
    ~instance_repository();

    instance_repository(const instance_repository&) = delete;
    auto operator=(const instance_repository&) -> instance_repository& = delete;
    instance_repository(instance_repository&&) noexcept;
    auto operator=(instance_repository&&) noexcept -> instance_repository&;

    [[nodiscard]] auto upsert_instance(int64_t series_pk,
                                       std::string_view sop_uid,
                                       std::string_view sop_class_uid,
                                       std::string_view file_path,
                                       int64_t file_size,
                                       std::string_view transfer_syntax = "",
                                       std::optional<int> instance_number = std::nullopt)
        -> Result<int64_t>;
    [[nodiscard]] auto upsert_instance(const instance_record& record)
        -> Result<int64_t>;
    [[nodiscard]] auto find_instance(std::string_view sop_uid) const
        -> std::optional<instance_record>;
    [[nodiscard]] auto find_instance_by_pk(int64_t pk) const
        -> std::optional<instance_record>;
    [[nodiscard]] auto list_instances(std::string_view series_uid) const
        -> Result<std::vector<instance_record>>;
    [[nodiscard]] auto search_instances(const instance_query& query) const
        -> Result<std::vector<instance_record>>;
    [[nodiscard]] auto delete_instance(std::string_view sop_uid)
        -> VoidResult;
    [[nodiscard]] auto instance_count() const -> Result<size_t>;
    [[nodiscard]] auto instance_count(std::string_view series_uid) const
        -> Result<size_t>;
    [[nodiscard]] auto get_file_path(std::string_view sop_instance_uid) const
        -> Result<std::optional<std::string>>;
    [[nodiscard]] auto get_study_files(std::string_view study_instance_uid) const
        -> Result<std::vector<std::string>>;
    [[nodiscard]] auto get_series_files(std::string_view series_instance_uid) const
        -> Result<std::vector<std::string>>;

private:
    [[nodiscard]] auto parse_instance_row(void* stmt) const -> instance_record;
    [[nodiscard]] static auto parse_timestamp(const std::string& str)
        -> std::chrono::system_clock::time_point;

    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
