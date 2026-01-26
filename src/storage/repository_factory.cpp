/**
 * @file repository_factory.cpp
 * @brief Implementation of repository factory
 *
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 * @see Epic #605 - Migrate Direct Database Access to database_system Abstraction
 */

#include "pacs/storage/repository_factory.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/storage/annotation_repository.hpp"
#include "pacs/storage/job_repository.hpp"
#include "pacs/storage/key_image_repository.hpp"
#include "pacs/storage/measurement_repository.hpp"
#include "pacs/storage/node_repository.hpp"
#include "pacs/storage/prefetch_repository.hpp"
#include "pacs/storage/routing_repository.hpp"
#include "pacs/storage/sync_repository.hpp"
#include "pacs/storage/viewer_state_repository.hpp"

namespace pacs::storage {

// =============================================================================
// Construction / Destruction
// =============================================================================

repository_factory::repository_factory(std::shared_ptr<pacs_database_adapter> db)
    : db_(std::move(db)) {}

repository_factory::~repository_factory() = default;

repository_factory::repository_factory(repository_factory&&) noexcept = default;

auto repository_factory::operator=(repository_factory&&) noexcept
    -> repository_factory& = default;

// =============================================================================
// Repository Accessors
// =============================================================================

auto repository_factory::jobs() -> std::shared_ptr<job_repository> {
    if (!jobs_) {
        // Note: Current repositories use sqlite3* directly
        // This will be migrated in Phase 4 to use pacs_database_adapter
        // For now, return nullptr to indicate not yet migrated
        // TODO: Implement after Phase 4 migration
        jobs_ = nullptr;
    }
    return jobs_;
}

auto repository_factory::annotations() -> std::shared_ptr<annotation_repository> {
    if (!annotations_) {
        // TODO: Implement after Phase 4 migration
        annotations_ = nullptr;
    }
    return annotations_;
}

auto repository_factory::routing_rules() -> std::shared_ptr<routing_repository> {
    if (!routing_rules_) {
        // TODO: Implement after Phase 4 migration
        routing_rules_ = nullptr;
    }
    return routing_rules_;
}

auto repository_factory::nodes() -> std::shared_ptr<node_repository> {
    if (!nodes_) {
        // TODO: Implement after Phase 4 migration
        nodes_ = nullptr;
    }
    return nodes_;
}

auto repository_factory::sync_states() -> std::shared_ptr<sync_repository> {
    if (!sync_states_) {
        // TODO: Implement after Phase 4 migration
        sync_states_ = nullptr;
    }
    return sync_states_;
}

auto repository_factory::key_images() -> std::shared_ptr<key_image_repository> {
    if (!key_images_) {
        // TODO: Implement after Phase 4 migration
        key_images_ = nullptr;
    }
    return key_images_;
}

auto repository_factory::measurements() -> std::shared_ptr<measurement_repository> {
    if (!measurements_) {
        // TODO: Implement after Phase 4 migration
        measurements_ = nullptr;
    }
    return measurements_;
}

auto repository_factory::viewer_states() -> std::shared_ptr<viewer_state_repository> {
    if (!viewer_states_) {
        // TODO: Implement after Phase 4 migration
        viewer_states_ = nullptr;
    }
    return viewer_states_;
}

auto repository_factory::prefetch_queue() -> std::shared_ptr<prefetch_repository> {
    if (!prefetch_queue_) {
        // TODO: Implement after Phase 4 migration
        prefetch_queue_ = nullptr;
    }
    return prefetch_queue_;
}

// =============================================================================
// Database Access
// =============================================================================

auto repository_factory::database() -> std::shared_ptr<pacs_database_adapter> {
    return db_;
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
