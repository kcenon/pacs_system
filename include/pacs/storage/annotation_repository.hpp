/**
 * @file annotation_repository.hpp
 * @brief Repository for annotation persistence using base_repository pattern
 *
 * This file provides the annotation_repository class for persisting annotation
 * records using the base_repository pattern. Supports CRUD operations and search.
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #581 - Part 1: Data Models and Repositories
 * @see Issue #610 - Phase 4: Repository Migrations
 * @see Issue #650 - Part 2: Migrate annotation, routing, node repositories
 */

#pragma once

#include "pacs/storage/annotation_record.hpp"

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
 * @brief Repository for annotation persistence using base_repository pattern
 *
 * Extends base_repository to inherit standard CRUD operations.
 * Provides database operations for storing and retrieving annotation records.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto db = std::make_shared<pacs_database_adapter>("pacs.db");
 * db->connect();
 * auto repo = annotation_repository(db);
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
class annotation_repository
    : public base_repository<annotation_record, std::string> {
public:
    explicit annotation_repository(std::shared_ptr<pacs_database_adapter> db);
    ~annotation_repository() override = default;

    annotation_repository(const annotation_repository&) = delete;
    auto operator=(const annotation_repository&)
        -> annotation_repository& = delete;
    annotation_repository(annotation_repository&&) noexcept = default;
    auto operator=(annotation_repository&&) noexcept
        -> annotation_repository& = default;

    // =========================================================================
    // Domain-Specific Operations
    // =========================================================================

    /**
     * @brief Find an annotation by primary key (integer pk)
     *
     * @param pk The primary key (integer)
     * @return Result containing the annotation if found, or error
     */
    [[nodiscard]] auto find_by_pk(int64_t pk) -> result_type;

    /**
     * @brief Find annotations by SOP Instance UID
     *
     * @param sop_instance_uid The SOP Instance UID to filter by
     * @return Result containing vector of annotations on the specified instance
     */
    [[nodiscard]] auto find_by_instance(std::string_view sop_instance_uid)
        -> list_result_type;

    /**
     * @brief Find annotations by Study UID
     *
     * @param study_uid The Study UID to filter by
     * @return Result containing vector of annotations in the specified study
     */
    [[nodiscard]] auto find_by_study(std::string_view study_uid)
        -> list_result_type;

    /**
     * @brief Search annotations with query options
     *
     * @param query Query options for filtering and pagination
     * @return Result containing vector of matching annotations or error
     */
    [[nodiscard]] auto search(const annotation_query& query) -> list_result_type;

    /**
     * @brief Update an existing annotation
     *
     * @param record The annotation record with updated values
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_annotation(const annotation_record& record)
        -> VoidResult;

    /**
     * @brief Count annotations matching a query
     *
     * @param query Query options for filtering
     * @return Result containing number of matching annotations
     */
    [[nodiscard]] auto count_matching(const annotation_query& query)
        -> Result<size_t>;

protected:
    // =========================================================================
    // base_repository overrides
    // =========================================================================

    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> annotation_record override;

    [[nodiscard]] auto entity_to_row(const annotation_record& entity) const
        -> std::map<std::string, database_value> override;

    [[nodiscard]] auto get_pk(const annotation_record& entity) const
        -> std::string override;

    [[nodiscard]] auto has_pk(const annotation_record& entity) const
        -> bool override;

    [[nodiscard]] auto select_columns() const
        -> std::vector<std::string> override;

private:
    [[nodiscard]] auto parse_timestamp(const std::string& str) const
        -> std::chrono::system_clock::time_point;

    [[nodiscard]] auto format_timestamp(
        std::chrono::system_clock::time_point tp) const -> std::string;

    [[nodiscard]] static auto serialize_style(const annotation_style& style)
        -> std::string;
    [[nodiscard]] static auto deserialize_style(std::string_view json)
        -> annotation_style;
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
 * @brief Repository for annotation persistence (legacy SQLite interface)
 *
 * This is the legacy interface maintained for builds without database_system.
 * New code should use the base_repository version when PACS_WITH_DATABASE_SYSTEM
 * is defined.
 */
class annotation_repository {
public:
    explicit annotation_repository(sqlite3* db);
    ~annotation_repository();

    annotation_repository(const annotation_repository&) = delete;
    auto operator=(const annotation_repository&) -> annotation_repository& = delete;
    annotation_repository(annotation_repository&&) noexcept;
    auto operator=(annotation_repository&&) noexcept -> annotation_repository&;

    [[nodiscard]] auto save(const annotation_record& record) -> VoidResult;
    [[nodiscard]] auto find_by_id(std::string_view annotation_id) const
        -> std::optional<annotation_record>;
    [[nodiscard]] auto find_by_pk(int64_t pk) const
        -> std::optional<annotation_record>;
    [[nodiscard]] auto find_by_instance(std::string_view sop_instance_uid) const
        -> std::vector<annotation_record>;
    [[nodiscard]] auto find_by_study(std::string_view study_uid) const
        -> std::vector<annotation_record>;
    [[nodiscard]] auto search(const annotation_query& query) const
        -> std::vector<annotation_record>;
    [[nodiscard]] auto update(const annotation_record& record) -> VoidResult;
    [[nodiscard]] auto remove(std::string_view annotation_id) -> VoidResult;
    [[nodiscard]] auto exists(std::string_view annotation_id) const -> bool;
    [[nodiscard]] auto count() const -> size_t;
    [[nodiscard]] auto count(const annotation_query& query) const -> size_t;
    [[nodiscard]] auto is_valid() const noexcept -> bool;

private:
    [[nodiscard]] auto parse_row(void* stmt) const -> annotation_record;

    [[nodiscard]] static auto serialize_style(const annotation_style& style) -> std::string;
    [[nodiscard]] static auto deserialize_style(std::string_view json) -> annotation_style;

    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
