/**
 * @file repository_factory_test.cpp
 * @brief Unit tests for repository_factory
 *
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 */

#include "pacs/storage/repository_factory.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

namespace pacs::storage {
namespace {

// =============================================================================
// Test Fixture
// =============================================================================

struct repository_factory_fixture {
    repository_factory_fixture() {
        // Create temporary test database
        db_path = std::filesystem::temp_directory_path() / "test_factory.db";

        // Remove existing test database
        std::filesystem::remove(db_path);

        // Create database adapter
        db = std::make_shared<pacs_database_adapter>(db_path);
        auto connect_result = db->connect();
        REQUIRE(connect_result.is_ok());

        // Create factory
        factory = std::make_unique<repository_factory>(db);
    }

    ~repository_factory_fixture() {
        factory.reset();

        if (db) {
            (void)db->disconnect();
            db.reset();
        }

        // Clean up test database
        std::filesystem::remove(db_path);
    }

    std::filesystem::path db_path;
    std::shared_ptr<pacs_database_adapter> db;
    std::unique_ptr<repository_factory> factory;
};

// =============================================================================
// Tests
// =============================================================================

TEST_CASE("RepositoryFactory - Construct with database adapter", "[repository_factory]") {
    repository_factory_fixture f;

    CHECK(f.factory != nullptr);
}

TEST_CASE("RepositoryFactory - Get database adapter", "[repository_factory]") {
    repository_factory_fixture f;

    auto db = f.factory->database();
    CHECK(db != nullptr);
    CHECK(db->is_connected());
}

TEST_CASE("RepositoryFactory - Get jobs repository", "[repository_factory]") {
    repository_factory_fixture f;

    auto jobs = f.factory->jobs();
    // Currently returns nullptr (to be implemented in Phase 4)
    CHECK(jobs == nullptr);
}

TEST_CASE("RepositoryFactory - Get annotations repository", "[repository_factory]") {
    repository_factory_fixture f;

    auto annotations = f.factory->annotations();
    // Currently returns nullptr (to be implemented in Phase 4)
    CHECK(annotations == nullptr);
}

TEST_CASE("RepositoryFactory - Get routing rules repository", "[repository_factory]") {
    repository_factory_fixture f;

    auto routing = f.factory->routing_rules();
    CHECK(routing == nullptr);
}

TEST_CASE("RepositoryFactory - Get nodes repository", "[repository_factory]") {
    repository_factory_fixture f;

    auto nodes = f.factory->nodes();
    CHECK(nodes == nullptr);
}

TEST_CASE("RepositoryFactory - Get sync states repository", "[repository_factory]") {
    repository_factory_fixture f;

    auto sync = f.factory->sync_states();
    CHECK(sync == nullptr);
}

TEST_CASE("RepositoryFactory - Get key images repository", "[repository_factory]") {
    repository_factory_fixture f;

    auto key_images = f.factory->key_images();
    CHECK(key_images == nullptr);
}

TEST_CASE("RepositoryFactory - Get measurements repository", "[repository_factory]") {
    repository_factory_fixture f;

    auto measurements = f.factory->measurements();
    CHECK(measurements == nullptr);
}

TEST_CASE("RepositoryFactory - Get viewer states repository", "[repository_factory]") {
    repository_factory_fixture f;

    auto viewer_states = f.factory->viewer_states();
    CHECK(viewer_states == nullptr);
}

TEST_CASE("RepositoryFactory - Get prefetch queue repository", "[repository_factory]") {
    repository_factory_fixture f;

    auto prefetch = f.factory->prefetch_queue();
    CHECK(prefetch == nullptr);
}

}  // namespace
}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
