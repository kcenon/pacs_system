/**
 * @file repository_factory_test.cpp
 * @brief Unit tests for repository_factory class
 *
 * Verifies that canonical and compatibility repositories are lazily
 * initialized and return valid (non-null) instances from the factory.
 *
 * @see Issue #716 - Complete repository_factory migration
 */

#include <catch2/catch_test_macros.hpp>

#include <chrono>

#include <pacs/storage/repository_factory.hpp>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <pacs/storage/migration_runner.hpp>
#include <pacs/storage/pacs_database_adapter.hpp>
#include <pacs/storage/prefetch_repository.hpp>
#include <pacs/storage/prefetch_rule_repository.hpp>
#include <pacs/storage/recent_study_repository.hpp>
#include <pacs/storage/viewer_state_record.hpp>
#include <pacs/storage/viewer_state_record_repository.hpp>
#include <pacs/storage/viewer_state_repository.hpp>
#include <pacs/storage/sync_config_repository.hpp>
#include <pacs/storage/sync_repository.hpp>
#include <pacs/client/prefetch_types.hpp>
#include <pacs/client/sync_types.hpp>

using namespace pacs::storage;
using namespace pacs::client;

namespace {

bool is_sqlite_backend_supported() {
    pacs_database_adapter db(":memory:");
    auto result = db.connect();
    return result.is_ok();
}

class test_database {
public:
    test_database() {
        db_ = std::make_shared<pacs_database_adapter>(":memory:");
        auto conn_result = db_->connect();
        if (conn_result.is_err()) {
            throw std::runtime_error(
                "Failed to connect: " + conn_result.error().message);
        }
        migration_runner runner;
        auto result = runner.run_migrations(*db_);
        if (result.is_err()) {
            throw std::runtime_error(
                "Migration failed: " + result.error().message);
        }
    }

    [[nodiscard]] auto get() const noexcept
        -> std::shared_ptr<pacs_database_adapter> {
        return db_;
    }

private:
    std::shared_ptr<pacs_database_adapter> db_;
};

}  // namespace

// ============================================================================
// Factory Construction
// ============================================================================

TEST_CASE("repository_factory returns shared db adapter",
          "[storage][repository_factory]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }
    test_database tdb;
    repository_factory factory(tdb.get());

    CHECK(factory.db() == tdb.get());
}

// ============================================================================
// All repositories return non-null
// ============================================================================

TEST_CASE("repository_factory returns non-null for canonical and compatibility repositories",
          "[storage][repository_factory]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }
    test_database tdb;
    repository_factory factory(tdb.get());

    SECTION("jobs") { CHECK(factory.jobs() != nullptr); }
    SECTION("annotations") { CHECK(factory.annotations() != nullptr); }
    SECTION("routing_rules") { CHECK(factory.routing_rules() != nullptr); }
    SECTION("nodes") { CHECK(factory.nodes() != nullptr); }
    SECTION("sync_states") { CHECK(factory.sync_states() != nullptr); }
    SECTION("sync_configs") { CHECK(factory.sync_configs() != nullptr); }
    SECTION("sync_conflicts") { CHECK(factory.sync_conflicts() != nullptr); }
    SECTION("sync_history") { CHECK(factory.sync_history() != nullptr); }
    SECTION("key_images") { CHECK(factory.key_images() != nullptr); }
    SECTION("measurements") { CHECK(factory.measurements() != nullptr); }
    SECTION("viewer_states") { CHECK(factory.viewer_states() != nullptr); }
    SECTION("viewer_state_records") {
        CHECK(factory.viewer_state_records() != nullptr);
    }
    SECTION("recent_studies") { CHECK(factory.recent_studies() != nullptr); }
    SECTION("prefetch_queue") { CHECK(factory.prefetch_queue() != nullptr); }
    SECTION("prefetch_rules") { CHECK(factory.prefetch_rules() != nullptr); }
    SECTION("prefetch_history") { CHECK(factory.prefetch_history() != nullptr); }
    SECTION("commitments") { CHECK(factory.commitments() != nullptr); }
}

// ============================================================================
// Lazy initialization returns same instance
// ============================================================================

TEST_CASE("repository_factory caches instances on repeated calls",
          "[storage][repository_factory]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }
    test_database tdb;
    repository_factory factory(tdb.get());

    SECTION("jobs returns same pointer") {
        auto first = factory.jobs();
        auto second = factory.jobs();
        CHECK(first == second);
    }

    SECTION("annotations returns same pointer") {
        auto first = factory.annotations();
        auto second = factory.annotations();
        CHECK(first == second);
    }

    SECTION("routing_rules returns same pointer") {
        auto first = factory.routing_rules();
        auto second = factory.routing_rules();
        CHECK(first == second);
    }

    SECTION("nodes returns same pointer") {
        auto first = factory.nodes();
        auto second = factory.nodes();
        CHECK(first == second);
    }

    SECTION("sync_states returns same pointer") {
        auto first = factory.sync_states();
        auto second = factory.sync_states();
        CHECK(first == second);
    }

    SECTION("sync_configs returns same pointer") {
        auto first = factory.sync_configs();
        auto second = factory.sync_configs();
        CHECK(first == second);
    }

    SECTION("sync_conflicts returns same pointer") {
        auto first = factory.sync_conflicts();
        auto second = factory.sync_conflicts();
        CHECK(first == second);
    }

    SECTION("sync_history returns same pointer") {
        auto first = factory.sync_history();
        auto second = factory.sync_history();
        CHECK(first == second);
    }

    SECTION("viewer_states returns same pointer") {
        auto first = factory.viewer_states();
        auto second = factory.viewer_states();
        CHECK(first == second);
    }

    SECTION("viewer_state_records returns same pointer") {
        auto first = factory.viewer_state_records();
        auto second = factory.viewer_state_records();
        CHECK(first == second);
    }

    SECTION("recent_studies returns same pointer") {
        auto first = factory.recent_studies();
        auto second = factory.recent_studies();
        CHECK(first == second);
    }

    SECTION("prefetch_queue returns same pointer") {
        auto first = factory.prefetch_queue();
        auto second = factory.prefetch_queue();
        CHECK(first == second);
    }

    SECTION("prefetch_rules returns same pointer") {
        auto first = factory.prefetch_rules();
        auto second = factory.prefetch_rules();
        CHECK(first == second);
    }

    SECTION("prefetch_history returns same pointer") {
        auto first = factory.prefetch_history();
        auto second = factory.prefetch_history();
        CHECK(first == second);
    }
}

TEST_CASE("repository_factory returns structured canonical and compatibility sets",
          "[storage][repository_factory]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }
    test_database tdb;
    repository_factory factory(tdb.get());

    const auto canonical = factory.canonical_repositories();
    CHECK(canonical.jobs == factory.jobs());
    CHECK(canonical.annotations == factory.annotations());
    CHECK(canonical.routing_rules == factory.routing_rules());
    CHECK(canonical.nodes == factory.nodes());
    CHECK(canonical.sync.configs == factory.sync_configs());
    CHECK(canonical.sync.conflicts == factory.sync_conflicts());
    CHECK(canonical.sync.history == factory.sync_history());
    CHECK(canonical.key_images == factory.key_images());
    CHECK(canonical.measurements == factory.measurements());
    CHECK(canonical.viewer_state.records == factory.viewer_state_records());
    CHECK(canonical.viewer_state.recent_studies == factory.recent_studies());
    CHECK(canonical.prefetch.rules == factory.prefetch_rules());
    CHECK(canonical.prefetch.history == factory.prefetch_history());
    CHECK(canonical.commitments == factory.commitments());

    const auto compatibility = factory.compatibility_repositories();
    CHECK(compatibility.sync_states == factory.sync_states());
    CHECK(compatibility.viewer_states == factory.viewer_states());
    CHECK(compatibility.prefetch_queue == factory.prefetch_queue());
}

TEST_CASE("repository_factory canonical repositories share persistence with compatibility repositories",
          "[storage][repository_factory]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database tdb;
    repository_factory factory(tdb.get());
    const auto canonical = factory.canonical_repositories();
    const auto compatibility = factory.compatibility_repositories();

    sync_config config;
    config.config_id = "factory-sync-config";
    config.source_node_id = "archive-ae";
    config.name = "Factory Sync Config";

    auto save_config_result = canonical.sync.configs->save(config);
    REQUIRE(save_config_result.is_ok());
    auto loaded_config = compatibility.sync_states->find_config(config.config_id);
    REQUIRE(loaded_config.has_value());
    CHECK(loaded_config->name == config.name);

    viewer_state_record state;
    state.state_id = "factory-viewer-state";
    state.study_uid = "1.2.840.factory.study";
    state.user_id = "viewer-user";
    state.state_json = "{}";
    state.created_at = std::chrono::system_clock::now();
    state.updated_at = state.created_at;

    auto save_state_result = canonical.viewer_state.records->save(state);
    REQUIRE(save_state_result.is_ok());
    auto loaded_state = compatibility.viewer_states->find_state_by_id(state.state_id);
    REQUIRE(loaded_state.has_value());
    CHECK(loaded_state->study_uid == state.study_uid);

    auto record_access_result =
        canonical.viewer_state.recent_studies->record_access("viewer-user",
                                                             state.study_uid);
    REQUIRE(record_access_result.is_ok());
    CHECK(compatibility.viewer_states->count_recent_studies("viewer-user") == 1);

    prefetch_rule rule;
    rule.rule_id = "factory-prefetch-rule";
    rule.name = "Factory Prefetch Rule";
    rule.trigger = prefetch_trigger::manual;
    rule.source_node_ids = {"archive-ae"};

    auto save_rule_result = canonical.prefetch.rules->save(rule);
    REQUIRE(save_rule_result.is_ok());
    auto loaded_rule =
        compatibility.prefetch_queue->find_rule_by_id(rule.rule_id);
    REQUIRE(loaded_rule.has_value());
    CHECK(loaded_rule->name == rule.name);
}

#endif  // PACS_WITH_DATABASE_SYSTEM
