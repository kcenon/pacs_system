/**
 * @file repository_factory_test.cpp
 * @brief Unit tests for repository_factory class
 *
 * Verifies that all 10 repositories are lazily initialized and return
 * valid (non-null) instances from the factory.
 *
 * @see Issue #716 - Complete repository_factory migration
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/storage/repository_factory.hpp>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <pacs/storage/migration_runner.hpp>
#include <pacs/storage/pacs_database_adapter.hpp>

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

TEST_CASE("repository_factory returns non-null for all repositories",
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
    SECTION("key_images") { CHECK(factory.key_images() != nullptr); }
    SECTION("measurements") { CHECK(factory.measurements() != nullptr); }
    SECTION("viewer_states") { CHECK(factory.viewer_states() != nullptr); }
    SECTION("prefetch_queue") { CHECK(factory.prefetch_queue() != nullptr); }
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

    SECTION("viewer_states returns same pointer") {
        auto first = factory.viewer_states();
        auto second = factory.viewer_states();
        CHECK(first == second);
    }

    SECTION("prefetch_queue returns same pointer") {
        auto first = factory.prefetch_queue();
        auto second = factory.prefetch_queue();
        CHECK(first == second);
    }
}

#endif  // PACS_WITH_DATABASE_SYSTEM
