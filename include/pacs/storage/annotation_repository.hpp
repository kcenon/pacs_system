/**
 * @file annotation_repository.hpp
 * @brief Repository for annotation persistence
 *
 * This file provides the annotation_repository class for persisting annotation
 * records in the SQLite database. Supports CRUD operations and search.
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #581 - Part 1: Data Models and Repositories
 */

#pragma once

#ifdef PACS_WITH_DATABASE_SYSTEM


#include "pacs/storage/annotation_record.hpp"

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
 * @brief Repository for annotation persistence
 *
 * Provides database operations for storing and retrieving annotation records.
 * Uses SQLite for persistence.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto repo = annotation_repository(db_handle);
 *
 * annotation_record ann;
 * ann.annotation_id = generate_uuid();
 * ann.study_uid = "1.2.3.4.5";
 * ann.type = annotation_type::arrow;
 * ann.geometry_json = R"({"start":{"x":0,"y":0},"end":{"x":100,"y":100}})";
 * repo.save(ann);
 *
 * auto found = repo.find_by_id(ann.annotation_id);
 * @endcode
 */
class annotation_repository {
public:
    explicit annotation_repository(sqlite3* db);
    ~annotation_repository();

    annotation_repository(const annotation_repository&) = delete;
    auto operator=(const annotation_repository&) -> annotation_repository& = delete;
    annotation_repository(annotation_repository&&) noexcept;
    auto operator=(annotation_repository&&) noexcept -> annotation_repository&;

    /**
     * @brief Save an annotation record
     *
     * If the annotation already exists (by annotation_id), updates it.
     * Otherwise, inserts a new record.
     *
     * @param record The annotation record to save
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto save(const annotation_record& record) -> VoidResult;

    /**
     * @brief Find an annotation by its unique ID
     *
     * @param annotation_id The annotation ID to search for
     * @return Optional containing the annotation if found
     */
    [[nodiscard]] auto find_by_id(std::string_view annotation_id) const
        -> std::optional<annotation_record>;

    /**
     * @brief Find an annotation by primary key
     *
     * @param pk The primary key
     * @return Optional containing the annotation if found
     */
    [[nodiscard]] auto find_by_pk(int64_t pk) const
        -> std::optional<annotation_record>;

    /**
     * @brief Find annotations by SOP Instance UID
     *
     * @param sop_instance_uid The SOP Instance UID to filter by
     * @return Vector of annotations on the specified instance
     */
    [[nodiscard]] auto find_by_instance(std::string_view sop_instance_uid) const
        -> std::vector<annotation_record>;

    /**
     * @brief Find annotations by Study UID
     *
     * @param study_uid The Study UID to filter by
     * @return Vector of annotations in the specified study
     */
    [[nodiscard]] auto find_by_study(std::string_view study_uid) const
        -> std::vector<annotation_record>;

    /**
     * @brief Search annotations with query options
     *
     * @param query Query options for filtering and pagination
     * @return Vector of matching annotations
     */
    [[nodiscard]] auto search(const annotation_query& query) const
        -> std::vector<annotation_record>;

    /**
     * @brief Update an existing annotation
     *
     * @param record The annotation record with updated values
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update(const annotation_record& record) -> VoidResult;

    /**
     * @brief Delete an annotation by ID
     *
     * @param annotation_id The annotation ID to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto remove(std::string_view annotation_id) -> VoidResult;

    /**
     * @brief Check if an annotation exists
     *
     * @param annotation_id The annotation ID to check
     * @return true if the annotation exists
     */
    [[nodiscard]] auto exists(std::string_view annotation_id) const -> bool;

    /**
     * @brief Get total annotation count
     *
     * @return Number of annotations in the repository
     */
    [[nodiscard]] auto count() const -> size_t;

    /**
     * @brief Count annotations matching a query
     *
     * @param query Query options for filtering
     * @return Number of matching annotations
     */
    [[nodiscard]] auto count(const annotation_query& query) const -> size_t;

    /**
     * @brief Check if the database connection is valid
     *
     * @return true if the database handle is valid
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool;

private:
    [[nodiscard]] auto parse_row(void* stmt) const -> annotation_record;

    [[nodiscard]] static auto serialize_style(const annotation_style& style) -> std::string;
    [[nodiscard]] static auto deserialize_style(std::string_view json) -> annotation_style;

    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
