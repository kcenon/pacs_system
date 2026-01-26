/**
 * @file repository_factory.hpp
 * @brief Factory for creating repository instances with shared database adapter
 *
 * This file provides the repository_factory class that creates repository
 * instances sharing a single database adapter. This centralizes repository
 * lifecycle management and ensures consistent database access.
 *
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 * @see Epic #605 - Migrate Direct Database Access to database_system Abstraction
 */

#pragma once

#include "pacs/storage/pacs_database_adapter.hpp"

#include <memory>

namespace pacs::storage {

// Forward declarations
class job_repository;
class annotation_repository;
class routing_repository;
class node_repository;
class sync_repository;
class key_image_repository;
class measurement_repository;
class viewer_state_repository;
class prefetch_repository;

/**
 * @brief Factory for creating repository instances with shared database adapter
 *
 * Provides centralized repository creation with shared database connection.
 * Repositories are lazily initialized on first access and reused thereafter.
 *
 * **Key Features:**
 * - Single database adapter shared across all repositories
 * - Lazy initialization (repositories created on demand)
 * - Thread-safety: NOT thread-safe, external synchronization required
 *
 * **Usage Pattern:**
 * @code
 * // Create adapter once
 * auto db = std::make_shared<pacs_database_adapter>("pacs.db");
 * db->connect();
 *
 * // Create factory
 * repository_factory factory(db);
 *
 * // Get repositories as needed (lazily initialized)
 * auto jobs = factory.jobs();
 * auto annotations = factory.annotations();
 *
 * // Use repositories
 * jobs->save(job_record);
 * annotations->find_by_study(study_uid);
 * @endcode
 *
 * @example
 * @code
 * // Application-wide singleton pattern
 * class storage_service {
 * public:
 *     static storage_service& instance() {
 *         static storage_service instance;
 *         return instance;
 *     }
 *
 *     repository_factory& factory() { return *factory_; }
 *
 * private:
 *     storage_service() {
 *         db_ = std::make_shared<pacs_database_adapter>("pacs.db");
 *         db_->connect();
 *         factory_ = std::make_unique<repository_factory>(db_);
 *     }
 *
 *     std::shared_ptr<pacs_database_adapter> db_;
 *     std::unique_ptr<repository_factory> factory_;
 * };
 *
 * // Usage anywhere
 * auto& factory = storage_service::instance().factory();
 * auto jobs = factory.jobs();
 * @endcode
 */
class repository_factory {
public:
    /**
     * @brief Construct factory with database adapter
     *
     * @param db Shared pointer to database adapter (must remain valid)
     */
    explicit repository_factory(std::shared_ptr<pacs_database_adapter> db);

    /**
     * @brief Destructor
     */
    ~repository_factory();

    // Non-copyable
    repository_factory(const repository_factory&) = delete;
    auto operator=(const repository_factory&) -> repository_factory& = delete;

    // Movable
    repository_factory(repository_factory&&) noexcept;
    auto operator=(repository_factory&&) noexcept -> repository_factory&;

    // =========================================================================
    // Repository Accessors
    // =========================================================================

    /**
     * @brief Get job repository
     *
     * Lazily creates the repository on first call.
     *
     * @return Shared pointer to job repository
     */
    [[nodiscard]] auto jobs() -> std::shared_ptr<job_repository>;

    /**
     * @brief Get annotation repository
     *
     * @return Shared pointer to annotation repository
     */
    [[nodiscard]] auto annotations() -> std::shared_ptr<annotation_repository>;

    /**
     * @brief Get routing rules repository
     *
     * @return Shared pointer to routing repository
     */
    [[nodiscard]] auto routing_rules() -> std::shared_ptr<routing_repository>;

    /**
     * @brief Get node repository
     *
     * @return Shared pointer to node repository
     */
    [[nodiscard]] auto nodes() -> std::shared_ptr<node_repository>;

    /**
     * @brief Get sync state repository
     *
     * @return Shared pointer to sync repository
     */
    [[nodiscard]] auto sync_states() -> std::shared_ptr<sync_repository>;

    /**
     * @brief Get key image repository
     *
     * @return Shared pointer to key image repository
     */
    [[nodiscard]] auto key_images() -> std::shared_ptr<key_image_repository>;

    /**
     * @brief Get measurement repository
     *
     * @return Shared pointer to measurement repository
     */
    [[nodiscard]] auto measurements() -> std::shared_ptr<measurement_repository>;

    /**
     * @brief Get viewer state repository
     *
     * @return Shared pointer to viewer state repository
     */
    [[nodiscard]] auto viewer_states() -> std::shared_ptr<viewer_state_repository>;

    /**
     * @brief Get prefetch queue repository
     *
     * @return Shared pointer to prefetch repository
     */
    [[nodiscard]] auto prefetch_queue() -> std::shared_ptr<prefetch_repository>;

    // =========================================================================
    // Database Access
    // =========================================================================

    /**
     * @brief Get underlying database adapter
     *
     * Use this for operations not covered by repositories
     * (e.g., schema migrations, custom queries).
     *
     * @return Shared pointer to database adapter
     */
    [[nodiscard]] auto database() -> std::shared_ptr<pacs_database_adapter>;

private:
    std::shared_ptr<pacs_database_adapter> db_;

    // Lazy-initialized repositories
    std::shared_ptr<job_repository> jobs_;
    std::shared_ptr<annotation_repository> annotations_;
    std::shared_ptr<routing_repository> routing_rules_;
    std::shared_ptr<node_repository> nodes_;
    std::shared_ptr<sync_repository> sync_states_;
    std::shared_ptr<key_image_repository> key_images_;
    std::shared_ptr<measurement_repository> measurements_;
    std::shared_ptr<viewer_state_repository> viewer_states_;
    std::shared_ptr<prefetch_repository> prefetch_queue_;
};

}  // namespace pacs::storage
