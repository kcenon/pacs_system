/**
 * @file repository_factory.hpp
 * @brief Factory for creating repository instances
 *
 * This file provides a factory class for creating repository instances with
 * a shared database adapter. This ensures consistent database connection
 * management across all repositories and simplifies dependency injection.
 *
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 * @see Epic #605 - Migrate Direct Database Access to database_system
 * Abstraction
 */

#pragma once

#include <memory>

#ifdef PACS_WITH_DATABASE_SYSTEM

namespace pacs::storage {

// Forward declarations
class pacs_database_adapter;
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
 * @brief Factory for creating repository instances with shared database
 * adapter
 *
 * The repository_factory provides a centralized way to create and manage
 * repository instances. It ensures that all repositories share the same
 * database adapter instance, which is important for:
 * - Connection pooling
 * - Transaction management across repositories
 * - Consistent configuration
 *
 * Repositories are lazy-initialized on first access and cached for subsequent
 * calls.
 *
 * **Thread Safety:** This class is NOT thread-safe. If you need concurrent
 * access, either:
 * - Create separate factory instances per thread
 * - Use external synchronization (mutex)
 * - Pre-initialize all repositories before sharing
 *
 * @example
 * @code
 * // Create database adapter
 * auto db = std::make_shared<pacs_database_adapter>("pacs.db");
 * db->connect();
 *
 * // Create factory
 * repository_factory factory(db);
 *
 * // Get repositories
 * auto jobs = factory.jobs();
 * auto annotations = factory.annotations();
 *
 * // Use repositories
 * auto result = jobs->find_by_id(123);
 *
 * // All repositories share the same database connection
 * // Transactions can span multiple repositories:
 * db->transaction([&]() -> VoidResult {
 *     jobs->insert(new_job);
 *     annotations->insert(new_annotation);
 *     return kcenon::common::ok();
 * });
 * @endcode
 */
class repository_factory {
public:
    /**
     * @brief Construct factory with database adapter
     *
     * @param db Shared pointer to database adapter
     */
    explicit repository_factory(std::shared_ptr<pacs_database_adapter> db);

    /// Default destructor
    ~repository_factory() = default;

    // Non-copyable
    repository_factory(const repository_factory&) = delete;
    auto operator=(const repository_factory&) -> repository_factory& = delete;

    // Movable
    repository_factory(repository_factory&&) noexcept = default;
    auto operator=(repository_factory&&) noexcept
        -> repository_factory& = default;

    /**
     * @brief Get or create job repository
     *
     * Returns a cached instance if already created, otherwise creates a new
     * instance.
     *
     * @return Shared pointer to job repository
     */
    [[nodiscard]] auto jobs() -> std::shared_ptr<job_repository>;

    /**
     * @brief Get or create annotation repository
     *
     * @return Shared pointer to annotation repository
     */
    [[nodiscard]] auto annotations() -> std::shared_ptr<annotation_repository>;

    /**
     * @brief Get or create routing repository
     *
     * @return Shared pointer to routing repository
     */
    [[nodiscard]] auto routing_rules() -> std::shared_ptr<routing_repository>;

    /**
     * @brief Get or create node repository
     *
     * @return Shared pointer to node repository
     */
    [[nodiscard]] auto nodes() -> std::shared_ptr<node_repository>;

    /**
     * @brief Get or create sync repository
     *
     * @return Shared pointer to sync repository
     */
    [[nodiscard]] auto sync_states() -> std::shared_ptr<sync_repository>;

    /**
     * @brief Get or create key image repository
     *
     * @return Shared pointer to key image repository
     */
    [[nodiscard]] auto key_images() -> std::shared_ptr<key_image_repository>;

    /**
     * @brief Get or create measurement repository
     *
     * @return Shared pointer to measurement repository
     */
    [[nodiscard]] auto measurements()
        -> std::shared_ptr<measurement_repository>;

    /**
     * @brief Get or create viewer state repository
     *
     * @return Shared pointer to viewer state repository
     */
    [[nodiscard]] auto viewer_states()
        -> std::shared_ptr<viewer_state_repository>;

    /**
     * @brief Get or create prefetch repository
     *
     * @return Shared pointer to prefetch repository
     */
    [[nodiscard]] auto prefetch_queue()
        -> std::shared_ptr<prefetch_repository>;

    /**
     * @brief Get the database adapter
     *
     * @return Shared pointer to database adapter
     */
    [[nodiscard]] auto db() const -> std::shared_ptr<pacs_database_adapter>;

private:
    /// Shared database adapter
    std::shared_ptr<pacs_database_adapter> db_;

    /// Lazy-initialized repositories
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

#endif  // PACS_WITH_DATABASE_SYSTEM
