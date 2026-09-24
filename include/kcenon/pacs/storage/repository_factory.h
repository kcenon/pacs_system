// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file repository_factory.h
 * @brief Factory for creating repository instances
 *
 * This file provides a factory class for creating repository instances with
 * a shared database adapter. This ensures consistent database connection
 * management across all repositories and simplifies dependency injection.
 *
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 * @see Epic #605 - Migrate Direct Database Access to database_system
 * Abstraction
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <memory>

#ifdef PACS_WITH_DATABASE_SYSTEM

namespace kcenon::pacs::storage {

// Forward declarations
class pacs_database_adapter;
class patient_repository;
class study_repository;
class series_repository;
class instance_repository;
class mpps_repository;
class worklist_repository;
class ups_repository;
class audit_repository;
class job_repository;
class annotation_repository;
class routing_repository;
class node_repository;
class sync_repository;
class sync_config_repository;
class sync_conflict_repository;
class sync_history_repository;
class key_image_repository;
class measurement_repository;
class viewer_state_repository;
class viewer_state_record_repository;
class recent_study_repository;
class prefetch_repository;
class prefetch_rule_repository;
class prefetch_history_repository;
class commitment_repository;

/**
 * @brief Canonical sync repository set for higher-level PACS wiring
 */
struct sync_repository_set {
    std::shared_ptr<sync_config_repository> configs;
    std::shared_ptr<sync_conflict_repository> conflicts;
    std::shared_ptr<sync_history_repository> history;
};

/**
 * @brief Canonical viewer-state repository set for higher-level PACS wiring
 */
struct viewer_state_repository_set {
    std::shared_ptr<viewer_state_record_repository> records;
    std::shared_ptr<recent_study_repository> recent_studies;
};

/**
 * @brief Canonical prefetch repository set for higher-level PACS wiring
 */
struct prefetch_repository_set {
    std::shared_ptr<prefetch_rule_repository> rules;
    std::shared_ptr<prefetch_history_repository> history;
};

/**
 * @brief Canonical PACS repository-set contract for higher-level service wiring
 *
 * This contract exposes the split repositories that own PACS persistence
 * semantics. Higher-level services should prefer this set over aggregate
 * compatibility repositories.
 */
struct canonical_repository_set {
    std::shared_ptr<patient_repository> patients;
    std::shared_ptr<study_repository> studies;
    std::shared_ptr<series_repository> series;
    std::shared_ptr<instance_repository> instances;
    std::shared_ptr<mpps_repository> mpps;
    std::shared_ptr<worklist_repository> worklist;
    std::shared_ptr<ups_repository> ups;
    std::shared_ptr<audit_repository> audit;
    std::shared_ptr<job_repository> jobs;
    std::shared_ptr<annotation_repository> annotations;
    std::shared_ptr<routing_repository> routing_rules;
    std::shared_ptr<node_repository> nodes;
    sync_repository_set sync;
    std::shared_ptr<key_image_repository> key_images;
    std::shared_ptr<measurement_repository> measurements;
    viewer_state_repository_set viewer_state;
    prefetch_repository_set prefetch;
    std::shared_ptr<commitment_repository> commitments;
};

/**
 * @brief Compatibility repository contract kept for incremental migration
 *
 * These aggregate repositories remain available while higher layers are moved
 * to canonical split repository sets.
 */
struct compatibility_repository_set {
    std::shared_ptr<sync_repository> sync_states;
    std::shared_ptr<viewer_state_repository> viewer_states;
    std::shared_ptr<prefetch_repository> prefetch_queue;
};

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
 * @par Example:
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
     * @brief Get or create patient repository
     *
     * @return Shared pointer to patient repository
     */
    [[nodiscard]] auto patients() -> std::shared_ptr<patient_repository>;

    /**
     * @brief Get or create study repository
     *
     * @return Shared pointer to study repository
     */
    [[nodiscard]] auto studies() -> std::shared_ptr<study_repository>;

    /**
     * @brief Get or create series repository
     *
     * @return Shared pointer to series repository
     */
    [[nodiscard]] auto series() -> std::shared_ptr<series_repository>;

    /**
     * @brief Get or create instance repository
     *
     * @return Shared pointer to instance repository
     */
    [[nodiscard]] auto instances() -> std::shared_ptr<instance_repository>;

    /**
     * @brief Get or create MPPS repository
     *
     * @return Shared pointer to MPPS repository
     */
    [[nodiscard]] auto mpps() -> std::shared_ptr<mpps_repository>;

    /**
     * @brief Get or create worklist repository
     *
     * @return Shared pointer to worklist repository
     */
    [[nodiscard]] auto worklist() -> std::shared_ptr<worklist_repository>;

    /**
     * @brief Get or create UPS repository
     *
     * @return Shared pointer to UPS repository
     */
    [[nodiscard]] auto ups() -> std::shared_ptr<ups_repository>;

    /**
     * @brief Get or create audit repository
     *
     * @return Shared pointer to audit repository
     */
    [[nodiscard]] auto audit() -> std::shared_ptr<audit_repository>;

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
     * @brief Get or create aggregate sync repository
     *
     * Compatibility-only contract. Prefer sync_configs(), sync_conflicts(),
     * and sync_history() for new higher-level wiring.
     * @return Shared pointer to sync repository
     */
    [[nodiscard]] auto sync_states() -> std::shared_ptr<sync_repository>;

    /**
     * @brief Get or create canonical sync config repository
     */
    [[nodiscard]] auto sync_configs()
        -> std::shared_ptr<sync_config_repository>;

    /**
     * @brief Get or create canonical sync conflict repository
     */
    [[nodiscard]] auto sync_conflicts()
        -> std::shared_ptr<sync_conflict_repository>;

    /**
     * @brief Get or create canonical sync history repository
     */
    [[nodiscard]] auto sync_history()
        -> std::shared_ptr<sync_history_repository>;

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
     * @brief Get or create aggregate viewer state repository
     *
     * Compatibility-only contract. Prefer viewer_state_records() and
     * recent_studies() for new higher-level wiring.
     * @return Shared pointer to viewer state repository
     */
    [[nodiscard]] auto viewer_states()
        -> std::shared_ptr<viewer_state_repository>;

    /**
     * @brief Get or create canonical viewer state record repository
     */
    [[nodiscard]] auto viewer_state_records()
        -> std::shared_ptr<viewer_state_record_repository>;

    /**
     * @brief Get or create canonical recent study repository
     */
    [[nodiscard]] auto recent_studies()
        -> std::shared_ptr<recent_study_repository>;

    /**
     * @brief Get or create aggregate prefetch repository
     *
     * Compatibility-only contract. Prefer prefetch_rules() and
     * prefetch_history() for new higher-level wiring.
     * @return Shared pointer to prefetch repository
     */
    [[nodiscard]] auto prefetch_queue()
        -> std::shared_ptr<prefetch_repository>;

    /**
     * @brief Get or create canonical prefetch rule repository
     */
    [[nodiscard]] auto prefetch_rules()
        -> std::shared_ptr<prefetch_rule_repository>;

    /**
     * @brief Get or create canonical prefetch history repository
     */
    [[nodiscard]] auto prefetch_history()
        -> std::shared_ptr<prefetch_history_repository>;

    /**
     * @brief Get or create commitment repository
     *
     * @return Shared pointer to commitment repository
     */
    [[nodiscard]] auto commitments()
        -> std::shared_ptr<commitment_repository>;

    /**
     * @brief Get canonical repository set for higher-level service wiring
     */
    [[nodiscard]] auto canonical_repositories() -> canonical_repository_set;

    /**
     * @brief Get compatibility-only aggregate repository set
     */
    [[nodiscard]] auto compatibility_repositories()
        -> compatibility_repository_set;

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
    std::shared_ptr<patient_repository> patients_;
    std::shared_ptr<study_repository> studies_;
    std::shared_ptr<series_repository> series_;
    std::shared_ptr<instance_repository> instances_;
    std::shared_ptr<mpps_repository> mpps_;
    std::shared_ptr<worklist_repository> worklist_;
    std::shared_ptr<ups_repository> ups_;
    std::shared_ptr<audit_repository> audit_;
    std::shared_ptr<job_repository> jobs_;
    std::shared_ptr<annotation_repository> annotations_;
    std::shared_ptr<routing_repository> routing_rules_;
    std::shared_ptr<node_repository> nodes_;
    std::shared_ptr<sync_repository> sync_states_;
    std::shared_ptr<sync_config_repository> sync_configs_;
    std::shared_ptr<sync_conflict_repository> sync_conflicts_;
    std::shared_ptr<sync_history_repository> sync_history_;
    std::shared_ptr<key_image_repository> key_images_;
    std::shared_ptr<measurement_repository> measurements_;
    std::shared_ptr<viewer_state_repository> viewer_states_;
    std::shared_ptr<viewer_state_record_repository> viewer_state_records_;
    std::shared_ptr<recent_study_repository> recent_studies_;
    std::shared_ptr<prefetch_repository> prefetch_queue_;
    std::shared_ptr<prefetch_rule_repository> prefetch_rules_;
    std::shared_ptr<prefetch_history_repository> prefetch_history_;
    std::shared_ptr<commitment_repository> commitments_;
};

}  // namespace kcenon::pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
