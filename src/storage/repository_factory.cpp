/**
 * @file repository_factory.cpp
 * @brief Implementation of repository factory
 *
 * Provides lazy-initialized repository instances with shared database adapter.
 *
 * @see repository_factory.hpp
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 * @see Issue #610 - Phase 4: Repository Migrations (9 repositories)
 */

#include "pacs/storage/repository_factory.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/storage/pacs_database_adapter.hpp"

// Migrated repositories (Issue #610)
#include "pacs/storage/key_image_repository.hpp"
#include "pacs/storage/measurement_repository.hpp"

// TODO(Issue #610): Uncomment these includes when remaining repositories are
// migrated
// #include "pacs/storage/annotation_repository.hpp"
// #include "pacs/storage/job_repository.hpp"
// #include "pacs/storage/node_repository.hpp"
// #include "pacs/storage/prefetch_repository.hpp"
// #include "pacs/storage/routing_repository.hpp"
// #include "pacs/storage/sync_repository.hpp"
// #include "pacs/storage/viewer_state_repository.hpp"

namespace pacs::storage {

repository_factory::repository_factory(
    std::shared_ptr<pacs_database_adapter> db)
    : db_(std::move(db)) {}

// TODO(Issue #610): Implement these methods after repository migration
// Currently, all repositories use sqlite3* directly and must be migrated
// to accept std::shared_ptr<pacs_database_adapter> in their constructors.

auto repository_factory::jobs() -> std::shared_ptr<job_repository> {
    // TODO: Implement after job_repository migration
    return nullptr;
}

auto repository_factory::annotations()
    -> std::shared_ptr<annotation_repository> {
    // TODO: Implement after annotation_repository migration
    return nullptr;
}

auto repository_factory::routing_rules()
    -> std::shared_ptr<routing_repository> {
    // TODO: Implement after routing_repository migration
    return nullptr;
}

auto repository_factory::nodes() -> std::shared_ptr<node_repository> {
    // TODO: Implement after node_repository migration
    return nullptr;
}

auto repository_factory::sync_states() -> std::shared_ptr<sync_repository> {
    // TODO: Implement after sync_repository migration
    return nullptr;
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
    // TODO: Implement after viewer_state_repository migration
    return nullptr;
}

auto repository_factory::prefetch_queue()
    -> std::shared_ptr<prefetch_repository> {
    // TODO: Implement after prefetch_repository migration
    return nullptr;
}

auto repository_factory::db() const
    -> std::shared_ptr<pacs_database_adapter> {
    return db_;
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
