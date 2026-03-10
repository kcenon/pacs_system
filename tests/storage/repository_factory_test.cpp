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

#include <pacs/client/prefetch_types.hpp>
#include <pacs/client/sync_types.hpp>
#include <pacs/storage/prefetch_repository.hpp>
#include <pacs/storage/prefetch_rule_repository.hpp>
#include <pacs/storage/recent_study_repository.hpp>
#include <pacs/storage/repository_factory.hpp>
#include <pacs/storage/sync_config_repository.hpp>
#include <pacs/storage/sync_repository.hpp>
#include <pacs/storage/viewer_state_repository.hpp>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <pacs/storage/migration_runner.hpp>
#include <pacs/storage/pacs_database_adapter.hpp>

#include <stdexcept>

using namespace pacs::storage;

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

TEST_CASE("repository_factory split and compatibility repositories share state",
          "[storage][repository_factory]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database tdb;
    repository_factory factory(tdb.get());
    const auto canonical = factory.canonical_repositories();
    const auto compatibility = factory.compatibility_repositories();

    pacs::client::sync_config config;
    config.config_id = "sync-config";
    config.source_node_id = "archive";
    config.name = "Shared Sync";
    REQUIRE(canonical.sync.configs->insert(config).is_ok());

    auto loaded_config = compatibility.sync_states->find_config(config.config_id);
    REQUIRE(loaded_config.has_value());
    CHECK(loaded_config->name == config.name);

    pacs::client::prefetch_rule rule;
    rule.rule_id = "prefetch-rule";
    rule.name = "Shared Rule";
    rule.trigger = pacs::client::prefetch_trigger::manual;
    rule.source_node_ids = {"archive"};
    REQUIRE(canonical.prefetch.rules->insert(rule).is_ok());
    CHECK(compatibility.prefetch_queue->rule_exists(rule.rule_id));

    REQUIRE(
        canonical.viewer_state.recent_studies->record_access("user1", "1.2.3").is_ok());
    CHECK(compatibility.viewer_states->count_recent_studies("user1") == 1);
}

#endif  // PACS_WITH_DATABASE_SYSTEM
