/**
 * @file pacs_database_adapter_test.cpp
 * @brief Unit tests for pacs_database_adapter
 *
 * Tests the unified database adapter including connection management,
 * CRUD operations, and transaction support.
 *
 * @note These tests require unified_database_system to support SQLite backend.
 *       Currently, unified_database_system only supports PostgreSQL.
 *       Integration tests will SKIP until SQLite backend is implemented.
 *       See: database_system/database/integrated/unified_database_system.cpp:85
 *
 * @see Issue #606 - Phase 1: Foundation - PACS Database Adapter
 */

#include <pacs/storage/pacs_database_adapter.hpp>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>

using namespace pacs::storage;

namespace {

/**
 * @brief Create test database path
 */
auto get_test_db_path() -> std::filesystem::path {
    return std::filesystem::temp_directory_path() / "pacs_adapter_test.db";
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
 * @brief Check if SQLite backend is supported by unified_database_system
 *
 * unified_database_system currently only supports PostgreSQL.
 * This helper attempts a connection and returns true if successful.
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

}  // namespace

// ============================================================================
// Interface Tests (no connection required)
// ============================================================================

TEST_CASE("pacs_database_adapter: construction and initial state",
          "[storage][adapter][interface]") {
    SECTION("construct with path") {
        pacs_database_adapter db(get_test_db_path());
        CHECK_FALSE(db.is_connected());
        CHECK_FALSE(db.in_transaction());
        CHECK(db.last_error().empty());
        CHECK(db.last_insert_rowid() == 0);
    }

    SECTION("construct with database type") {
        pacs_database_adapter db(database::database_types::sqlite,
                                 get_test_db_path().string());
        CHECK_FALSE(db.is_connected());
    }
}

TEST_CASE("pacs_database_adapter: operations fail when not connected",
          "[storage][adapter][interface]") {
    pacs_database_adapter db(":memory:");

    auto select_result = db.select("SELECT 1");
    CHECK(select_result.is_err());

    auto insert_result = db.insert("INSERT INTO test VALUES (1)");
    CHECK(insert_result.is_err());

    auto update_result = db.update("UPDATE test SET x = 1");
    CHECK(update_result.is_err());

    auto remove_result = db.remove("DELETE FROM test");
    CHECK(remove_result.is_err());

    auto exec_result = db.execute("CREATE TABLE test (x INT)");
    CHECK(exec_result.is_err());
}

TEST_CASE("pacs_database_adapter: transaction state when not connected",
          "[storage][adapter][interface]") {
    pacs_database_adapter db(":memory:");

    auto begin_result = db.begin_transaction();
    CHECK(begin_result.is_err());
    CHECK_FALSE(db.in_transaction());

    auto commit_result = db.commit();
    CHECK(commit_result.is_err());

    auto rollback_result = db.rollback();
    // Rollback when not in transaction is OK (no-op)
    CHECK(rollback_result.is_ok());
}

// ============================================================================
// Integration Tests (require SQLite backend support)
// ============================================================================

TEST_CASE("pacs_database_adapter: connect with SQLite path",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    test_db_guard guard;
    pacs_database_adapter db(get_test_db_path());

    CHECK_FALSE(db.is_connected());

    auto result = db.connect();
    REQUIRE(result.is_ok());
    CHECK(db.is_connected());
}

TEST_CASE("pacs_database_adapter: connect with database type",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    test_db_guard guard;
    pacs_database_adapter db(database::database_types::sqlite,
                             get_test_db_path().string());

    auto result = db.connect();
    REQUIRE(result.is_ok());
    CHECK(db.is_connected());
}

TEST_CASE("pacs_database_adapter: disconnect",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    test_db_guard guard;
    pacs_database_adapter db(get_test_db_path());
    REQUIRE(db.connect().is_ok());
    CHECK(db.is_connected());

    auto result = db.disconnect();
    REQUIRE(result.is_ok());
    CHECK_FALSE(db.is_connected());
}

TEST_CASE("pacs_database_adapter: execute DDL",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    pacs_database_adapter db(":memory:");
    REQUIRE(db.connect().is_ok());

    auto result = db.execute(
        "CREATE TABLE test_table ("
        "  id INTEGER PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  value REAL"
        ")");
    REQUIRE(result.is_ok());

    // Verify table exists
    auto check = db.select(
        "SELECT name FROM sqlite_master WHERE type='table' "
        "AND name='test_table'");
    REQUIRE(check.is_ok());
    CHECK_FALSE(check.value().empty());
    CHECK(check.value()[0].at("name") == "test_table");
}

TEST_CASE("pacs_database_adapter: INSERT operation",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    pacs_database_adapter db(":memory:");
    REQUIRE(db.connect().is_ok());

    REQUIRE(
        db.execute(
              "CREATE TABLE patients (id INTEGER PRIMARY KEY, name TEXT)")
            .is_ok());

    auto result = db.insert("INSERT INTO patients (name) VALUES ('John Doe')");
    REQUIRE(result.is_ok());
    CHECK(result.value() == 1);

    // Verify rowid
    CHECK(db.last_insert_rowid() == 1);

    // Insert another
    result = db.insert("INSERT INTO patients (name) VALUES ('Jane Doe')");
    REQUIRE(result.is_ok());
    CHECK(db.last_insert_rowid() == 2);
}

TEST_CASE("pacs_database_adapter: SELECT operation",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    pacs_database_adapter db(":memory:");
    REQUIRE(db.connect().is_ok());

    REQUIRE(
        db.execute(
              "CREATE TABLE patients (id INTEGER PRIMARY KEY, name TEXT)")
            .is_ok());
    REQUIRE(
        db.insert("INSERT INTO patients (name) VALUES ('John Doe')").is_ok());
    REQUIRE(
        db.insert("INSERT INTO patients (name) VALUES ('Jane Doe')").is_ok());

    auto result = db.select("SELECT * FROM patients ORDER BY id");
    REQUIRE(result.is_ok());

    const auto& rows = result.value();
    CHECK(rows.size() == 2);
    CHECK(rows[0].at("name") == "John Doe");
    CHECK(rows[1].at("name") == "Jane Doe");
}

TEST_CASE("pacs_database_adapter: UPDATE operation",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    pacs_database_adapter db(":memory:");
    REQUIRE(db.connect().is_ok());

    REQUIRE(
        db.execute(
              "CREATE TABLE patients (id INTEGER PRIMARY KEY, name TEXT)")
            .is_ok());
    REQUIRE(
        db.insert("INSERT INTO patients (name) VALUES ('John Doe')").is_ok());

    auto result = db.update("UPDATE patients SET name = 'Updated' WHERE id = 1");
    REQUIRE(result.is_ok());
    CHECK(result.value() == 1);

    // Verify update
    auto check = db.select("SELECT name FROM patients WHERE id = 1");
    REQUIRE(check.is_ok());
    CHECK(check.value()[0].at("name") == "Updated");
}

TEST_CASE("pacs_database_adapter: DELETE operation",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    pacs_database_adapter db(":memory:");
    REQUIRE(db.connect().is_ok());

    REQUIRE(
        db.execute(
              "CREATE TABLE patients (id INTEGER PRIMARY KEY, name TEXT)")
            .is_ok());
    REQUIRE(
        db.insert("INSERT INTO patients (name) VALUES ('John Doe')").is_ok());
    REQUIRE(
        db.insert("INSERT INTO patients (name) VALUES ('Jane Doe')").is_ok());

    auto result = db.remove("DELETE FROM patients WHERE id = 1");
    REQUIRE(result.is_ok());
    CHECK(result.value() == 1);

    // Verify deletion
    auto check = db.select("SELECT COUNT(*) as cnt FROM patients");
    REQUIRE(check.is_ok());
    CHECK(check.value()[0].at("cnt") == "1");
}

// ============================================================================
// Transaction Tests
// ============================================================================

TEST_CASE("pacs_database_adapter: transaction commit",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    pacs_database_adapter db(":memory:");
    REQUIRE(db.connect().is_ok());

    REQUIRE(
        db.execute(
              "CREATE TABLE patients (id INTEGER PRIMARY KEY, name TEXT)")
            .is_ok());

    REQUIRE(db.begin_transaction().is_ok());
    CHECK(db.in_transaction());

    REQUIRE(
        db.insert("INSERT INTO patients (name) VALUES ('Transaction Test')")
            .is_ok());

    REQUIRE(db.commit().is_ok());
    CHECK_FALSE(db.in_transaction());

    // Verify data persisted
    auto check = db.select("SELECT name FROM patients WHERE id = 1");
    REQUIRE(check.is_ok());
    CHECK(check.value()[0].at("name") == "Transaction Test");
}

TEST_CASE("pacs_database_adapter: transaction rollback",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    pacs_database_adapter db(":memory:");
    REQUIRE(db.connect().is_ok());

    REQUIRE(
        db.execute(
              "CREATE TABLE patients (id INTEGER PRIMARY KEY, name TEXT)")
            .is_ok());

    REQUIRE(db.begin_transaction().is_ok());
    REQUIRE(
        db.insert("INSERT INTO patients (name) VALUES ('Will Be Rolled Back')")
            .is_ok());

    REQUIRE(db.rollback().is_ok());
    CHECK_FALSE(db.in_transaction());

    // Verify data not persisted
    auto check = db.select("SELECT COUNT(*) as cnt FROM patients");
    REQUIRE(check.is_ok());
    CHECK(check.value()[0].at("cnt") == "0");
}

TEST_CASE("pacs_database_adapter: nested transaction rejected",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    pacs_database_adapter db(":memory:");
    REQUIRE(db.connect().is_ok());

    REQUIRE(db.begin_transaction().is_ok());

    auto result = db.begin_transaction();
    CHECK(result.is_err());

    (void)db.rollback();
}

TEST_CASE("pacs_database_adapter: transaction template function",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    pacs_database_adapter db(":memory:");
    REQUIRE(db.connect().is_ok());

    REQUIRE(
        db.execute(
              "CREATE TABLE patients (id INTEGER PRIMARY KEY, name TEXT)")
            .is_ok());

    auto result = db.transaction([&]() -> VoidResult {
        auto r1 = db.insert("INSERT INTO patients (name) VALUES ('One')");
        if (r1.is_err()) {
            return VoidResult(kcenon::common::error_info{
                r1.error().code, r1.error().message, "storage"});
        }

        auto r2 = db.insert("INSERT INTO patients (name) VALUES ('Two')");
        if (r2.is_err()) {
            return VoidResult(kcenon::common::error_info{
                r2.error().code, r2.error().message, "storage"});
        }

        return kcenon::common::ok();
    });

    REQUIRE(result.is_ok());

    // Verify both rows inserted
    auto check = db.select("SELECT COUNT(*) as cnt FROM patients");
    REQUIRE(check.is_ok());
    CHECK(check.value()[0].at("cnt") == "2");
}

// ============================================================================
// scoped_transaction Tests
// ============================================================================

TEST_CASE("scoped_transaction: auto rollback on destruction",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    pacs_database_adapter db(":memory:");
    REQUIRE(db.connect().is_ok());

    REQUIRE(
        db.execute(
              "CREATE TABLE patients (id INTEGER PRIMARY KEY, name TEXT)")
            .is_ok());

    {
        scoped_transaction tx(db);
        CHECK(tx.is_active());

        REQUIRE(
            db.insert("INSERT INTO patients (name) VALUES ('Will Rollback')")
                .is_ok());
        // tx goes out of scope without commit
    }

    // Verify rollback occurred
    auto check = db.select("SELECT COUNT(*) as cnt FROM patients");
    REQUIRE(check.is_ok());
    CHECK(check.value()[0].at("cnt") == "0");
}

TEST_CASE("scoped_transaction: explicit commit",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    pacs_database_adapter db(":memory:");
    REQUIRE(db.connect().is_ok());

    REQUIRE(
        db.execute(
              "CREATE TABLE patients (id INTEGER PRIMARY KEY, name TEXT)")
            .is_ok());

    {
        scoped_transaction tx(db);
        REQUIRE(
            db.insert("INSERT INTO patients (name) VALUES ('Will Commit')")
                .is_ok());

        auto result = tx.commit();
        REQUIRE(result.is_ok());
        CHECK_FALSE(tx.is_active());
    }

    // Verify commit occurred
    auto check = db.select("SELECT name FROM patients WHERE id = 1");
    REQUIRE(check.is_ok());
    CHECK(check.value()[0].at("name") == "Will Commit");
}

TEST_CASE("scoped_transaction: explicit rollback",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    pacs_database_adapter db(":memory:");
    REQUIRE(db.connect().is_ok());

    REQUIRE(
        db.execute(
              "CREATE TABLE patients (id INTEGER PRIMARY KEY, name TEXT)")
            .is_ok());

    {
        scoped_transaction tx(db);
        REQUIRE(
            db.insert("INSERT INTO patients (name) VALUES ('Will Rollback')")
                .is_ok());

        tx.rollback();
        CHECK_FALSE(tx.is_active());
    }

    // Verify rollback occurred
    auto check = db.select("SELECT COUNT(*) as cnt FROM patients");
    REQUIRE(check.is_ok());
    CHECK(check.value()[0].at("cnt") == "0");
}

// ============================================================================
// Query Builder Tests
// ============================================================================

TEST_CASE("pacs_database_adapter: query builder integration",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    pacs_database_adapter db(":memory:");
    REQUIRE(db.connect().is_ok());

    REQUIRE(db.execute(
                  "CREATE TABLE patients ("
                  "  id INTEGER PRIMARY KEY,"
                  "  name TEXT,"
                  "  age INTEGER"
                  ")")
                .is_ok());

    REQUIRE(
        db.insert("INSERT INTO patients (name, age) VALUES ('John', 30)")
            .is_ok());
    REQUIRE(
        db.insert("INSERT INTO patients (name, age) VALUES ('Jane', 25)")
            .is_ok());
    REQUIRE(
        db.insert("INSERT INTO patients (name, age) VALUES ('Bob', 35)")
            .is_ok());

    auto builder = db.create_query_builder();
    builder.select({"name", "age"}).from("patients").limit(2);

    auto query = builder.build();
    auto result = db.select(query);

    REQUIRE(result.is_ok());
    CHECK(result.value().size() == 2);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_CASE("pacs_database_adapter: last_error reports failure",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    pacs_database_adapter db(":memory:");
    REQUIRE(db.connect().is_ok());

    // Try to select from non-existent table
    auto result = db.select("SELECT * FROM nonexistent_table");
    CHECK(result.is_err());
    CHECK_FALSE(db.last_error().empty());
}

TEST_CASE("pacs_database_adapter: database_result iteration",
          "[storage][adapter][integration]") {
    if (!is_sqlite_backend_supported()) {
        SKIP(SQLITE_NOT_SUPPORTED_MSG);
    }

    pacs_database_adapter db(":memory:");
    REQUIRE(db.connect().is_ok());

    REQUIRE(db.execute("CREATE TABLE items (id INTEGER, name TEXT)").is_ok());
    REQUIRE(db.insert("INSERT INTO items VALUES (1, 'A')").is_ok());
    REQUIRE(db.insert("INSERT INTO items VALUES (2, 'B')").is_ok());
    REQUIRE(db.insert("INSERT INTO items VALUES (3, 'C')").is_ok());

    auto result = db.select("SELECT * FROM items ORDER BY id");
    REQUIRE(result.is_ok());

    auto& data = result.value();
    CHECK(data.size() == 3);
    CHECK_FALSE(data.empty());

    // Test iteration
    int count = 0;
    for (const auto& row : data) {
        ++count;
        CHECK(row.find("id") != row.end());
        CHECK(row.find("name") != row.end());
    }
    CHECK(count == 3);

    // Test index access
    CHECK(data[0].at("name") == "A");
    CHECK(data[1].at("name") == "B");
    CHECK(data[2].at("name") == "C");
}

#endif  // PACS_WITH_DATABASE_SYSTEM
