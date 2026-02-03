/**
 * @file key_image_repository.hpp
 * @brief Repository for key image persistence using base_repository pattern
 *
 * This file provides the key_image_repository class for persisting key image
 * records using the base_repository pattern. Supports CRUD operations and search.
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #581 - Part 1: Data Models and Repositories
 * @see Issue #610 - Phase 4: Repository Migrations
 */

#pragma once

#include "pacs/storage/key_image_record.hpp"

#include <kcenon/common/patterns/result.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/storage/base_repository.hpp"

namespace pacs::storage {

/**
 * @brief Repository for key image persistence using base_repository pattern
 *
 * Provides database operations for storing and retrieving key image records.
 * Extends base_repository to inherit standard CRUD operations.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto db = std::make_shared<pacs_database_adapter>("pacs.db");
 * db->connect();
 * auto repo = key_image_repository(db);
 *
 * key_image_record ki;
 * ki.key_image_id = generate_uuid();
 * ki.study_uid = "1.2.3.4.5";
 * ki.sop_instance_uid = "1.2.3.4.5.6";
 * ki.reason = "Significant finding";
 * repo.save(ki);
 *
 * auto images = repo.find_by_study("1.2.3.4.5");
 * @endcode
 */
class key_image_repository
    : public base_repository<key_image_record, std::string> {
public:
    explicit key_image_repository(std::shared_ptr<pacs_database_adapter> db);
    ~key_image_repository() override = default;

    key_image_repository(const key_image_repository&) = delete;
    auto operator=(const key_image_repository&)
        -> key_image_repository& = delete;
    key_image_repository(key_image_repository&&) noexcept = default;
    auto operator=(key_image_repository&&) noexcept
        -> key_image_repository& = default;

    // ========================================================================
    // Domain-Specific Operations
    // ========================================================================

    /**
     * @brief Find a key image by primary key (integer pk)
     *
     * @param pk The primary key (integer)
     * @return Result containing the key image if found, or error
     */
    [[nodiscard]] auto find_by_pk(int64_t pk) -> result_type;

    /**
     * @brief Find key images by Study UID
     *
     * @param study_uid The Study UID to filter by
     * @return Result containing vector of key images in the specified study
     */
    [[nodiscard]] auto find_by_study(std::string_view study_uid)
        -> list_result_type;

    /**
     * @brief Search key images with query options
     *
     * @param query Query options for filtering and pagination
     * @return Result containing vector of matching key images or error
     */
    [[nodiscard]] auto search(const key_image_query& query) -> list_result_type;

    /**
     * @brief Count key images in a study
     *
     * @param study_uid The Study UID to count key images for
     * @return Result containing number of key images in the study or error
     */
    [[nodiscard]] auto count_by_study(std::string_view study_uid)
        -> Result<size_t>;

protected:
    // ========================================================================
    // base_repository overrides
    // ========================================================================

    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> key_image_record override;

    [[nodiscard]] auto entity_to_row(const key_image_record& entity) const
        -> std::map<std::string, database_value> override;

    [[nodiscard]] auto get_pk(const key_image_record& entity) const
        -> std::string override;

    [[nodiscard]] auto has_pk(const key_image_record& entity) const
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

#else  // !PACS_WITH_DATABASE_SYSTEM

// Legacy interface for builds without database_system
struct sqlite3;

namespace pacs::storage {

template <typename T>
using Result = kcenon::common::Result<T>;

using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Repository for key image persistence (legacy SQLite interface)
 *
 * This is the legacy interface maintained for builds without database_system.
 * New code should use the base_repository version when PACS_WITH_DATABASE_SYSTEM
 * is defined.
 */
class key_image_repository {
public:
    explicit key_image_repository(sqlite3* db);
    ~key_image_repository();

    key_image_repository(const key_image_repository&) = delete;
    auto operator=(const key_image_repository&)
        -> key_image_repository& = delete;
    key_image_repository(key_image_repository&&) noexcept;
    auto operator=(key_image_repository&&) noexcept -> key_image_repository&;

    [[nodiscard]] auto save(const key_image_record& record) -> VoidResult;
    [[nodiscard]] auto find_by_id(std::string_view key_image_id) const
        -> std::optional<key_image_record>;
    [[nodiscard]] auto find_by_pk(int64_t pk) const
        -> std::optional<key_image_record>;
    [[nodiscard]] auto find_by_study(std::string_view study_uid) const
        -> std::vector<key_image_record>;
    [[nodiscard]] auto search(const key_image_query& query) const
        -> std::vector<key_image_record>;
    [[nodiscard]] auto remove(std::string_view key_image_id) -> VoidResult;
    [[nodiscard]] auto exists(std::string_view key_image_id) const -> bool;
    [[nodiscard]] auto count() const -> size_t;
    [[nodiscard]] auto count_by_study(std::string_view study_uid) const
        -> size_t;
    [[nodiscard]] auto is_valid() const noexcept -> bool;

private:
    [[nodiscard]] auto parse_row(void* stmt) const -> key_image_record;

    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
