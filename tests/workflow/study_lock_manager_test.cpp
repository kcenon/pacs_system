/**
 * @file study_lock_manager_test.cpp
 * @brief Unit tests for study_lock_manager class
 */

#include <catch2/catch_test_macros.hpp>

#include <kcenon/common/patterns/result.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include "pacs/workflow/study_lock_manager.hpp"

using namespace pacs::workflow;
using namespace std::chrono_literals;

// ============================================================================
// Construction Tests
// ============================================================================

TEST_CASE("study_lock_manager default construction", "[study_lock_manager][construction]") {
    study_lock_manager manager;

    CHECK(manager.get_stats().active_locks == 0);
    CHECK(manager.get_all_locks().empty());
}

TEST_CASE("study_lock_manager configuration", "[study_lock_manager][construction]") {
    study_lock_manager_config config;
    config.default_timeout = 60s;
    config.max_shared_locks = 10;
    config.allow_force_unlock = false;

    study_lock_manager manager{config};

    auto retrieved_config = manager.get_config();
    CHECK(retrieved_config.default_timeout == 60s);
    CHECK(retrieved_config.max_shared_locks == 10);
    CHECK(retrieved_config.allow_force_unlock == false);
}

// ============================================================================
// Lock Acquisition Tests
// ============================================================================

TEST_CASE("study_lock_manager exclusive lock", "[study_lock_manager][lock]") {
    study_lock_manager manager;
    const std::string study_uid = "1.2.3.4.5";
    const std::string reason = "Test lock";
    const std::string holder = "test_user";

    SECTION("acquire exclusive lock") {
        auto result = manager.lock(study_uid, reason, holder);

        REQUIRE(result.is_ok());
        auto token = result.value();
        CHECK(token.study_uid == study_uid);
        CHECK(token.type == lock_type::exclusive);
        CHECK_FALSE(token.token_id.empty());
        CHECK(token.is_valid());

        CHECK(manager.is_locked(study_uid));
        CHECK(manager.is_locked(study_uid, lock_type::exclusive));
    }

    SECTION("cannot acquire second exclusive lock") {
        auto result1 = manager.lock(study_uid, reason, holder);
        REQUIRE(result1.is_ok());

        auto result2 = manager.lock(study_uid, reason, "another_user");
        REQUIRE(result2.is_err());
        CHECK(result2.error().code == lock_error::already_locked);
    }

    SECTION("lock with timeout") {
        study_lock_manager_config config;
        config.default_timeout = 1s;
        study_lock_manager timeout_manager{config};

        auto result = timeout_manager.lock(study_uid, reason, holder);
        REQUIRE(result.is_ok());
        auto token = result.value();

        CHECK(token.expires_at.has_value());
        CHECK(token.is_valid());

        // Wait for expiration
        std::this_thread::sleep_for(1100ms);
        CHECK(token.is_expired());
        CHECK_FALSE(timeout_manager.is_locked(study_uid));
    }
}

TEST_CASE("study_lock_manager shared lock", "[study_lock_manager][lock]") {
    study_lock_manager manager;
    const std::string study_uid = "1.2.3.4.5";
    const std::string reason = "Read access";

    SECTION("acquire shared lock") {
        auto result = manager.lock(study_uid, lock_type::shared, reason, "user1");

        REQUIRE(result.is_ok());
        auto token = result.value();
        CHECK(token.type == lock_type::shared);
        CHECK(manager.is_locked(study_uid, lock_type::shared));
    }

    SECTION("multiple shared locks allowed") {
        auto result1 = manager.lock(study_uid, lock_type::shared, reason, "user1");
        auto result2 = manager.lock(study_uid, lock_type::shared, reason, "user2");
        auto result3 = manager.lock(study_uid, lock_type::shared, reason, "user3");

        REQUIRE(result1.is_ok());
        REQUIRE(result2.is_ok());
        REQUIRE(result3.is_ok());

        auto info = manager.get_lock_info(study_uid);
        REQUIRE(info.has_value());
        CHECK(info->shared_count == 3);
    }

    SECTION("exclusive lock blocks shared lock") {
        auto exclusive_result = manager.lock(study_uid, lock_type::exclusive, reason, "owner");
        REQUIRE(exclusive_result.is_ok());

        auto shared_result = manager.lock(study_uid, lock_type::shared, reason, "reader");
        REQUIRE(shared_result.is_err());
        CHECK(shared_result.error().code == lock_error::already_locked);
    }

    SECTION("shared lock blocks exclusive lock") {
        auto shared_result = manager.lock(study_uid, lock_type::shared, reason, "reader");
        REQUIRE(shared_result.is_ok());

        auto exclusive_result = manager.lock(study_uid, lock_type::exclusive, reason, "owner");
        REQUIRE(exclusive_result.is_err());
        CHECK(exclusive_result.error().code == lock_error::already_locked);
    }

    SECTION("max shared locks enforced") {
        study_lock_manager_config config;
        config.max_shared_locks = 3;
        study_lock_manager limited_manager{config};

        for (int i = 0; i < 3; ++i) {
            auto result = limited_manager.lock(
                study_uid, lock_type::shared, reason,
                "user" + std::to_string(i));
            REQUIRE(result.is_ok());
        }

        auto overflow_result = limited_manager.lock(
            study_uid, lock_type::shared, reason, "user_overflow");
        REQUIRE(overflow_result.is_err());
        CHECK(overflow_result.error().code == lock_error::max_shared_exceeded);
    }
}

TEST_CASE("study_lock_manager migration lock", "[study_lock_manager][lock]") {
    study_lock_manager manager;
    const std::string study_uid = "1.2.3.4.5";

    auto result = manager.lock(study_uid, lock_type::migration, "Migration in progress", "migration_service");

    REQUIRE(result.is_ok());
    CHECK(result.value().type == lock_type::migration);
    CHECK(manager.is_locked(study_uid, lock_type::migration));
}

// ============================================================================
// Lock Release Tests
// ============================================================================

TEST_CASE("study_lock_manager unlock by token", "[study_lock_manager][unlock]") {
    study_lock_manager manager;
    const std::string study_uid = "1.2.3.4.5";

    auto lock_result = manager.lock(study_uid, "Test", "user");
    REQUIRE(lock_result.is_ok());
    auto token = lock_result.value();

    CHECK(manager.is_locked(study_uid));

    auto unlock_result = manager.unlock(token);
    REQUIRE(unlock_result.is_ok());
    CHECK_FALSE(manager.is_locked(study_uid));
}

TEST_CASE("study_lock_manager unlock by holder", "[study_lock_manager][unlock]") {
    study_lock_manager manager;
    const std::string study_uid = "1.2.3.4.5";
    const std::string holder = "test_user";

    auto lock_result = manager.lock(study_uid, "Test", holder);
    REQUIRE(lock_result.is_ok());

    SECTION("unlock with correct holder") {
        auto unlock_result = manager.unlock(study_uid, holder);
        REQUIRE(unlock_result.is_ok());
        CHECK_FALSE(manager.is_locked(study_uid));
    }

    SECTION("unlock with wrong holder fails") {
        auto unlock_result = manager.unlock(study_uid, "wrong_user");
        REQUIRE(unlock_result.is_err());
        CHECK(unlock_result.error().code == lock_error::permission_denied);
        CHECK(manager.is_locked(study_uid));
    }
}

TEST_CASE("study_lock_manager force unlock", "[study_lock_manager][unlock]") {
    study_lock_manager manager;
    const std::string study_uid = "1.2.3.4.5";

    auto lock_result = manager.lock(study_uid, "Test", "user");
    REQUIRE(lock_result.is_ok());

    SECTION("force unlock succeeds when allowed") {
        auto force_result = manager.force_unlock(study_uid, "Admin override");
        REQUIRE(force_result.is_ok());
        CHECK_FALSE(manager.is_locked(study_uid));
    }

    SECTION("force unlock fails when disabled") {
        study_lock_manager_config config;
        config.allow_force_unlock = false;
        study_lock_manager restricted_manager{config};

        auto lock_result2 = restricted_manager.lock(study_uid, "Test", "user");
        REQUIRE(lock_result2.is_ok());

        auto force_result = restricted_manager.force_unlock(study_uid);
        REQUIRE(force_result.is_err());
        CHECK(force_result.error().code == lock_error::permission_denied);
    }
}

TEST_CASE("study_lock_manager unlock all by holder", "[study_lock_manager][unlock]") {
    study_lock_manager manager;
    const std::string holder = "test_user";

    // Lock multiple studies
    REQUIRE(manager.lock("study1", "Test", holder).is_ok());
    REQUIRE(manager.lock("study2", "Test", holder).is_ok());
    REQUIRE(manager.lock("study3", "Test", "other_user").is_ok());

    auto count = manager.unlock_all_by_holder(holder);

    CHECK(count == 2);
    CHECK_FALSE(manager.is_locked("study1"));
    CHECK_FALSE(manager.is_locked("study2"));
    CHECK(manager.is_locked("study3"));  // Still locked by other user
}

// ============================================================================
// Lock Status Tests
// ============================================================================

TEST_CASE("study_lock_manager lock info", "[study_lock_manager][status]") {
    study_lock_manager manager;
    const std::string study_uid = "1.2.3.4.5";
    const std::string reason = "Test reason";
    const std::string holder = "test_user";

    auto lock_result = manager.lock(study_uid, lock_type::exclusive, reason, holder);
    REQUIRE(lock_result.is_ok());

    auto info = manager.get_lock_info(study_uid);
    REQUIRE(info.has_value());
    CHECK(info->study_uid == study_uid);
    CHECK(info->type == lock_type::exclusive);
    CHECK(info->reason == reason);
    CHECK(info->holder == holder);
    CHECK_FALSE(info->token_id.empty());
}

TEST_CASE("study_lock_manager get lock info by token", "[study_lock_manager][status]") {
    study_lock_manager manager;
    const std::string study_uid = "1.2.3.4.5";

    auto lock_result = manager.lock(study_uid, "Test", "user");
    REQUIRE(lock_result.is_ok());
    auto token = lock_result.value();

    auto info = manager.get_lock_info_by_token(token.token_id);
    REQUIRE(info.has_value());
    CHECK(info->study_uid == study_uid);
}

TEST_CASE("study_lock_manager validate token", "[study_lock_manager][status]") {
    study_lock_manager manager;
    const std::string study_uid = "1.2.3.4.5";

    auto lock_result = manager.lock(study_uid, "Test", "user");
    REQUIRE(lock_result.is_ok());
    auto token = lock_result.value();

    SECTION("valid token") {
        CHECK(manager.validate_token(token));
    }

    SECTION("invalid token after unlock") {
        REQUIRE(manager.unlock(token).is_ok());
        CHECK_FALSE(manager.validate_token(token));
    }

    SECTION("invalid token ID") {
        lock_token fake_token;
        fake_token.token_id = "fake_token_id";
        fake_token.study_uid = study_uid;
        CHECK_FALSE(manager.validate_token(fake_token));
    }
}

TEST_CASE("study_lock_manager refresh lock", "[study_lock_manager][status]") {
    study_lock_manager_config config;
    config.default_timeout = 5s;
    study_lock_manager manager{config};

    const std::string study_uid = "1.2.3.4.5";

    auto lock_result = manager.lock(study_uid, "Test", "user");
    REQUIRE(lock_result.is_ok());
    auto token = lock_result.value();

    auto original_expiry = token.expires_at;

    // Refresh the lock
    auto refresh_result = manager.refresh_lock(token, 10s);
    REQUIRE(refresh_result.is_ok());

    auto refreshed_token = refresh_result.value();
    REQUIRE(refreshed_token.expires_at.has_value());

    // New expiry should be later than original
    CHECK(*refreshed_token.expires_at > *original_expiry);
}

// ============================================================================
// Lock Query Tests
// ============================================================================

TEST_CASE("study_lock_manager get all locks", "[study_lock_manager][query]") {
    study_lock_manager manager;

    REQUIRE(manager.lock("study1", "Reason 1", "user1").is_ok());
    REQUIRE(manager.lock("study2", "Reason 2", "user2").is_ok());
    REQUIRE(manager.lock("study3", lock_type::shared, "Reason 3", "user3").is_ok());

    auto all_locks = manager.get_all_locks();
    CHECK(all_locks.size() == 3);
}

TEST_CASE("study_lock_manager get locks by holder", "[study_lock_manager][query]") {
    study_lock_manager manager;

    REQUIRE(manager.lock("study1", "Reason", "user1").is_ok());
    REQUIRE(manager.lock("study2", "Reason", "user1").is_ok());
    REQUIRE(manager.lock("study3", "Reason", "user2").is_ok());

    auto user1_locks = manager.get_locks_by_holder("user1");
    CHECK(user1_locks.size() == 2);

    auto user2_locks = manager.get_locks_by_holder("user2");
    CHECK(user2_locks.size() == 1);
}

TEST_CASE("study_lock_manager get locks by type", "[study_lock_manager][query]") {
    study_lock_manager manager;

    REQUIRE(manager.lock("study1", lock_type::exclusive, "Reason", "user1").is_ok());
    REQUIRE(manager.lock("study2", lock_type::shared, "Reason", "user2").is_ok());
    REQUIRE(manager.lock("study3", lock_type::migration, "Reason", "user3").is_ok());
    REQUIRE(manager.lock("study4", lock_type::exclusive, "Reason", "user4").is_ok());

    auto exclusive_locks = manager.get_locks_by_type(lock_type::exclusive);
    CHECK(exclusive_locks.size() == 2);

    auto shared_locks = manager.get_locks_by_type(lock_type::shared);
    CHECK(shared_locks.size() == 1);

    auto migration_locks = manager.get_locks_by_type(lock_type::migration);
    CHECK(migration_locks.size() == 1);
}

// ============================================================================
// Cleanup and Maintenance Tests
// ============================================================================

TEST_CASE("study_lock_manager cleanup expired locks", "[study_lock_manager][maintenance]") {
    study_lock_manager_config config;
    config.default_timeout = 1s;
    study_lock_manager manager{config};

    // Create locks that will expire
    REQUIRE(manager.lock("study1", "Reason", "user1").is_ok());
    REQUIRE(manager.lock("study2", "Reason", "user2").is_ok());

    CHECK(manager.get_all_locks().size() == 2);

    // Wait for expiration
    std::this_thread::sleep_for(1100ms);

    auto cleaned = manager.cleanup_expired_locks();
    CHECK(cleaned == 2);
    CHECK(manager.get_all_locks().empty());
}

TEST_CASE("study_lock_manager get expired locks", "[study_lock_manager][maintenance]") {
    study_lock_manager_config config;
    config.default_timeout = 1s;
    study_lock_manager manager{config};

    REQUIRE(manager.lock("study1", "Reason", "user1").is_ok());
    REQUIRE(manager.lock("study2", "Reason", "user2", 10s).is_ok());  // Longer timeout

    std::this_thread::sleep_for(1100ms);

    auto expired = manager.get_expired_locks();
    CHECK(expired.size() == 1);
    CHECK(expired[0].study_uid == "study1");
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("study_lock_manager statistics", "[study_lock_manager][stats]") {
    study_lock_manager manager;

    // Acquire some locks
    auto token1 = manager.lock("study1", "Reason", "user1");
    auto token2 = manager.lock("study2", lock_type::shared, "Reason", "user2");
    REQUIRE(manager.lock("study3", lock_type::migration, "Reason", "user3").is_ok());

    auto stats = manager.get_stats();
    CHECK(stats.active_locks == 3);
    CHECK(stats.exclusive_locks == 1);
    CHECK(stats.shared_locks == 1);
    CHECK(stats.migration_locks == 1);
    CHECK(stats.total_acquisitions == 3);

    // Release one lock
    REQUIRE(manager.unlock(token1.value()).is_ok());

    stats = manager.get_stats();
    CHECK(stats.active_locks == 2);
    CHECK(stats.total_releases == 1);
}

TEST_CASE("study_lock_manager reset statistics", "[study_lock_manager][stats]") {
    study_lock_manager manager;

    REQUIRE(manager.lock("study1", "Reason", "user1").is_ok());
    auto stats_before = manager.get_stats();
    CHECK(stats_before.total_acquisitions == 1);

    manager.reset_stats();

    auto stats_after = manager.get_stats();
    CHECK(stats_after.total_acquisitions == 0);
    CHECK(stats_after.active_locks == 1);  // Active locks still counted
}

// ============================================================================
// Event Callback Tests
// ============================================================================

TEST_CASE("study_lock_manager event callbacks", "[study_lock_manager][callbacks]") {
    study_lock_manager manager;
    const std::string study_uid = "1.2.3.4.5";

    std::atomic<int> acquired_count{0};
    std::atomic<int> released_count{0};

    manager.set_on_lock_acquired([&](const std::string&, const lock_info&) {
        ++acquired_count;
    });

    manager.set_on_lock_released([&](const std::string&, const lock_info&) {
        ++released_count;
    });

    SECTION("acquisition callback") {
        REQUIRE(manager.lock(study_uid, "Reason", "user").is_ok());
        CHECK(acquired_count == 1);
    }

    SECTION("release callback") {
        auto token = manager.lock(study_uid, "Reason", "user");
        REQUIRE(token.is_ok());
        REQUIRE(manager.unlock(token.value()).is_ok());
        CHECK(released_count == 1);
    }
}

// ============================================================================
// Concurrent Access Tests
// ============================================================================

TEST_CASE("study_lock_manager concurrent access", "[study_lock_manager][concurrency]") {
    study_lock_manager manager;
    const int num_threads = 10;
    const std::string study_uid = "1.2.3.4.5";

    std::atomic<int> success_count{0};
    std::atomic<int> contention_count{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, [&, i]() {
            auto result = manager.lock(
                study_uid,
                lock_type::exclusive,
                "Concurrent test",
                "thread_" + std::to_string(i));

            if (result.is_ok()) {
                ++success_count;
                std::this_thread::sleep_for(10ms);
                (void)manager.unlock(result.value());
            } else {
                ++contention_count;
            }
        }));
    }

    for (auto& f : futures) {
        f.wait();
    }

    // Only one thread should have succeeded at a time
    // Due to sequential locking, we expect varying results
    CHECK(success_count >= 1);
    auto stats = manager.get_stats();
    CHECK(stats.contention_count > 0);
}

TEST_CASE("study_lock_manager concurrent shared locks", "[study_lock_manager][concurrency]") {
    study_lock_manager manager;
    const int num_threads = 10;
    const std::string study_uid = "1.2.3.4.5";

    std::atomic<int> success_count{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, [&, i]() {
            auto result = manager.lock(
                study_uid,
                lock_type::shared,
                "Shared access",
                "thread_" + std::to_string(i));

            if (result.is_ok()) {
                ++success_count;
            }
        }));
    }

    for (auto& f : futures) {
        f.wait();
    }

    // All threads should succeed with shared locks
    CHECK(success_count == num_threads);
}

// ============================================================================
// Lock Type Conversion Tests
// ============================================================================

TEST_CASE("lock_type to_string and parse", "[study_lock_manager][utility]") {
    SECTION("to_string") {
        CHECK(to_string(lock_type::exclusive) == "exclusive");
        CHECK(to_string(lock_type::shared) == "shared");
        CHECK(to_string(lock_type::migration) == "migration");
    }

    SECTION("parse_lock_type") {
        CHECK(parse_lock_type("exclusive") == lock_type::exclusive);
        CHECK(parse_lock_type("shared") == lock_type::shared);
        CHECK(parse_lock_type("migration") == lock_type::migration);
        CHECK_FALSE(parse_lock_type("invalid").has_value());
    }
}
