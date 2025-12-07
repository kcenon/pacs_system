/**
 * @file query_cache_test.cpp
 * @brief Unit tests for query_cache class
 */

#include <pacs/services/cache/query_cache.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <chrono>
#include <string>
#include <thread>

using namespace pacs::services::cache;
using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────
// Basic Operations
// ─────────────────────────────────────────────────────

TEST_CASE("query_cache basic operations", "[cache][query]") {
    query_cache_config config;
    config.max_entries = 100;
    config.ttl = 60s;
    config.cache_name = "test_query_cache";

    query_cache cache(config);

    SECTION("empty cache returns nullopt") {
        auto result = cache.get("nonexistent");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("put and get query result") {
        cached_query_result result;
        result.data = {0x01, 0x02, 0x03, 0x04};
        result.match_count = 5;
        result.query_level = "STUDY";
        result.cached_at = std::chrono::steady_clock::now();

        cache.put("test_key", result);

        auto retrieved = cache.get("test_key");
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->data.size() == 4);
        REQUIRE(retrieved->match_count == 5);
        REQUIRE(retrieved->query_level == "STUDY");
    }

    SECTION("invalidate removes entry") {
        cached_query_result result;
        result.data = {0x01};
        cache.put("key1", result);

        REQUIRE(cache.size() == 1);

        bool removed = cache.invalidate("key1");
        REQUIRE(removed);
        REQUIRE(cache.size() == 0);
    }

    SECTION("clear removes all entries") {
        cached_query_result result;
        result.data = {0x01};

        cache.put("key1", result);
        cache.put("key2", result);
        cache.put("key3", result);

        REQUIRE(cache.size() == 3);

        cache.clear();
        REQUIRE(cache.empty());
    }
}

// ─────────────────────────────────────────────────────
// Key Generation
// ─────────────────────────────────────────────────────

TEST_CASE("query_cache key generation", "[cache][query][keys]") {
    SECTION("build_key creates deterministic key") {
        auto key1 = query_cache::build_key("STUDY", {
            {"PatientID", "12345"},
            {"StudyDate", "20240101"}
        });

        auto key2 = query_cache::build_key("STUDY", {
            {"PatientID", "12345"},
            {"StudyDate", "20240101"}
        });

        REQUIRE(key1 == key2);
    }

    SECTION("build_key sorts parameters") {
        // Parameters in different order should produce same key
        auto key1 = query_cache::build_key("STUDY", {
            {"PatientID", "12345"},
            {"StudyDate", "20240101"}
        });

        auto key2 = query_cache::build_key("STUDY", {
            {"StudyDate", "20240101"},
            {"PatientID", "12345"}
        });

        REQUIRE(key1 == key2);
    }

    SECTION("build_key includes query level") {
        auto study_key = query_cache::build_key("STUDY", {{"PatientID", "12345"}});
        auto patient_key = query_cache::build_key("PATIENT", {{"PatientID", "12345"}});

        REQUIRE(study_key != patient_key);
        REQUIRE_THAT(study_key, Catch::Matchers::StartsWith("STUDY:"));
        REQUIRE_THAT(patient_key, Catch::Matchers::StartsWith("PATIENT:"));
    }

    SECTION("build_key_with_ae includes AE title") {
        auto key1 = query_cache::build_key_with_ae("MODALITY1", "STUDY", {
            {"PatientID", "12345"}
        });

        auto key2 = query_cache::build_key_with_ae("MODALITY2", "STUDY", {
            {"PatientID", "12345"}
        });

        REQUIRE(key1 != key2);
        REQUIRE_THAT(key1, Catch::Matchers::StartsWith("MODALITY1/"));
        REQUIRE_THAT(key2, Catch::Matchers::StartsWith("MODALITY2/"));
    }

    SECTION("build_key handles empty params") {
        auto key = query_cache::build_key("STUDY", {});
        REQUIRE(key == "STUDY:");
    }

    SECTION("build_key handles single param") {
        auto key = query_cache::build_key("STUDY", {{"PatientID", "12345"}});
        REQUIRE(key == "STUDY:PatientID=12345");
    }
}

// ─────────────────────────────────────────────────────
// Statistics
// ─────────────────────────────────────────────────────

TEST_CASE("query_cache statistics", "[cache][query][stats]") {
    query_cache_config config;
    config.max_entries = 100;
    config.ttl = std::chrono::seconds{60};

    query_cache cache(config);

    SECTION("hit rate tracking") {
        cached_query_result result;
        result.data = {0x01};

        cache.put("key1", result);

        (void)cache.get("key1");     // Hit
        (void)cache.get("key1");     // Hit
        (void)cache.get("missing");  // Miss

        auto rate = cache.hit_rate();
        REQUIRE(rate > 60.0);
        REQUIRE(rate < 70.0);
    }

    SECTION("reset_stats clears counters") {
        cached_query_result result;
        result.data = {0x01};

        cache.put("key1", result);
        (void)cache.get("key1");
        (void)cache.get("missing");

        cache.reset_stats();

        const auto& stats = cache.stats();
        REQUIRE(stats.hits.load() == 0);
        REQUIRE(stats.misses.load() == 0);
    }
}

// ─────────────────────────────────────────────────────
// TTL Expiration
// ─────────────────────────────────────────────────────

TEST_CASE("query_cache TTL expiration", "[cache][query][ttl]") {
    query_cache_config config;
    config.max_entries = 100;
    config.ttl = std::chrono::seconds{1};  // 1 second TTL for testing

    query_cache cache(config);

    SECTION("expired entries are not returned") {
        cached_query_result result;
        result.data = {0x01, 0x02, 0x03};

        cache.put("key1", result);
        REQUIRE(cache.get("key1").has_value());

        std::this_thread::sleep_for(std::chrono::milliseconds{1100});

        REQUIRE_FALSE(cache.get("key1").has_value());
    }

    SECTION("purge_expired removes old entries") {
        cached_query_result result;
        result.data = {0x01};

        cache.put("key1", result);
        cache.put("key2", result);

        std::this_thread::sleep_for(std::chrono::milliseconds{1100});

        auto removed = cache.purge_expired();
        REQUIRE(removed == 2);
        REQUIRE(cache.empty());
    }
}

// ─────────────────────────────────────────────────────
// Move Semantics
// ─────────────────────────────────────────────────────

TEST_CASE("query_cache move semantics", "[cache][query][move]") {
    query_cache_config config;
    config.max_entries = 100;
    config.ttl = 60s;

    query_cache cache(config);

    SECTION("move put for large data") {
        cached_query_result result;
        result.data.resize(1024 * 1024);  // 1MB
        std::fill(result.data.begin(), result.data.end(), 0xAB);
        result.match_count = 10;

        cache.put("large_key", std::move(result));

        auto retrieved = cache.get("large_key");
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->data.size() == 1024 * 1024);
        REQUIRE(retrieved->data[0] == 0xAB);
        REQUIRE(retrieved->match_count == 10);
    }
}
