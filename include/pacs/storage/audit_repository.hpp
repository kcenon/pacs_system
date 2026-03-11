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
 * @file audit_repository.hpp
 * @brief Repository for audit log persistence
 *
 * @see Issue #915 - Part 4: Extract UPS and audit lifecycle repositories
 */

#pragma once

#include "pacs/storage/audit_record.hpp"

#include <kcenon/common/patterns/result.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/storage/base_repository.hpp"

namespace pacs::storage {

class audit_repository : public base_repository<audit_record, int64_t> {
public:
    explicit audit_repository(std::shared_ptr<pacs_database_adapter> db);
    ~audit_repository() override = default;

    audit_repository(const audit_repository&) = delete;
    auto operator=(const audit_repository&) -> audit_repository& = delete;
    audit_repository(audit_repository&&) noexcept = default;
    auto operator=(audit_repository&&) noexcept -> audit_repository& = default;

    [[nodiscard]] auto add_audit_log(const audit_record& record)
        -> Result<int64_t>;
    [[nodiscard]] auto query_audit_log(const audit_query& query)
        -> Result<std::vector<audit_record>>;
    [[nodiscard]] auto find_audit_by_pk(int64_t pk)
        -> std::optional<audit_record>;
    [[nodiscard]] auto audit_count() -> Result<size_t>;
    [[nodiscard]] auto cleanup_old_audit_logs(std::chrono::hours age)
        -> Result<size_t>;

protected:
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> audit_record override;
    [[nodiscard]] auto entity_to_row(const audit_record& entity) const
        -> std::map<std::string, database_value> override;
    [[nodiscard]] auto get_pk(const audit_record& entity) const
        -> int64_t override;
    [[nodiscard]] auto has_pk(const audit_record& entity) const
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

}  // namespace pacs::storage

#else

struct sqlite3;

namespace pacs::storage {

template <typename T>
using Result = kcenon::common::Result<T>;

using VoidResult = kcenon::common::VoidResult;

class audit_repository {
public:
    explicit audit_repository(sqlite3* db);
    ~audit_repository();

    audit_repository(const audit_repository&) = delete;
    auto operator=(const audit_repository&) -> audit_repository& = delete;
    audit_repository(audit_repository&&) noexcept;
    auto operator=(audit_repository&&) noexcept -> audit_repository&;

    [[nodiscard]] auto add_audit_log(const audit_record& record)
        -> Result<int64_t>;
    [[nodiscard]] auto query_audit_log(const audit_query& query) const
        -> Result<std::vector<audit_record>>;
    [[nodiscard]] auto find_audit_by_pk(int64_t pk) const
        -> std::optional<audit_record>;
    [[nodiscard]] auto audit_count() const -> Result<size_t>;
    [[nodiscard]] auto cleanup_old_audit_logs(std::chrono::hours age)
        -> Result<size_t>;

private:
    [[nodiscard]] auto parse_audit_row(void* stmt) const -> audit_record;
    [[nodiscard]] static auto to_like_pattern(std::string_view pattern)
        -> std::string;
    [[nodiscard]] static auto parse_timestamp(const std::string& str)
        -> std::chrono::system_clock::time_point;

    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage

#endif
