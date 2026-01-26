/**
 * @file base_repository_test.cpp
 * @brief Unit tests for base_repository template class
 *
 * Tests the generic repository pattern including CRUD operations,
 * batch operations, and transaction support.
 *
 * @note These tests require unified_database_system to support SQLite backend.
 *       Currently, unified_database_system only supports PostgreSQL.
 *       Integration tests will SKIP until SQLite backend is implemented.
 *
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 */

#include <pacs/storage/base_repository.hpp>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <pacs/storage/pacs_database_adapter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <string>

using namespace pacs::storage;

namespace {

// =============================================================================
// Test Entity and Repository
// =============================================================================

/**
 * @brief Simple test entity for repository tests
 */
struct test_entity {
    int64_t id{0};
    std::string name;
    int value{0};

    auto operator==(const test_entity& other) const -> bool {
        return id == other.id && name == other.name && value == other.value;
    }
};

/**
 * @brief Test repository implementation
 */
class test_repository : public base_repository<test_entity> {
public:
    explicit test_repository(std::shared_ptr<pacs_database_adapter> db)
        : base_repository<test_entity>(db, "test_entities", "id") {}

protected:
    test_entity map_row_to_entity(const database_row& row) const override {
        return test_entity{std::stoll(row.at("id")), row.at("name"),
                           std::stoi(row.at("value"))};
    }

    std::map<std::string, database_value> entity_to_row(const test_entity& entity) const override {
        return {{"name", entity.name},
                {"value", static_cast<int64_t>(entity.value)}};
    }

    int64_t get_pk(const test_entity& entity) const override {
        return entity.id;
    }

    bool has_pk(const test_entity& entity) const override {
        return entity.id > 0;
    }
};

// =============================================================================
// Test Helpers
// =============================================================================

/**
 * @brief Create test database path
 */
auto get_test_db_path() -> std::filesystem::path {
    return std::filesystem::temp_directory_path() /
           "base_repository_test.db";
}

/**
 * @brief Clean up test database
 */
void cleanup_test_db() {
    auto path = get_test_db_path();
    std::filesystem::remove(path);
    std::filesystem::remove(std::filesystem::path(path.string() + "-wal"));
    std::filesystem::remove(std::filesystem::path(path.string() + "-shm"));
}

/**
 * @brief RAII helper for test database cleanup
 */
struct test_db_guard {
    test_db_guard() { cleanup_test_db(); }
    ~test_db_guard() { cleanup_test_db(); }
};

/**
 * @brief Check if SQLite backend is supported
 */
bool is_sqlite_backend_supported() {
    pacs_database_adapter db(":memory:");
    auto result = db.connect();
    return result.is_ok();
}

/**
 * @brief Skip message for unsupported SQLite backend
 */
constexpr const char* SQLITE_NOT_SUPPORTED_MSG =
    "SQLite backend not yet supported by unified_database_system. "
    "See database_system Issue for backend implementation.";

/**
 * @brief Create test table in database
 */
auto create_test_table(pacs_database_adapter& db) -> VoidResult {
    auto create_sql = R"(
        CREATE TABLE IF NOT EXISTS test_entities (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            value INTEGER DEFAULT 0
        )
    )";
    return db.execute(create_sql);
}

}  // namespace

// =============================================================================
// Interface Tests (no connection required)
// =============================================================================

TEST_CASE("base_repository: construction", "[storage][repository][interface]") {
    auto db = std::make_shared<pacs_database_adapter>(":memory:");
    test_repository repo(db);

    // Constructor should not throw
    REQUIRE(true);
}

// =============================================================================
// Integration Tests (require SQLite backend)
// =============================================================================

TEST_CASE("base_repository: basic CRUD operations",
          "[storage][repository][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    test_db_guard guard;
    auto db = std::make_shared<pacs_database_adapter>(get_test_db_path());
    REQUIRE(db->connect().is_ok());
    REQUIRE(create_test_table(*db).is_ok());

    test_repository repo(db);

    SECTION("insert and find_by_id") {
        test_entity entity{0, "test_name", 42};

        auto insert_result = repo.insert(entity);
        REQUIRE(insert_result.is_ok());

        auto id = insert_result.value();
        REQUIRE(id > 0);

        auto find_result = repo.find_by_id(id);
        REQUIRE(find_result.is_ok());

        auto found = find_result.value();
        REQUIRE(found.id == id);
        REQUIRE(found.name == "test_name");
        REQUIRE(found.value == 42);
    }

    SECTION("update existing entity") {
        test_entity entity{0, "original", 10};
        auto insert_result = repo.insert(entity);
        REQUIRE(insert_result.is_ok());

        auto id = insert_result.value();
        test_entity updated{id, "updated", 20};

        auto update_result = repo.update(updated);
        REQUIRE(update_result.is_ok());

        auto find_result = repo.find_by_id(id);
        REQUIRE(find_result.is_ok());

        auto found = find_result.value();
        REQUIRE(found.name == "updated");
        REQUIRE(found.value == 20);
    }

    SECTION("save with new entity (insert)") {
        test_entity entity{0, "new_entity", 100};

        auto save_result = repo.save(entity);
        REQUIRE(save_result.is_ok());

        auto id = save_result.value();
        REQUIRE(id > 0);

        auto find_result = repo.find_by_id(id);
        REQUIRE(find_result.is_ok());
    }

    SECTION("save with existing entity (update)") {
        test_entity entity{0, "entity", 50};
        auto insert_result = repo.insert(entity);
        REQUIRE(insert_result.is_ok());

        auto id = insert_result.value();
        test_entity existing{id, "modified", 60};

        auto save_result = repo.save(existing);
        REQUIRE(save_result.is_ok());
        REQUIRE(save_result.value() == id);

        auto find_result = repo.find_by_id(id);
        REQUIRE(find_result.is_ok());
        REQUIRE(find_result.value().name == "modified");
    }

    SECTION("remove entity") {
        test_entity entity{0, "to_delete", 99};
        auto insert_result = repo.insert(entity);
        REQUIRE(insert_result.is_ok());

        auto id = insert_result.value();

        auto remove_result = repo.remove(id);
        REQUIRE(remove_result.is_ok());

        auto find_result = repo.find_by_id(id);
        REQUIRE(find_result.is_err());
    }
}

TEST_CASE("base_repository: query operations",
          "[storage][repository][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    test_db_guard guard;
    auto db = std::make_shared<pacs_database_adapter>(get_test_db_path());
    REQUIRE(db->connect().is_ok());
    REQUIRE(create_test_table(*db).is_ok());

    test_repository repo(db);

    SECTION("find_all returns all entities") {
        (void)repo.insert(test_entity{0, "entity1", 1});
        (void)repo.insert(test_entity{0, "entity2", 2});
        (void)repo.insert(test_entity{0, "entity3", 3});

        auto result = repo.find_all();
        REQUIRE(result.is_ok());
        REQUIRE(result.value().size() == 3);
    }

    SECTION("find_all with limit") {
        (void)repo.insert(test_entity{0, "entity1", 1});
        (void)repo.insert(test_entity{0, "entity2", 2});
        (void)repo.insert(test_entity{0, "entity3", 3});

        auto result = repo.find_all(2);
        REQUIRE(result.is_ok());
        REQUIRE(result.value().size() == 2);
    }

    SECTION("find_where") {
        (void)repo.insert(test_entity{0, "alice", 100});
        (void)repo.insert(test_entity{0, "bob", 200});
        (void)repo.insert(test_entity{0, "alice", 300});

        auto result = repo.find_where("name", "=", "alice");
        REQUIRE(result.is_ok());
        REQUIRE(result.value().size() == 2);

        for (const auto& entity : result.value()) {
            REQUIRE(entity.name == "alice");
        }
    }

    SECTION("exists") {
        auto insert_result = repo.insert(test_entity{0, "exists_test", 42});
        REQUIRE(insert_result.is_ok());

        auto id = insert_result.value();

        auto exists_result = repo.exists(id);
        REQUIRE(exists_result.is_ok());
        REQUIRE(exists_result.value() == true);

        auto not_exists_result = repo.exists(99999);
        REQUIRE(not_exists_result.is_ok());
        REQUIRE(not_exists_result.value() == false);
    }

    SECTION("count") {
        (void)repo.insert(test_entity{0, "entity1", 1});
        (void)repo.insert(test_entity{0, "entity2", 2});

        auto result = repo.count();
        REQUIRE(result.is_ok());
        REQUIRE(result.value() == 2);
    }
}

TEST_CASE("base_repository: batch operations",
          "[storage][repository][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    test_db_guard guard;
    auto db = std::make_shared<pacs_database_adapter>(get_test_db_path());
    REQUIRE(db->connect().is_ok());
    REQUIRE(create_test_table(*db).is_ok());

    test_repository repo(db);

    SECTION("insert_batch") {
        std::vector<test_entity> entities = {
            {0, "batch1", 1}, {0, "batch2", 2}, {0, "batch3", 3}};

        auto result = repo.insert_batch(entities);
        REQUIRE(result.is_ok());
        REQUIRE(result.value().size() == 3);

        auto count_result = repo.count();
        REQUIRE(count_result.is_ok());
        REQUIRE(count_result.value() == 3);
    }

    SECTION("in_transaction with success") {
        auto result = repo.in_transaction([&]() -> VoidResult {
            auto r1 = repo.insert(test_entity{0, "tx1", 1});
            if (r1.is_err()) {
                return VoidResult(r1.error());
            }

            auto r2 = repo.insert(test_entity{0, "tx2", 2});
            if (r2.is_err()) {
                return VoidResult(r2.error());
            }

            return kcenon::common::ok();
        });

        REQUIRE(result.is_ok());

        auto count_result = repo.count();
        REQUIRE(count_result.is_ok());
        REQUIRE(count_result.value() == 2);
    }
}

TEST_CASE("base_repository: error handling",
          "[storage][repository][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    test_db_guard guard;
    auto db = std::make_shared<pacs_database_adapter>(get_test_db_path());
    REQUIRE(db->connect().is_ok());
    REQUIRE(create_test_table(*db).is_ok());

    test_repository repo(db);

    SECTION("find_by_id with non-existent id") {
        auto result = repo.find_by_id(99999);
        REQUIRE(result.is_err());
    }

    SECTION("update entity without valid primary key") {
        test_entity entity{0, "no_pk", 42};
        auto result = repo.update(entity);
        REQUIRE(result.is_err());
    }

    SECTION("remove non-existent entity") {
        auto result = repo.remove(99999);
        REQUIRE(result.is_err());
    }
}

#endif  // PACS_WITH_DATABASE_SYSTEM
