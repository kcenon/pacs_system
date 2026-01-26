/**
 * @file base_repository_test.cpp
 * @brief Unit tests for base_repository
 *
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 */

#include "pacs/storage/base_repository.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

namespace pacs::storage {
namespace {

// =============================================================================
// Test Entity
// =============================================================================

struct test_entity {
    int64_t id{0};
    std::string name;
    std::string description;
    int value{0};
};

// =============================================================================
// Test Repository Implementation
// =============================================================================

class test_repository : public base_repository<test_entity, int64_t> {
public:
    explicit test_repository(std::shared_ptr<pacs_database_adapter> db)
        : base_repository(db, "test_entities", "id") {}

protected:
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> test_entity override {
        test_entity entity;

        if (row.find("id") != row.end()) {
            entity.id = std::stoll(row.at("id"));
        }

        if (row.find("name") != row.end()) {
            entity.name = row.at("name");
        }

        if (row.find("description") != row.end()) {
            entity.description = row.at("description");
        }

        if (row.find("value") != row.end()) {
            entity.value = std::stoi(row.at("value"));
        }

        return entity;
    }

    [[nodiscard]] auto entity_to_row(const test_entity& entity) const
        -> std::map<std::string, std::string> override {
        std::map<std::string, std::string> row;

        if (entity.id != 0) {
            row["id"] = std::to_string(entity.id);
        }

        row["name"] = entity.name;
        row["description"] = entity.description;
        row["value"] = std::to_string(entity.value);

        return row;
    }

    [[nodiscard]] auto get_pk(const test_entity& entity) const
        -> int64_t override {
        return entity.id;
    }

    [[nodiscard]] auto has_pk(const test_entity& entity) const -> bool override {
        return entity.id != 0;
    }
};

// =============================================================================
// Test Fixture
// =============================================================================

struct base_repository_fixture {
    base_repository_fixture() {
        // Create temporary test database
        db_path = std::filesystem::temp_directory_path() / "test_base_repo.db";

        // Remove existing test database
        std::filesystem::remove(db_path);

        // Create database adapter
        db = std::make_shared<pacs_database_adapter>(db_path);
        auto connect_result = db->connect();
        REQUIRE(connect_result.is_ok());

        // Create test table
        auto create_result = db->execute(R"(
            CREATE TABLE IF NOT EXISTS test_entities (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                description TEXT,
                value INTEGER DEFAULT 0
            )
        )");
        REQUIRE(create_result.is_ok());

        // Create repository
        repo = std::make_unique<test_repository>(db);
    }

    ~base_repository_fixture() {
        repo.reset();

        if (db) {
            (void)db->disconnect();
            db.reset();
        }

        // Clean up test database
        std::filesystem::remove(db_path);
    }

    std::filesystem::path db_path;
    std::shared_ptr<pacs_database_adapter> db;
    std::unique_ptr<test_repository> repo;
};

// =============================================================================
// Tests
// =============================================================================

TEST_CASE("BaseRepository - Insert entity", "[base_repository]") {
    base_repository_fixture f;

    test_entity entity;
    entity.name = "Test Entity";
    entity.description = "This is a test";
    entity.value = 42;

    auto result = f.repo->insert(entity);
    REQUIRE(result.is_ok());
    CHECK(result.value() > 0);
}

TEST_CASE("BaseRepository - Find by ID", "[base_repository]") {
    base_repository_fixture f;

    // Insert entity
    test_entity entity;
    entity.name = "Findable";
    entity.description = "Can be found";
    entity.value = 123;

    auto insert_result = f.repo->insert(entity);
    REQUIRE(insert_result.is_ok());
    auto id = insert_result.value();

    // Find by ID
    auto find_result = f.repo->find_by_id(id);
    REQUIRE(find_result.is_ok());

    const auto& found = find_result.value();
    CHECK(found.id == id);
    CHECK(found.name == "Findable");
    CHECK(found.description == "Can be found");
    CHECK(found.value == 123);
}

TEST_CASE("BaseRepository - Find by ID not found", "[base_repository]") {
    base_repository_fixture f;

    auto result = f.repo->find_by_id(99999);
    CHECK(result.is_err());
}

TEST_CASE("BaseRepository - Find all", "[base_repository]") {
    base_repository_fixture f;

    // Insert multiple entities
    for (int i = 0; i < 5; ++i) {
        test_entity entity;
        entity.name = "Entity " + std::to_string(i);
        entity.value = i * 10;
        auto result = f.repo->insert(entity);
        REQUIRE(result.is_ok());
    }

    // Find all
    auto result = f.repo->find_all();
    REQUIRE(result.is_ok());

    const auto& entities = result.value();
    CHECK(entities.size() == 5);
}

TEST_CASE("BaseRepository - Find all with limit", "[base_repository]") {
    base_repository_fixture f;

    // Insert 10 entities
    for (int i = 0; i < 10; ++i) {
        test_entity entity;
        entity.name = "Entity " + std::to_string(i);
        auto result = f.repo->insert(entity);
        REQUIRE(result.is_ok());
    }

    // Find with limit
    auto result = f.repo->find_all(3);
    REQUIRE(result.is_ok());

    const auto& entities = result.value();
    CHECK(entities.size() == 3);
}

TEST_CASE("BaseRepository - Update entity", "[base_repository]") {
    base_repository_fixture f;

    // Insert entity
    test_entity entity;
    entity.name = "Original";
    entity.value = 42;

    auto insert_result = f.repo->insert(entity);
    REQUIRE(insert_result.is_ok());
    entity.id = insert_result.value();

    // Update entity
    entity.name = "Updated";
    entity.value = 100;

    auto update_result = f.repo->update(entity);
    REQUIRE(update_result.is_ok());

    // Verify update
    auto find_result = f.repo->find_by_id(entity.id);
    REQUIRE(find_result.is_ok());

    const auto& found = find_result.value();
    CHECK(found.name == "Updated");
    CHECK(found.value == 100);
}

TEST_CASE("BaseRepository - Remove entity", "[base_repository]") {
    base_repository_fixture f;

    // Insert entity
    test_entity entity;
    entity.name = "To Delete";

    auto insert_result = f.repo->insert(entity);
    REQUIRE(insert_result.is_ok());
    auto id = insert_result.value();

    // Delete entity
    auto remove_result = f.repo->remove(id);
    REQUIRE(remove_result.is_ok());

    // Verify deletion
    auto find_result = f.repo->find_by_id(id);
    CHECK(find_result.is_err());
}

TEST_CASE("BaseRepository - Save new entity", "[base_repository]") {
    base_repository_fixture f;

    test_entity entity;
    entity.name = "New Entity";
    entity.value = 777;

    auto result = f.repo->save(entity);
    REQUIRE(result.is_ok());
    CHECK(result.value() > 0);
}

TEST_CASE("BaseRepository - Save existing entity", "[base_repository]") {
    base_repository_fixture f;

    // Insert entity
    test_entity entity;
    entity.name = "Original";
    entity.value = 42;

    auto insert_result = f.repo->insert(entity);
    REQUIRE(insert_result.is_ok());
    entity.id = insert_result.value();

    // Save with updates (should update existing)
    entity.name = "Modified";
    entity.value = 888;

    auto save_result = f.repo->save(entity);
    REQUIRE(save_result.is_ok());
    CHECK(save_result.value() == entity.id);

    // Verify update
    auto find_result = f.repo->find_by_id(entity.id);
    REQUIRE(find_result.is_ok());

    const auto& found = find_result.value();
    CHECK(found.name == "Modified");
    CHECK(found.value == 888);
}

TEST_CASE("BaseRepository - Count", "[base_repository]") {
    base_repository_fixture f;

    // Initially empty
    auto result1 = f.repo->count();
    REQUIRE(result1.is_ok());
    CHECK(result1.value() == 0);

    // Insert 3 entities
    for (int i = 0; i < 3; ++i) {
        test_entity entity;
        entity.name = "Entity " + std::to_string(i);
        auto result = f.repo->insert(entity);
        REQUIRE(result.is_ok());
    }

    // Count should be 3
    auto result2 = f.repo->count();
    REQUIRE(result2.is_ok());
    CHECK(result2.value() == 3);
}

TEST_CASE("BaseRepository - Exists", "[base_repository]") {
    base_repository_fixture f;

    test_entity entity;
    entity.name = "Exists Test";

    auto insert_result = f.repo->insert(entity);
    REQUIRE(insert_result.is_ok());
    auto id = insert_result.value();

    // Entity should exist
    auto exists_result = f.repo->exists(id);
    REQUIRE(exists_result.is_ok());
    CHECK(exists_result.value() == true);

    // Non-existent entity
    auto not_exists_result = f.repo->exists(99999);
    REQUIRE(not_exists_result.is_ok());
    CHECK(not_exists_result.value() == false);
}

}  // namespace
}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
