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
 * @file prefetch_rule_repository.hpp
 * @brief Repository for prefetch rules using base_repository pattern
 *
 * @see Issue #610 - Phase 4: Repository Migrations
 */

#pragma once

#include "pacs/storage/base_repository.hpp"
#include "pacs/client/prefetch_types.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

namespace pacs::storage {

/**
 * @brief Repository for prefetch rules using base_repository pattern
 */
class prefetch_rule_repository
    : public base_repository<client::prefetch_rule, std::string> {
public:
    explicit prefetch_rule_repository(
        std::shared_ptr<pacs_database_adapter> db);

    [[nodiscard]] auto find_by_rule_id(std::string_view rule_id)
        -> result_type;

    [[nodiscard]] auto find_enabled() -> list_result_type;

    [[nodiscard]] auto find_by_trigger(client::prefetch_trigger trigger)
        -> list_result_type;

    [[nodiscard]] auto enable(std::string_view rule_id) -> VoidResult;

    [[nodiscard]] auto disable(std::string_view rule_id) -> VoidResult;

    [[nodiscard]] auto increment_triggered(std::string_view rule_id)
        -> VoidResult;

    [[nodiscard]] auto increment_studies_prefetched(
        std::string_view rule_id,
        size_t count = 1) -> VoidResult;

protected:
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> client::prefetch_rule override;

    [[nodiscard]] auto entity_to_row(const client::prefetch_rule& entity) const
        -> std::map<std::string, database_value> override;

    [[nodiscard]] auto get_pk(const client::prefetch_rule& entity) const
        -> std::string override;

    [[nodiscard]] auto has_pk(const client::prefetch_rule& entity) const
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
