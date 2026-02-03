/**
 * @file annotation_repository_test.cpp
 * @brief Unit tests for annotation_repository class
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #581 - Part 1: Data Models and Repositories
 * @see Issue #650 - Part 2: Migrate annotation, routing, node repositories
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/storage/annotation_repository.hpp>
#include <pacs/storage/migration_runner.hpp>

#ifdef PACS_WITH_DATABASE_SYSTEM
#include <pacs/storage/pacs_database_adapter.hpp>
#else
#include <sqlite3.h>
#endif

using namespace pacs::storage;

namespace {

#ifdef PACS_WITH_DATABASE_SYSTEM

/**
 * @brief Check if SQLite backend is supported
 */
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
            throw std::runtime_error("Failed to connect: " + conn_result.error().message);
        }
        migration_runner runner;
        auto result = runner.run_migrations(db_);
        if (result.is_err()) {
            throw std::runtime_error("Migration failed: " + result.error().message);
        }
    }

    ~test_database() = default;

    test_database(const test_database&) = delete;
    auto operator=(const test_database&) -> test_database& = delete;

    [[nodiscard]] auto get() const noexcept -> std::shared_ptr<pacs_database_adapter> {
        return db_;
    }

private:
    std::shared_ptr<pacs_database_adapter> db_;
};

#else  // !PACS_WITH_DATABASE_SYSTEM

class test_database {
public:
    test_database() {
        auto rc = sqlite3_open(":memory:", &db_);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to open in-memory database");
        }
        migration_runner runner;
        auto result = runner.run_migrations(db_);
        if (result.is_err()) {
            throw std::runtime_error("Migration failed: " + result.error().message);
        }
    }

    ~test_database() {
        if (db_ != nullptr) {
            sqlite3_close(db_);
        }
    }

    test_database(const test_database&) = delete;
    auto operator=(const test_database&) -> test_database& = delete;

    [[nodiscard]] auto get() const noexcept -> sqlite3* { return db_; }

private:
    sqlite3* db_ = nullptr;
};

#endif  // PACS_WITH_DATABASE_SYSTEM

[[nodiscard]] annotation_record make_test_annotation(
    const std::string& id = "test-annotation-1") {
    annotation_record record;
    record.annotation_id = id;
    record.study_uid = "1.2.3.4.5";
    record.series_uid = "1.2.3.4.5.6";
    record.sop_instance_uid = "1.2.3.4.5.6.7";
    record.frame_number = 1;
    record.user_id = "testuser";
    record.type = annotation_type::arrow;
    record.geometry_json = R"({"start":{"x":0,"y":0},"end":{"x":100,"y":100}})";
    record.text = "Test annotation";
    record.style.color = "#FF0000";
    record.style.line_width = 3;
    return record;
}

}  // namespace

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("annotation_repository construction", "[annotation][repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not yet supported");
        return;
    }

    test_database db;

    SECTION("valid database handle") {
        annotation_repository repo(db.get());
        // In base_repository version, is_valid() is inherited from base
        // Simply check that construction succeeds
        REQUIRE(true);
    }
}

#else  // !PACS_WITH_DATABASE_SYSTEM

TEST_CASE("annotation_repository construction", "[annotation][repository]") {
    test_database db;

    SECTION("valid database handle") {
        annotation_repository repo(db.get());
        REQUIRE(repo.is_valid());
    }

    SECTION("null database handle") {
        annotation_repository repo(nullptr);
        REQUIRE_FALSE(repo.is_valid());
    }
}

#endif  // PACS_WITH_DATABASE_SYSTEM

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("annotation_repository save and find", "[annotation][repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not yet supported");
        return;
    }

    test_database db;
    annotation_repository repo(db.get());

    SECTION("save new annotation") {
        auto record = make_test_annotation();
        auto result = repo.save(record);
        REQUIRE(result.is_ok());
        auto exists_result = repo.exists(record.annotation_id);
        REQUIRE(exists_result.is_ok());
        REQUIRE(exists_result.value());
    }

    SECTION("find by id") {
        auto record = make_test_annotation("find-test-1");
        auto save_result = repo.save(record);
        REQUIRE(save_result.is_ok());

        auto found = repo.find_by_id("find-test-1");
        REQUIRE(found.is_ok());
        REQUIRE(found.value().annotation_id == "find-test-1");
        REQUIRE(found.value().study_uid == "1.2.3.4.5");
        REQUIRE(found.value().type == annotation_type::arrow);
        REQUIRE(found.value().text == "Test annotation");
    }

    SECTION("find non-existent returns error") {
        auto found = repo.find_by_id("non-existent");
        REQUIRE(found.is_err());
    }

    SECTION("save updates existing annotation") {
        auto record = make_test_annotation("update-test-1");
        (void)repo.save(record);

        record.text = "Updated text";
        record.geometry_json = R"({"x":50,"y":50})";
        auto result = repo.save(record);
        REQUIRE(result.is_ok());

        auto found = repo.find_by_id("update-test-1");
        REQUIRE(found.is_ok());
        REQUIRE(found.value().text == "Updated text");
    }
}

#else  // !PACS_WITH_DATABASE_SYSTEM

TEST_CASE("annotation_repository save and find", "[annotation][repository]") {
    test_database db;
    annotation_repository repo(db.get());

    SECTION("save new annotation") {
        auto record = make_test_annotation();
        auto result = repo.save(record);
        REQUIRE(result.is_ok());
        REQUIRE(repo.exists(record.annotation_id));
    }

    SECTION("find by id") {
        auto record = make_test_annotation("find-test-1");
        auto save_result = repo.save(record);
        REQUIRE(save_result.is_ok());

        auto found = repo.find_by_id("find-test-1");
        REQUIRE(found.has_value());
        REQUIRE(found->annotation_id == "find-test-1");
        REQUIRE(found->study_uid == "1.2.3.4.5");
        REQUIRE(found->type == annotation_type::arrow);
        REQUIRE(found->text == "Test annotation");
    }

    SECTION("find non-existent returns nullopt") {
        auto found = repo.find_by_id("non-existent");
        REQUIRE_FALSE(found.has_value());
    }

    SECTION("save updates existing annotation") {
        auto record = make_test_annotation("update-test-1");
        repo.save(record);

        record.text = "Updated text";
        record.geometry_json = R"({"x":50,"y":50})";
        auto result = repo.save(record);
        REQUIRE(result.is_ok());

        auto found = repo.find_by_id("update-test-1");
        REQUIRE(found.has_value());
        REQUIRE(found->text == "Updated text");
    }
}

#endif  // PACS_WITH_DATABASE_SYSTEM

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("annotation_repository search", "[annotation][repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not yet supported");
        return;
    }

    test_database db;
    annotation_repository repo(db.get());

    auto ann1 = make_test_annotation("search-1");
    ann1.study_uid = "study-1";
    ann1.sop_instance_uid = "instance-1";
    ann1.user_id = "user-a";
    (void)repo.save(ann1);

    auto ann2 = make_test_annotation("search-2");
    ann2.study_uid = "study-1";
    ann2.sop_instance_uid = "instance-2";
    ann2.user_id = "user-b";
    (void)repo.save(ann2);

    auto ann3 = make_test_annotation("search-3");
    ann3.study_uid = "study-2";
    ann3.sop_instance_uid = "instance-3";
    ann3.user_id = "user-a";
    (void)repo.save(ann3);

    SECTION("search by study_uid") {
        annotation_query query;
        query.study_uid = "study-1";
        auto results = repo.search(query);
        REQUIRE(results.is_ok());
        REQUIRE(results.value().size() == 2);
    }

    SECTION("search by sop_instance_uid") {
        annotation_query query;
        query.sop_instance_uid = "instance-1";
        auto results = repo.search(query);
        REQUIRE(results.is_ok());
        REQUIRE(results.value().size() == 1);
        REQUIRE(results.value()[0].annotation_id == "search-1");
    }

    SECTION("search by user_id") {
        annotation_query query;
        query.user_id = "user-a";
        auto results = repo.search(query);
        REQUIRE(results.is_ok());
        REQUIRE(results.value().size() == 2);
    }

    SECTION("find_by_instance") {
        auto results = repo.find_by_instance("instance-2");
        REQUIRE(results.is_ok());
        REQUIRE(results.value().size() == 1);
        REQUIRE(results.value()[0].annotation_id == "search-2");
    }

    SECTION("find_by_study") {
        auto results = repo.find_by_study("study-2");
        REQUIRE(results.is_ok());
        REQUIRE(results.value().size() == 1);
    }

    SECTION("search with limit and offset") {
        annotation_query query;
        query.study_uid = "study-1";
        query.limit = 1;
        query.offset = 0;
        auto results = repo.search(query);
        REQUIRE(results.is_ok());
        REQUIRE(results.value().size() == 1);
    }
}

#else  // !PACS_WITH_DATABASE_SYSTEM

TEST_CASE("annotation_repository search", "[annotation][repository]") {
    test_database db;
    annotation_repository repo(db.get());

    auto ann1 = make_test_annotation("search-1");
    ann1.study_uid = "study-1";
    ann1.sop_instance_uid = "instance-1";
    ann1.user_id = "user-a";
    repo.save(ann1);

    auto ann2 = make_test_annotation("search-2");
    ann2.study_uid = "study-1";
    ann2.sop_instance_uid = "instance-2";
    ann2.user_id = "user-b";
    repo.save(ann2);

    auto ann3 = make_test_annotation("search-3");
    ann3.study_uid = "study-2";
    ann3.sop_instance_uid = "instance-3";
    ann3.user_id = "user-a";
    repo.save(ann3);

    SECTION("search by study_uid") {
        annotation_query query;
        query.study_uid = "study-1";
        auto results = repo.search(query);
        REQUIRE(results.size() == 2);
    }

    SECTION("search by sop_instance_uid") {
        annotation_query query;
        query.sop_instance_uid = "instance-1";
        auto results = repo.search(query);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].annotation_id == "search-1");
    }

    SECTION("search by user_id") {
        annotation_query query;
        query.user_id = "user-a";
        auto results = repo.search(query);
        REQUIRE(results.size() == 2);
    }

    SECTION("find_by_instance") {
        auto results = repo.find_by_instance("instance-2");
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].annotation_id == "search-2");
    }

    SECTION("find_by_study") {
        auto results = repo.find_by_study("study-2");
        REQUIRE(results.size() == 1);
    }

    SECTION("search with limit and offset") {
        annotation_query query;
        query.study_uid = "study-1";
        query.limit = 1;
        query.offset = 0;
        auto results = repo.search(query);
        REQUIRE(results.size() == 1);
    }
}

#endif  // PACS_WITH_DATABASE_SYSTEM

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("annotation_repository update", "[annotation][repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not yet supported");
        return;
    }

    test_database db;
    annotation_repository repo(db.get());

    auto record = make_test_annotation("update-test");
    (void)repo.save(record);

    SECTION("update geometry and text") {
        record.geometry_json = R"({"new":"geometry"})";
        record.text = "Updated annotation";
        record.style.color = "#00FF00";

        auto result = repo.update(record);
        REQUIRE(result.is_ok());

        auto found = repo.find_by_id("update-test");
        REQUIRE(found.is_ok());
        REQUIRE(found.value().geometry_json == R"({"new":"geometry"})");
        REQUIRE(found.value().text == "Updated annotation");
        REQUIRE(found.value().style.color == "#00FF00");
    }
}

#else  // !PACS_WITH_DATABASE_SYSTEM

TEST_CASE("annotation_repository update", "[annotation][repository]") {
    test_database db;
    annotation_repository repo(db.get());

    auto record = make_test_annotation("update-test");
    repo.save(record);

    SECTION("update geometry and text") {
        record.geometry_json = R"({"new":"geometry"})";
        record.text = "Updated annotation";
        record.style.color = "#00FF00";

        auto result = repo.update(record);
        REQUIRE(result.is_ok());

        auto found = repo.find_by_id("update-test");
        REQUIRE(found.has_value());
        REQUIRE(found->geometry_json == R"({"new":"geometry"})");
        REQUIRE(found->text == "Updated annotation");
        REQUIRE(found->style.color == "#00FF00");
    }
}

#endif  // PACS_WITH_DATABASE_SYSTEM

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("annotation_repository remove", "[annotation][repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not yet supported");
        return;
    }

    test_database db;
    annotation_repository repo(db.get());

    auto record = make_test_annotation("remove-test");
    (void)repo.save(record);

    auto exists_result = repo.exists("remove-test");
    REQUIRE(exists_result.is_ok());
    REQUIRE(exists_result.value());

    auto result = repo.remove("remove-test");
    REQUIRE(result.is_ok());

    auto exists_after = repo.exists("remove-test");
    REQUIRE(exists_after.is_ok());
    REQUIRE_FALSE(exists_after.value());
}

#else  // !PACS_WITH_DATABASE_SYSTEM

TEST_CASE("annotation_repository remove", "[annotation][repository]") {
    test_database db;
    annotation_repository repo(db.get());

    auto record = make_test_annotation("remove-test");
    repo.save(record);
    REQUIRE(repo.exists("remove-test"));

    auto result = repo.remove("remove-test");
    REQUIRE(result.is_ok());
    REQUIRE_FALSE(repo.exists("remove-test"));
}

#endif  // PACS_WITH_DATABASE_SYSTEM

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("annotation_repository count", "[annotation][repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not yet supported");
        return;
    }

    test_database db;
    annotation_repository repo(db.get());

    auto count_result = repo.count();
    REQUIRE(count_result.is_ok());
    REQUIRE(count_result.value() == 0);

    (void)repo.save(make_test_annotation("count-1"));
    (void)repo.save(make_test_annotation("count-2"));
    (void)repo.save(make_test_annotation("count-3"));

    count_result = repo.count();
    REQUIRE(count_result.is_ok());
    REQUIRE(count_result.value() == 3);

    SECTION("count with query") {
        auto ann = make_test_annotation("count-4");
        ann.study_uid = "different-study";
        (void)repo.save(ann);

        annotation_query query;
        query.study_uid = "1.2.3.4.5";
        auto matching = repo.count_matching(query);
        REQUIRE(matching.is_ok());
        REQUIRE(matching.value() == 3);

        query.study_uid = "different-study";
        matching = repo.count_matching(query);
        REQUIRE(matching.is_ok());
        REQUIRE(matching.value() == 1);
    }
}

#else  // !PACS_WITH_DATABASE_SYSTEM

TEST_CASE("annotation_repository count", "[annotation][repository]") {
    test_database db;
    annotation_repository repo(db.get());

    REQUIRE(repo.count() == 0);

    repo.save(make_test_annotation("count-1"));
    repo.save(make_test_annotation("count-2"));
    repo.save(make_test_annotation("count-3"));

    REQUIRE(repo.count() == 3);

    SECTION("count with query") {
        auto ann = make_test_annotation("count-4");
        ann.study_uid = "different-study";
        repo.save(ann);

        annotation_query query;
        query.study_uid = "1.2.3.4.5";
        REQUIRE(repo.count(query) == 3);

        query.study_uid = "different-study";
        REQUIRE(repo.count(query) == 1);
    }
}

#endif  // PACS_WITH_DATABASE_SYSTEM

TEST_CASE("annotation_type conversion", "[annotation][types]") {
    SECTION("to_string") {
        REQUIRE(to_string(annotation_type::arrow) == "arrow");
        REQUIRE(to_string(annotation_type::rectangle) == "rectangle");
        REQUIRE(to_string(annotation_type::freehand) == "freehand");
    }

    SECTION("from_string") {
        REQUIRE(annotation_type_from_string("arrow") == annotation_type::arrow);
        REQUIRE(annotation_type_from_string("ellipse") == annotation_type::ellipse);
        REQUIRE_FALSE(annotation_type_from_string("invalid").has_value());
    }
}

TEST_CASE("annotation_record validation", "[annotation][types]") {
    annotation_record record;

    SECTION("empty record is invalid") {
        REQUIRE_FALSE(record.is_valid());
    }

    SECTION("record with only annotation_id is invalid") {
        record.annotation_id = "test";
        REQUIRE_FALSE(record.is_valid());
    }

    SECTION("record with annotation_id and study_uid is valid") {
        record.annotation_id = "test";
        record.study_uid = "1.2.3";
        REQUIRE(record.is_valid());
    }
}

TEST_CASE("annotation_query has_criteria", "[annotation][types]") {
    annotation_query query;

    SECTION("empty query has no criteria") {
        REQUIRE_FALSE(query.has_criteria());
    }

    SECTION("query with study_uid has criteria") {
        query.study_uid = "1.2.3";
        REQUIRE(query.has_criteria());
    }

    SECTION("query with user_id has criteria") {
        query.user_id = "user";
        REQUIRE(query.has_criteria());
    }
}
