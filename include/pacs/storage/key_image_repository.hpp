/**
 * @file key_image_repository.hpp
 * @brief Repository for key image persistence
 *
 * This file provides the key_image_repository class for persisting key image
 * records in the SQLite database. Supports CRUD operations and search.
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #581 - Part 1: Data Models and Repositories
 */

#pragma once

#ifdef PACS_WITH_DATABASE_SYSTEM


#include "pacs/storage/key_image_record.hpp"

#include <kcenon/common/patterns/result.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace pacs::storage {

template <typename T>
using Result = kcenon::common::Result<T>;

using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Repository for key image persistence
 *
 * Provides database operations for storing and retrieving key image records.
 * Uses SQLite for persistence.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto repo = key_image_repository(db_handle);
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
class key_image_repository {
public:
    explicit key_image_repository(sqlite3* db);
    ~key_image_repository();

    key_image_repository(const key_image_repository&) = delete;
    auto operator=(const key_image_repository&) -> key_image_repository& = delete;
    key_image_repository(key_image_repository&&) noexcept;
    auto operator=(key_image_repository&&) noexcept -> key_image_repository&;

    /**
     * @brief Save a key image record
     *
     * If the key image already exists (by key_image_id), updates it.
     * Otherwise, inserts a new record.
     *
     * @param record The key image record to save
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto save(const key_image_record& record) -> VoidResult;

    /**
     * @brief Find a key image by its unique ID
     *
     * @param key_image_id The key image ID to search for
     * @return Optional containing the key image if found
     */
    [[nodiscard]] auto find_by_id(std::string_view key_image_id) const
        -> std::optional<key_image_record>;

    /**
     * @brief Find a key image by primary key
     *
     * @param pk The primary key
     * @return Optional containing the key image if found
     */
    [[nodiscard]] auto find_by_pk(int64_t pk) const
        -> std::optional<key_image_record>;

    /**
     * @brief Find key images by Study UID
     *
     * @param study_uid The Study UID to filter by
     * @return Vector of key images in the specified study
     */
    [[nodiscard]] auto find_by_study(std::string_view study_uid) const
        -> std::vector<key_image_record>;

    /**
     * @brief Search key images with query options
     *
     * @param query Query options for filtering and pagination
     * @return Vector of matching key images
     */
    [[nodiscard]] auto search(const key_image_query& query) const
        -> std::vector<key_image_record>;

    /**
     * @brief Delete a key image by ID
     *
     * @param key_image_id The key image ID to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto remove(std::string_view key_image_id) -> VoidResult;

    /**
     * @brief Check if a key image exists
     *
     * @param key_image_id The key image ID to check
     * @return true if the key image exists
     */
    [[nodiscard]] auto exists(std::string_view key_image_id) const -> bool;

    /**
     * @brief Get total key image count
     *
     * @return Number of key images in the repository
     */
    [[nodiscard]] auto count() const -> size_t;

    /**
     * @brief Count key images in a study
     *
     * @param study_uid The Study UID to count key images for
     * @return Number of key images in the study
     */
    [[nodiscard]] auto count_by_study(std::string_view study_uid) const -> size_t;

    /**
     * @brief Check if the database connection is valid
     *
     * @return true if the database handle is valid
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool;

private:
    [[nodiscard]] auto parse_row(void* stmt) const -> key_image_record;

    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
