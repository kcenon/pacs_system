/**
 * @file simple_lru_cache_test.cpp
 * @brief Unit tests for simple_lru_cache template class
 */

#include <pacs/services/cache/simple_lru_cache.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace pacs::services::cache;
using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────
// Basic Operations
// ─────────────────────────────────────────────────────

TEST_CASE("simple_lru_cache basic operations", "[cache][lru]") {
    cache_config config;
    config.max_size = 3;
    config.ttl = 60s;

    simple_lru_cache<std::string, int> cache(config);

    SECTION("empty cache returns nullopt") {
        auto result = cache.get("nonexistent");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("put and get single item") {
        cache.put("key1", 42);
        auto result = cache.get("key1");

        REQUIRE(result.has_value());
        REQUIRE(*result == 42);
    }

    SECTION("put multiple items") {
        cache.put("key1", 1);
        cache.put("key2", 2);
        cache.put("key3", 3);

        REQUIRE(cache.get("key1") == 1);
        REQUIRE(cache.get("key2") == 2);
        REQUIRE(cache.get("key3") == 3);
        REQUIRE(cache.size() == 3);
    }

    SECTION("update existing key") {
        cache.put("key1", 100);
        cache.put("key1", 200);

        auto result = cache.get("key1");
        REQUIRE(result.has_value());
        REQUIRE(*result == 200);
        REQUIRE(cache.size() == 1);
    }

    SECTION("contains check") {
        cache.put("key1", 42);

        REQUIRE(cache.contains("key1"));
        REQUIRE_FALSE(cache.contains("nonexistent"));
    }

    SECTION("invalidate removes entry") {
        cache.put("key1", 42);
        REQUIRE(cache.contains("key1"));

        bool removed = cache.invalidate("key1");
        REQUIRE(removed);
        REQUIRE_FALSE(cache.contains("key1"));
        REQUIRE(cache.size() == 0);
    }

    SECTION("invalidate nonexistent key returns false") {
        bool removed = cache.invalidate("nonexistent");
        REQUIRE_FALSE(removed);
    }

    SECTION("clear removes all entries") {
        cache.put("key1", 1);
        cache.put("key2", 2);
        cache.put("key3", 3);

        cache.clear();

        REQUIRE(cache.empty());
        REQUIRE(cache.size() == 0);
    }
}

// ─────────────────────────────────────────────────────
// LRU Eviction
// ─────────────────────────────────────────────────────

TEST_CASE("simple_lru_cache LRU eviction", "[cache][lru][eviction]") {
    simple_lru_cache<std::string, int> cache(3, 60s);  // max 3 entries

    SECTION("evicts oldest when full") {
        cache.put("key1", 1);
        cache.put("key2", 2);
        cache.put("key3", 3);

        // This should evict key1 (oldest)
        cache.put("key4", 4);

        REQUIRE_FALSE(cache.contains("key1"));
        REQUIRE(cache.contains("key2"));
        REQUIRE(cache.contains("key3"));
        REQUIRE(cache.contains("key4"));
        REQUIRE(cache.size() == 3);
    }

    SECTION("access promotes entry") {
        cache.put("key1", 1);
        cache.put("key2", 2);
        cache.put("key3", 3);

        // Access key1 to promote it
        auto _ = cache.get("key1");

        // This should evict key2 (now the oldest)
        cache.put("key4", 4);

        REQUIRE(cache.contains("key1"));  // Was promoted
        REQUIRE_FALSE(cache.contains("key2"));  // Evicted
        REQUIRE(cache.contains("key3"));
        REQUIRE(cache.contains("key4"));
    }

    SECTION("update promotes entry") {
        cache.put("key1", 1);
        cache.put("key2", 2);
        cache.put("key3", 3);

        // Update key1 to promote it
        cache.put("key1", 100);

        // This should evict key2 (now the oldest)
        cache.put("key4", 4);

        REQUIRE(cache.contains("key1"));
        REQUIRE(*cache.get("key1") == 100);
        REQUIRE_FALSE(cache.contains("key2"));
    }

    SECTION("eviction count in stats") {
        cache.put("key1", 1);
        cache.put("key2", 2);
        cache.put("key3", 3);
        cache.put("key4", 4);  // Evicts key1
        cache.put("key5", 5);  // Evicts key2

        const auto& stats = cache.stats();
        REQUIRE(stats.evictions.load() == 2);
    }
}

// ─────────────────────────────────────────────────────
// TTL Expiration
// ─────────────────────────────────────────────────────

TEST_CASE("simple_lru_cache TTL expiration", "[cache][lru][ttl]") {
    SECTION("expired entries are not returned") {
        simple_lru_cache<std::string, int> cache(100, 1s);  // 1 second TTL

        cache.put("key1", 42);
        REQUIRE(cache.get("key1").has_value());

        // Wait for expiration
        std::this_thread::sleep_for(1100ms);

        auto result = cache.get("key1");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("purge_expired removes expired entries") {
        simple_lru_cache<std::string, int> cache(100, 1s);

        cache.put("key1", 1);
        cache.put("key2", 2);
        cache.put("key3", 3);

        std::this_thread::sleep_for(1100ms);

        auto removed = cache.purge_expired();
        REQUIRE(removed == 3);
        REQUIRE(cache.empty());
    }

    SECTION("expiration updates stats") {
        simple_lru_cache<std::string, int> cache(100, 1s);

        cache.put("key1", 42);
        std::this_thread::sleep_for(1100ms);

        // This should trigger expiration
        auto _ = cache.get("key1");

        const auto& stats = cache.stats();
        REQUIRE(stats.expirations.load() >= 1);
    }
}

// ─────────────────────────────────────────────────────
// Statistics
// ─────────────────────────────────────────────────────

TEST_CASE("simple_lru_cache statistics", "[cache][lru][stats]") {
    simple_lru_cache<std::string, int> cache(100, 60s);

    SECTION("hit/miss tracking") {
        cache.put("key1", 42);

        (void)cache.get("key1");     // Hit
        (void)cache.get("key1");     // Hit
        (void)cache.get("missing");  // Miss

        const auto& stats = cache.stats();
        REQUIRE(stats.hits.load() == 2);
        REQUIRE(stats.misses.load() == 1);
    }

    SECTION("hit rate calculation") {
        cache.put("key1", 42);

        (void)cache.get("key1");     // Hit
        (void)cache.get("key1");     // Hit
        (void)cache.get("key1");     // Hit
        (void)cache.get("missing");  // Miss

        auto rate = cache.hit_rate();
        REQUIRE_THAT(rate, Catch::Matchers::WithinAbs(75.0, 0.01));
    }

    SECTION("hit rate is 0 when no accesses") {
        REQUIRE(cache.hit_rate() == 0.0);
    }

    SECTION("insertion count") {
        cache.put("key1", 1);
        cache.put("key2", 2);
        cache.put("key1", 100);  // Update, not new insertion

        const auto& stats = cache.stats();
        REQUIRE(stats.insertions.load() == 2);  // Only new keys count
    }

    SECTION("current_size tracking") {
        cache.put("key1", 1);
        cache.put("key2", 2);

        const auto& stats = cache.stats();
        REQUIRE(stats.current_size.load() == 2);

        cache.invalidate("key1");
        REQUIRE(stats.current_size.load() == 1);
    }

    SECTION("reset_stats clears counters but not size") {
        cache.put("key1", 42);
        (void)cache.get("key1");
        (void)cache.get("missing");

        cache.reset_stats();

        const auto& stats = cache.stats();
        REQUIRE(stats.hits.load() == 0);
        REQUIRE(stats.misses.load() == 0);
        REQUIRE(stats.insertions.load() == 0);
        // current_size is not reset
    }
}

// ─────────────────────────────────────────────────────
// Thread Safety
// ─────────────────────────────────────────────────────

TEST_CASE("simple_lru_cache thread safety", "[cache][lru][threads]") {
    simple_lru_cache<int, int> cache(1000, 60s);

    SECTION("concurrent writes") {
        constexpr int num_threads = 4;
        constexpr int items_per_thread = 100;

        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&cache, t]() {
                for (int i = 0; i < items_per_thread; ++i) {
                    int key = t * items_per_thread + i;
                    cache.put(key, key * 2);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        // All items should be present (cache is large enough)
        REQUIRE(cache.size() == num_threads * items_per_thread);
    }

    SECTION("concurrent reads and writes") {
        // Pre-populate cache
        for (int i = 0; i < 100; ++i) {
            cache.put(i, i * 2);
        }

        std::atomic<int> hits{0};
        std::atomic<int> misses{0};

        std::vector<std::thread> threads;

        // Reader threads
        for (int t = 0; t < 2; ++t) {
            threads.emplace_back([&cache, &hits, &misses]() {
                for (int i = 0; i < 200; ++i) {
                    auto result = cache.get(i % 150);
                    if (result) {
                        hits.fetch_add(1);
                    } else {
                        misses.fetch_add(1);
                    }
                }
            });
        }

        // Writer threads
        for (int t = 0; t < 2; ++t) {
            threads.emplace_back([&cache, t]() {
                for (int i = 0; i < 100; ++i) {
                    cache.put(100 + t * 100 + i, i);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        // Just verify no crashes and some operations completed
        REQUIRE(hits.load() + misses.load() == 400);
    }

    SECTION("concurrent eviction stress test") {
        // Small cache to force frequent evictions
        simple_lru_cache<int, int> small_cache(50, 60s);

        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&small_cache, t]() {
                for (int i = 0; i < 200; ++i) {
                    int key = t * 1000 + i;
                    small_cache.put(key, key);
                    (void)small_cache.get(key);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        // Cache should never exceed max_size
        REQUIRE(small_cache.size() <= 50);
    }
}

// ─────────────────────────────────────────────────────
// Edge Cases
// ─────────────────────────────────────────────────────

TEST_CASE("simple_lru_cache edge cases", "[cache][lru][edge]") {
    SECTION("cache size of 1") {
        simple_lru_cache<std::string, int> cache(1, 60s);

        cache.put("key1", 1);
        cache.put("key2", 2);

        REQUIRE_FALSE(cache.contains("key1"));
        REQUIRE(cache.contains("key2"));
        REQUIRE(cache.size() == 1);
    }

    SECTION("empty string key") {
        simple_lru_cache<std::string, int> cache(100, 60s);

        cache.put("", 42);
        auto result = cache.get("");

        REQUIRE(result.has_value());
        REQUIRE(*result == 42);
    }

    SECTION("move semantics for value") {
        simple_lru_cache<std::string, std::vector<int>> cache(100, 60s);

        std::vector<int> data = {1, 2, 3, 4, 5};
        cache.put("key1", std::move(data));

        auto result = cache.get("key1");
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 5);
    }

    SECTION("max_size 0 defaults to 1") {
        cache_config config;
        config.max_size = 0;
        simple_lru_cache<std::string, int> cache(config);

        REQUIRE(cache.max_size() == 1);
    }
}

// ─────────────────────────────────────────────────────
// Configuration
// ─────────────────────────────────────────────────────

TEST_CASE("simple_lru_cache configuration", "[cache][lru][config]") {
    SECTION("config is accessible") {
        cache_config config;
        config.max_size = 500;
        config.ttl = 120s;
        config.cache_name = "test_cache";

        simple_lru_cache<std::string, int> cache(config);

        REQUIRE(cache.max_size() == 500);
        REQUIRE(cache.ttl() == 120s);
        REQUIRE(cache.name() == "test_cache");
    }

    SECTION("constructor with size and ttl") {
        simple_lru_cache<std::string, int> cache(200, 90s);

        REQUIRE(cache.max_size() == 200);
        REQUIRE(cache.ttl() == 90s);
    }
}
