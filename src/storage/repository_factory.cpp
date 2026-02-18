/**
 * @file repository_factory.cpp
 * @brief Implementation of repository factory
 *
 * Provides lazy-initialized repository instances with shared database adapter.
 *
 * @see repository_factory.hpp
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 * @see Issue #610 - Phase 4: Repository Migrations (9 repositories)
 * @see Issue #716 - Complete repository_factory migration for remaining 6
 */

#include "pacs/storage/repository_factory.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/storage/pacs_database_adapter.hpp"

#include "pacs/storage/annotation_repository.hpp"
#include "pacs/storage/commitment_repository.hpp"
#include "pacs/storage/job_repository.hpp"
#include "pacs/storage/key_image_repository.hpp"
#include "pacs/storage/measurement_repository.hpp"
#include "pacs/storage/node_repository.hpp"
#include "pacs/storage/prefetch_repository.hpp"
#include "pacs/storage/routing_repository.hpp"
#include "pacs/storage/sync_repository.hpp"
#include "pacs/storage/viewer_state_repository.hpp"

namespace pacs::storage {

repository_factory::repository_factory(
    std::shared_ptr<pacs_database_adapter> db)
    : db_(std::move(db)) {}

auto repository_factory::jobs() -> std::shared_ptr<job_repository> {
    if (!jobs_) {
        jobs_ = std::make_shared<job_repository>(db_);
    }
    return jobs_;
}

auto repository_factory::annotations()
    -> std::shared_ptr<annotation_repository> {
    if (!annotations_) {
        annotations_ = std::make_shared<annotation_repository>(db_);
    }
    return annotations_;
}

auto repository_factory::routing_rules()
    -> std::shared_ptr<routing_repository> {
    if (!routing_rules_) {
        routing_rules_ = std::make_shared<routing_repository>(db_);
    }
    return routing_rules_;
}

auto repository_factory::nodes() -> std::shared_ptr<node_repository> {
    if (!nodes_) {
        nodes_ = std::make_shared<node_repository>(db_);
    }
    return nodes_;
}

auto repository_factory::sync_states() -> std::shared_ptr<sync_repository> {
    if (!sync_states_) {
        sync_states_ = std::make_shared<sync_repository>(db_);
    }
    return sync_states_;
}

auto repository_factory::key_images()
    -> std::shared_ptr<key_image_repository> {
    if (!key_images_) {
        key_images_ = std::make_shared<key_image_repository>(db_);
    }
    return key_images_;
}

auto repository_factory::measurements()
    -> std::shared_ptr<measurement_repository> {
    if (!measurements_) {
        measurements_ = std::make_shared<measurement_repository>(db_);
    }
    return measurements_;
}

auto repository_factory::viewer_states()
    -> std::shared_ptr<viewer_state_repository> {
    if (!viewer_states_) {
        viewer_states_ = std::make_shared<viewer_state_repository>(db_);
    }
    return viewer_states_;
}

auto repository_factory::prefetch_queue()
    -> std::shared_ptr<prefetch_repository> {
    if (!prefetch_queue_) {
        prefetch_queue_ = std::make_shared<prefetch_repository>(db_);
    }
    return prefetch_queue_;
}

auto repository_factory::commitments()
    -> std::shared_ptr<commitment_repository> {
    if (!commitments_) {
        commitments_ = std::make_shared<commitment_repository>(db_);
    }
    return commitments_;
}

auto repository_factory::db() const
    -> std::shared_ptr<pacs_database_adapter> {
    return db_;
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
