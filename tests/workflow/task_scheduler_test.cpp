/**
 * @file task_scheduler_test.cpp
 * @brief Unit tests for task_scheduler class
 *
 * Tests the task scheduler service for automated PACS operations
 * like cleanup, archiving, and verification.
 */

#include <pacs/workflow/task_scheduler.hpp>
#include <pacs/workflow/task_scheduler_config.hpp>

#include <pacs/storage/index_database.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace pacs::workflow;
using namespace pacs::storage;
using namespace std::chrono_literals;

namespace {

/**
 * @brief Create a test database for scheduler testing
 */
auto create_test_database() -> std::unique_ptr<index_database> {
    auto result = index_database::open(":memory:");
    if (!result.is_ok()) {
        throw std::runtime_error("Failed to create test database");
    }
    return std::move(result.value());
}

}  // namespace

// ============================================================================
// cron_schedule Tests
// ============================================================================

TEST_CASE("cron_schedule: factory methods", "[workflow][scheduler][cron]") {
    SECTION("every_minutes creates correct schedule") {
        auto schedule = cron_schedule::every_minutes(5);

        CHECK(schedule.minute == "*/5");
        CHECK(schedule.hour == "*");
        CHECK(schedule.day_of_month == "*");
        CHECK(schedule.month == "*");
        CHECK(schedule.day_of_week == "*");
    }

    SECTION("every_hours creates correct schedule") {
        auto schedule = cron_schedule::every_hours(2);

        CHECK(schedule.minute == "0");
        CHECK(schedule.hour == "*/2");
        CHECK(schedule.day_of_month == "*");
    }

    SECTION("daily_at creates correct schedule") {
        auto schedule = cron_schedule::daily_at(14, 30);

        CHECK(schedule.minute == "30");
        CHECK(schedule.hour == "14");
        CHECK(schedule.day_of_month == "*");
        CHECK(schedule.month == "*");
        CHECK(schedule.day_of_week == "*");
    }

    SECTION("weekly_on creates correct schedule") {
        auto schedule = cron_schedule::weekly_on(1, 9, 0);  // Monday 9:00

        CHECK(schedule.minute == "0");
        CHECK(schedule.hour == "9");
        CHECK(schedule.day_of_week == "1");
    }
}

TEST_CASE("cron_schedule: parsing", "[workflow][scheduler][cron]") {
    SECTION("parse valid cron expression") {
        auto schedule = cron_schedule::parse("0 2 * * *");

        CHECK(schedule.minute == "0");
        CHECK(schedule.hour == "2");
        CHECK(schedule.day_of_month == "*");
        CHECK(schedule.month == "*");
        CHECK(schedule.day_of_week == "*");
    }

    SECTION("to_string roundtrip") {
        cron_schedule original;
        original.minute = "30";
        original.hour = "2";
        original.day_of_month = "*";
        original.month = "*";
        original.day_of_week = "0";

        auto str = original.to_string();
        auto parsed = cron_schedule::parse(str);

        CHECK(parsed.minute == original.minute);
        CHECK(parsed.hour == original.hour);
        CHECK(parsed.day_of_week == original.day_of_week);
    }
}

TEST_CASE("cron_schedule: validation", "[workflow][scheduler][cron]") {
    SECTION("valid schedule") {
        cron_schedule schedule;
        schedule.minute = "0";
        schedule.hour = "2";
        schedule.day_of_month = "*";
        schedule.month = "*";
        schedule.day_of_week = "*";

        CHECK(schedule.is_valid() == true);
    }

    SECTION("invalid schedule with empty field") {
        cron_schedule schedule;
        schedule.minute = "";
        schedule.hour = "2";

        CHECK(schedule.is_valid() == false);
    }
}

// ============================================================================
// task_scheduler_config Tests
// ============================================================================

TEST_CASE("task_scheduler_config: default configuration", "[workflow][scheduler][config]") {
    task_scheduler_config config;

    SECTION("defaults are sensible") {
        CHECK(config.enabled == true);
        CHECK(config.auto_start == false);
        CHECK(config.max_concurrent_tasks == 4);
        CHECK(config.check_interval == std::chrono::seconds{60});
        CHECK(config.persistence_path.empty());
    }

    SECTION("validation passes for enabled config") {
        CHECK(config.is_valid() == true);
    }

    SECTION("validation fails for invalid max_concurrent") {
        config.max_concurrent_tasks = 0;
        CHECK(config.is_valid() == false);
    }
}

TEST_CASE("cleanup_config: retention configuration", "[workflow][scheduler][cleanup]") {
    cleanup_config config;

    SECTION("default retention period") {
        CHECK(config.default_retention == std::chrono::days{365});
    }

    SECTION("modality-specific retention") {
        config.modality_retention["CT"] = std::chrono::days{730};
        config.modality_retention["XR"] = std::chrono::days{180};

        CHECK(config.retention_for("CT") == std::chrono::days{730});
        CHECK(config.retention_for("XR") == std::chrono::days{180});
        CHECK(config.retention_for("MR") == config.default_retention);
    }
}

// ============================================================================
// scheduled_task Tests
// ============================================================================

TEST_CASE("scheduled_task: state conversion", "[workflow][scheduler][task]") {
    CHECK(to_string(task_state::pending) == "pending");
    CHECK(to_string(task_state::running) == "running");
    CHECK(to_string(task_state::completed) == "completed");
    CHECK(to_string(task_state::failed) == "failed");
    CHECK(to_string(task_state::cancelled) == "cancelled");
    CHECK(to_string(task_state::paused) == "paused");
}

TEST_CASE("scheduled_task: type conversion", "[workflow][scheduler][task]") {
    CHECK(to_string(task_type::cleanup) == "cleanup");
    CHECK(to_string(task_type::archive) == "archive");
    CHECK(to_string(task_type::verification) == "verification");
    CHECK(to_string(task_type::custom) == "custom");
}

TEST_CASE("task_execution_record: duration calculation", "[workflow][scheduler][task]") {
    task_execution_record record;
    record.started_at = std::chrono::system_clock::now();

    SECTION("no duration when not ended") {
        CHECK(record.duration().has_value() == false);
    }

    SECTION("duration calculated when ended") {
        std::this_thread::sleep_for(10ms);
        record.ended_at = std::chrono::system_clock::now();

        auto duration = record.duration();
        REQUIRE(duration.has_value());
        CHECK(duration->count() >= 10);
    }
}

// ============================================================================
// task_scheduler Basic Tests
// ============================================================================

TEST_CASE("task_scheduler: construction", "[workflow][scheduler]") {
    auto db = create_test_database();

    SECTION("default construction") {
        task_scheduler_config config;
        config.auto_start = false;

        task_scheduler scheduler(*db, config);

        CHECK(scheduler.is_running() == false);
    }

    SECTION("auto-start enabled") {
        task_scheduler_config config;
        config.auto_start = true;

        task_scheduler scheduler(*db, config);

        CHECK(scheduler.is_running() == true);
        scheduler.stop();
    }
}

TEST_CASE("task_scheduler: lifecycle", "[workflow][scheduler]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;
    config.check_interval = std::chrono::seconds{1};

    task_scheduler scheduler(*db, config);

    SECTION("start and stop") {
        CHECK(scheduler.is_running() == false);

        scheduler.start();
        CHECK(scheduler.is_running() == true);

        scheduler.stop();
        CHECK(scheduler.is_running() == false);
    }

    SECTION("multiple start calls are safe") {
        scheduler.start();
        scheduler.start();  // Should be no-op
        CHECK(scheduler.is_running() == true);

        scheduler.stop();
    }

    SECTION("multiple stop calls are safe") {
        scheduler.start();
        scheduler.stop();
        scheduler.stop();  // Should be no-op
        CHECK(scheduler.is_running() == false);
    }
}

// ============================================================================
// Task Scheduling Tests
// ============================================================================

TEST_CASE("task_scheduler: schedule custom task with interval", "[workflow][scheduler]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;

    task_scheduler scheduler(*db, config);

    std::atomic<int> counter{0};

    auto task_id = scheduler.schedule(
        "test_task",
        "Test task description",
        std::chrono::seconds{60},
        [&counter]() -> std::optional<std::string> {
            ++counter;
            return std::nullopt;  // Success
        });

    CHECK(!task_id.empty());

    auto tasks = scheduler.list_tasks();
    REQUIRE(tasks.size() == 1);
    CHECK(tasks[0].name == "test_task");
    CHECK(tasks[0].type == task_type::custom);
}

TEST_CASE("task_scheduler: schedule custom task with cron", "[workflow][scheduler]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;

    task_scheduler scheduler(*db, config);

    auto task_id = scheduler.schedule(
        "cron_task",
        "Cron scheduled task",
        cron_schedule::daily_at(2, 0),
        []() -> std::optional<std::string> {
            return std::nullopt;
        });

    CHECK(!task_id.empty());

    auto task = scheduler.get_task(task_id);
    REQUIRE(task.has_value());
    CHECK(task->name == "cron_task");
    CHECK(std::holds_alternative<cron_schedule>(task->task_schedule));
}

TEST_CASE("task_scheduler: schedule one-time task", "[workflow][scheduler]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;

    task_scheduler scheduler(*db, config);

    auto execute_at = std::chrono::system_clock::now() + std::chrono::hours{1};

    auto task_id = scheduler.schedule_once(
        "one_time_task",
        "Execute once",
        execute_at,
        []() -> std::optional<std::string> {
            return std::nullopt;
        });

    CHECK(!task_id.empty());

    auto task = scheduler.get_task(task_id);
    REQUIRE(task.has_value());
    CHECK(std::holds_alternative<one_time_schedule>(task->task_schedule));
}

// ============================================================================
// Task Management Tests
// ============================================================================

TEST_CASE("task_scheduler: task management", "[workflow][scheduler]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;

    task_scheduler scheduler(*db, config);

    // Add test tasks
    auto task1_id = scheduler.schedule(
        "task1", "First task", 60s,
        []() { return std::optional<std::string>{}; });

    auto task2_id = scheduler.schedule(
        "task2", "Second task", 120s,
        []() { return std::optional<std::string>{}; });

    SECTION("list_tasks returns all tasks") {
        auto tasks = scheduler.list_tasks();
        CHECK(tasks.size() == 2);
    }

    SECTION("get_task returns correct task") {
        auto task = scheduler.get_task(task1_id);
        REQUIRE(task.has_value());
        CHECK(task->name == "task1");

        auto missing = scheduler.get_task("nonexistent");
        CHECK(!missing.has_value());
    }

    SECTION("cancel_task removes task") {
        CHECK(scheduler.cancel_task(task1_id) == true);

        auto task = scheduler.get_task(task1_id);
        REQUIRE(task.has_value());
        CHECK(task->state == task_state::cancelled);
        CHECK(task->enabled == false);
    }

    SECTION("cancel nonexistent task returns false") {
        CHECK(scheduler.cancel_task("nonexistent") == false);
    }

    SECTION("pause and resume task") {
        CHECK(scheduler.pause_task(task1_id) == true);

        auto paused = scheduler.get_task(task1_id);
        REQUIRE(paused.has_value());
        CHECK(paused->state == task_state::paused);

        CHECK(scheduler.resume_task(task1_id) == true);

        auto resumed = scheduler.get_task(task1_id);
        REQUIRE(resumed.has_value());
        CHECK(resumed->state == task_state::pending);
    }
}

TEST_CASE("task_scheduler: filter tasks by type", "[workflow][scheduler]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;

    // Configure cleanup task
    config.cleanup = cleanup_config{};

    task_scheduler scheduler(*db, config);

    // Add custom task
    scheduler.schedule("custom", "Custom", 60s,
        []() { return std::optional<std::string>{}; });

    SECTION("list by type") {
        auto cleanup_tasks = scheduler.list_tasks(task_type::cleanup);
        CHECK(cleanup_tasks.size() == 1);

        auto custom_tasks = scheduler.list_tasks(task_type::custom);
        CHECK(custom_tasks.size() == 1);

        auto archive_tasks = scheduler.list_tasks(task_type::archive);
        CHECK(archive_tasks.empty());
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("task_scheduler: statistics", "[workflow][scheduler]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;

    task_scheduler scheduler(*db, config);

    scheduler.schedule("task1", "Task 1", 60s,
        []() { return std::optional<std::string>{}; });

    SECTION("initial stats") {
        auto stats = scheduler.get_stats();

        CHECK(stats.scheduled_tasks == 1);
        CHECK(stats.running_tasks == 0);
        CHECK(stats.total_executions == 0);
    }

    SECTION("pending count") {
        CHECK(scheduler.pending_count() == 1);
    }

    SECTION("running count") {
        CHECK(scheduler.running_count() == 0);
    }
}

// ============================================================================
// Cleanup/Archive/Verification Config Tests
// ============================================================================

TEST_CASE("task_scheduler: schedule cleanup task", "[workflow][scheduler][cleanup]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;

    task_scheduler scheduler(*db, config);

    cleanup_config cleanup;
    cleanup.default_retention = std::chrono::days{90};
    cleanup.dry_run = true;

    auto task_id = scheduler.schedule_cleanup(cleanup);
    CHECK(task_id == "cleanup_task");

    auto task = scheduler.get_task(task_id);
    REQUIRE(task.has_value());
    CHECK(task->type == task_type::cleanup);
    CHECK(task->name == "Storage Cleanup");
}

TEST_CASE("task_scheduler: schedule archive task", "[workflow][scheduler][archive]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;

    task_scheduler scheduler(*db, config);

    archive_config archive;
    archive.archive_after = std::chrono::days{30};
    archive.destination = "/archive";

    auto task_id = scheduler.schedule_archive(archive);
    CHECK(task_id == "archive_task");

    auto task = scheduler.get_task(task_id);
    REQUIRE(task.has_value());
    CHECK(task->type == task_type::archive);
}

TEST_CASE("task_scheduler: schedule verification task", "[workflow][scheduler][verification]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;

    task_scheduler scheduler(*db, config);

    verification_config verification;
    verification.interval = std::chrono::hours{12};
    verification.check_checksums = true;

    auto task_id = scheduler.schedule_verification(verification);
    CHECK(task_id == "verification_task");

    auto task = scheduler.get_task(task_id);
    REQUIRE(task.has_value());
    CHECK(task->type == task_type::verification);
}

// ============================================================================
// Task Execution Tests
// ============================================================================

TEST_CASE("task_scheduler: task execution", "[workflow][scheduler][execution]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;
    config.check_interval = std::chrono::seconds{1};

    task_scheduler scheduler(*db, config);

    std::atomic<int> execution_count{0};
    std::atomic<bool> task_completed{false};

    // Schedule a task that runs immediately
    auto execute_at = std::chrono::system_clock::now() - std::chrono::seconds{1};

    scheduler.schedule_once(
        "immediate_task",
        "Execute immediately",
        execute_at,
        [&execution_count, &task_completed]() -> std::optional<std::string> {
            ++execution_count;
            task_completed.store(true);
            return std::nullopt;
        });

    // Set up completion callback
    config.on_task_complete = [](const task_id& id, const task_execution_record& record) {
        // Callback invoked on completion
        (void)id;
        (void)record;
    };

    scheduler.start();

    // Wait for task to execute
    for (int i = 0; i < 50 && !task_completed.load(); ++i) {
        std::this_thread::sleep_for(20ms);
    }

    scheduler.stop();

    CHECK(task_completed.load() == true);
    CHECK(execution_count.load() == 1);
}

TEST_CASE("task_scheduler: task failure handling", "[workflow][scheduler][execution]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;
    config.check_interval = std::chrono::seconds{1};

    std::atomic<bool> error_callback_invoked{false};
    std::string captured_error;

    config.on_task_error = [&error_callback_invoked, &captured_error](
        const task_id& id, const std::string& error) {
        (void)id;
        captured_error = error;
        error_callback_invoked.store(true);
    };

    task_scheduler scheduler(*db, config);

    // Schedule a failing task
    auto execute_at = std::chrono::system_clock::now() - std::chrono::seconds{1};

    scheduler.schedule_once(
        "failing_task",
        "This task fails",
        execute_at,
        []() -> std::optional<std::string> {
            return "Task failed intentionally";
        });

    scheduler.start();

    // Wait for error callback
    for (int i = 0; i < 50 && !error_callback_invoked.load(); ++i) {
        std::this_thread::sleep_for(20ms);
    }

    scheduler.stop();

    CHECK(error_callback_invoked.load() == true);
    CHECK(captured_error == "Task failed intentionally");
}

// ============================================================================
// Execution History Tests
// ============================================================================

TEST_CASE("task_scheduler: execution history", "[workflow][scheduler][history]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;
    config.check_interval = std::chrono::seconds{1};

    task_scheduler scheduler(*db, config);

    std::atomic<bool> task_done{false};

    auto execute_at = std::chrono::system_clock::now() - std::chrono::seconds{1};
    auto task_id = scheduler.schedule_once(
        "history_test",
        "Test execution history",
        execute_at,
        [&task_done]() -> std::optional<std::string> {
            task_done.store(true);
            return std::nullopt;
        });

    scheduler.start();

    // Wait for task to execute
    for (int i = 0; i < 50 && !task_done.load(); ++i) {
        std::this_thread::sleep_for(20ms);
    }

    scheduler.stop();

    SECTION("get execution history for task") {
        auto history = scheduler.get_execution_history(task_id);
        REQUIRE(history.size() >= 1);
        CHECK(history.back().state == task_state::completed);
    }

    SECTION("get recent executions") {
        auto recent = scheduler.get_recent_executions(10);
        CHECK(recent.size() >= 1);
    }
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_CASE("task_scheduler: callbacks", "[workflow][scheduler][callbacks]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;

    task_scheduler scheduler(*db, config);

    SECTION("set task complete callback") {
        std::atomic<bool> called{false};

        scheduler.set_task_complete_callback(
            [&called](const task_id& id, const task_execution_record& record) {
                (void)id;
                (void)record;
                called.store(true);
            });

        // Callback would be invoked when task completes
        CHECK(called.load() == false);  // Not called yet
    }

    SECTION("set error callback") {
        std::atomic<bool> called{false};

        scheduler.set_error_callback(
            [&called](const task_id& id, const std::string& error) {
                (void)id;
                (void)error;
                called.store(true);
            });

        CHECK(called.load() == false);  // Not called yet
    }
}

// ============================================================================
// Concurrency Tests
// ============================================================================

TEST_CASE("task_scheduler: concurrent task scheduling", "[workflow][scheduler][concurrency]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;
    config.max_concurrent_tasks = 4;

    task_scheduler scheduler(*db, config);

    SECTION("multiple threads can schedule tasks simultaneously") {
        constexpr int num_threads = 8;
        constexpr int tasks_per_thread = 10;
        std::vector<std::thread> threads;
        std::atomic<int> scheduled_count{0};

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&scheduler, &scheduled_count, t]() {
                for (int i = 0; i < tasks_per_thread; ++i) {
                    std::string name = "thread_" + std::to_string(t) + "_task_" + std::to_string(i);
                    auto task_id = scheduler.schedule(
                        name, "Concurrent test task", 3600s,
                        []() { return std::optional<std::string>{}; });
                    if (!task_id.empty()) {
                        ++scheduled_count;
                    }
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        CHECK(scheduled_count.load() == num_threads * tasks_per_thread);

        auto tasks = scheduler.list_tasks();
        CHECK(tasks.size() == static_cast<std::size_t>(num_threads * tasks_per_thread));
    }
}

TEST_CASE("task_scheduler: concurrent task management", "[workflow][scheduler][concurrency]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;

    task_scheduler scheduler(*db, config);

    // Create tasks first
    std::vector<task_id> task_ids;
    for (int i = 0; i < 20; ++i) {
        auto id = scheduler.schedule(
            "task_" + std::to_string(i), "Test task", 3600s,
            []() { return std::optional<std::string>{}; });
        task_ids.push_back(id);
    }

    SECTION("concurrent pause and resume operations") {
        std::vector<std::thread> threads;
        std::atomic<int> operations{0};

        // Half the threads pause tasks
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&scheduler, &task_ids, &operations, t]() {
                for (std::size_t i = t; i < task_ids.size(); i += 4) {
                    scheduler.pause_task(task_ids[i]);
                    ++operations;
                    std::this_thread::sleep_for(1ms);
                    scheduler.resume_task(task_ids[i]);
                    ++operations;
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        // All operations should complete without crash
        CHECK(operations.load() > 0);

        // All tasks should be in pending state after resume
        auto tasks = scheduler.list_tasks();
        for (const auto& task : tasks) {
            CHECK((task.state == task_state::pending || task.state == task_state::paused));
        }
    }

    SECTION("concurrent list_tasks is thread-safe") {
        std::vector<std::thread> threads;
        std::atomic<int> list_count{0};

        for (int t = 0; t < 8; ++t) {
            threads.emplace_back([&scheduler, &list_count]() {
                for (int i = 0; i < 50; ++i) {
                    auto tasks = scheduler.list_tasks();
                    if (!tasks.empty()) {
                        ++list_count;
                    }
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        CHECK(list_count.load() == 8 * 50);
    }
}

TEST_CASE("task_scheduler: concurrent execution with max limit", "[workflow][scheduler][concurrency]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;
    config.check_interval = std::chrono::seconds{1};
    config.max_concurrent_tasks = 2;

    task_scheduler scheduler(*db, config);

    std::atomic<int> concurrent_running{0};
    std::atomic<int> max_concurrent{0};
    std::atomic<int> completed{0};
    constexpr int total_tasks = 6;

    // Schedule multiple tasks that track concurrency
    auto past_time = std::chrono::system_clock::now() - 1s;
    for (int i = 0; i < total_tasks; ++i) {
        scheduler.schedule_once(
            "concurrent_" + std::to_string(i),
            "Test max concurrency",
            past_time,
            [&concurrent_running, &max_concurrent, &completed]() -> std::optional<std::string> {
                ++concurrent_running;

                // Update max observed concurrency
                int current = concurrent_running.load();
                int expected = max_concurrent.load();
                while (current > expected) {
                    if (max_concurrent.compare_exchange_weak(expected, current)) {
                        break;
                    }
                }

                std::this_thread::sleep_for(50ms);
                --concurrent_running;
                ++completed;
                return std::nullopt;
            });
    }

    scheduler.start();

    // Wait for all tasks to complete
    for (int i = 0; i < 100 && completed.load() < total_tasks; ++i) {
        std::this_thread::sleep_for(50ms);
    }

    scheduler.stop();

    CHECK(completed.load() == total_tasks);
    CHECK(static_cast<std::size_t>(max_concurrent.load()) <= config.max_concurrent_tasks);
}

TEST_CASE("task_scheduler: stress test rapid scheduling", "[workflow][scheduler][concurrency][stress]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;

    task_scheduler scheduler(*db, config);

    SECTION("rapid schedule and cancel") {
        constexpr int iterations = 100;
        std::vector<task_id> ids;

        for (int i = 0; i < iterations; ++i) {
            auto id = scheduler.schedule(
                "rapid_" + std::to_string(i), "Rapid test", 3600s,
                []() { return std::optional<std::string>{}; });
            ids.push_back(id);
        }

        // Cancel half immediately
        for (int i = 0; i < iterations / 2; ++i) {
            scheduler.cancel_task(ids[i]);
        }

        auto all_tasks = scheduler.list_tasks();
        CHECK(all_tasks.size() == iterations);

        auto enabled_count = std::count_if(all_tasks.begin(), all_tasks.end(),
            [](const auto& t) { return t.enabled; });
        CHECK(enabled_count == iterations / 2);
    }
}

// ============================================================================
// Thread System Integration Tests
// ============================================================================

TEST_CASE("task_scheduler: statistics under concurrent load", "[workflow][scheduler][concurrency]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;
    config.check_interval = std::chrono::seconds{1};
    config.max_concurrent_tasks = 4;

    task_scheduler scheduler(*db, config);

    std::atomic<int> completed{0};
    constexpr int total_tasks = 10;

    // Schedule tasks that complete quickly
    auto past_time = std::chrono::system_clock::now() - 1s;
    for (int i = 0; i < total_tasks; ++i) {
        scheduler.schedule_once(
            "stats_test_" + std::to_string(i),
            "Statistics test",
            past_time,
            [&completed]() -> std::optional<std::string> {
                std::this_thread::sleep_for(10ms);
                ++completed;
                return std::nullopt;
            });
    }

    scheduler.start();

    // Wait for completion
    for (int i = 0; i < 100 && completed.load() < total_tasks; ++i) {
        std::this_thread::sleep_for(50ms);
    }

    scheduler.stop();

    auto stats = scheduler.get_stats();
    CHECK(stats.total_executions == total_tasks);
    CHECK(stats.successful_executions == total_tasks);
    CHECK(stats.failed_executions == 0);
    CHECK(stats.avg_execution_time.count() > 0);
}

TEST_CASE("task_scheduler: execution history under load", "[workflow][scheduler][concurrency]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;
    config.check_interval = std::chrono::seconds{1};

    task_scheduler scheduler(*db, config);

    std::atomic<int> completed{0};
    constexpr int total_tasks = 5;

    auto past_time = std::chrono::system_clock::now() - 1s;
    std::vector<task_id> task_ids;

    for (int i = 0; i < total_tasks; ++i) {
        auto id = scheduler.schedule_once(
            "history_load_" + std::to_string(i),
            "History load test",
            past_time,
            [&completed]() -> std::optional<std::string> {
                ++completed;
                return std::nullopt;
            });
        task_ids.push_back(id);
    }

    scheduler.start();

    for (int i = 0; i < 100 && completed.load() < total_tasks; ++i) {
        std::this_thread::sleep_for(50ms);
    }

    scheduler.stop();

    // Verify execution history for each task
    for (const auto& id : task_ids) {
        auto history = scheduler.get_execution_history(id);
        CHECK(history.size() >= 1);
        CHECK(history.back().state == task_state::completed);
    }

    // Verify recent executions
    auto recent = scheduler.get_recent_executions(total_tasks);
    CHECK(recent.size() == total_tasks);
}

TEST_CASE("task_scheduler: retry mechanism under concurrency", "[workflow][scheduler][concurrency]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;
    config.check_interval = std::chrono::seconds{1};

    task_scheduler scheduler(*db, config);

    std::atomic<int> attempt_count{0};
    constexpr int max_retries = 2;

    // Create task with retries
    scheduled_task task;
    task.id = "retry_test";
    task.name = "Retry Test";
    task.description = "Tests retry mechanism";
    task.type = task_type::custom;
    task.task_schedule = one_time_schedule{std::chrono::system_clock::now() - 1s};
    task.enabled = true;
    task.max_retries = max_retries;
    task.retry_delay = std::chrono::seconds{1};
    task.next_run_at = std::chrono::system_clock::now() - 1s;  // Ensure it runs immediately
    task.created_at = std::chrono::system_clock::now();
    task.updated_at = task.created_at;
    task.callback = [&attempt_count]() -> std::optional<std::string> {
        ++attempt_count;
        if (attempt_count.load() <= max_retries) {
            return "Intentional failure for retry test";
        }
        return std::nullopt;  // Success on final attempt
    };

    scheduler.schedule(std::move(task));
    scheduler.start();

    // Wait for retries to complete (longer wait for retry delays)
    for (int i = 0; i < 100 && attempt_count.load() < max_retries + 1; ++i) {
        std::this_thread::sleep_for(100ms);
    }

    scheduler.stop();

    // Should have attempted max_retries + 1 times (initial + retries)
    CHECK(attempt_count.load() == max_retries + 1);
}

TEST_CASE("task_scheduler: trigger_task under concurrent execution", "[workflow][scheduler][concurrency]") {
    auto db = create_test_database();
    task_scheduler_config config;
    config.auto_start = false;
    config.check_interval = std::chrono::seconds{1};

    task_scheduler scheduler(*db, config);

    std::atomic<int> execution_count{0};

    // Schedule a task with future execution time
    auto future_time = std::chrono::system_clock::now() + std::chrono::hours{24};
    auto task_id = scheduler.schedule_once(
        "trigger_test",
        "Trigger test task",
        future_time,
        [&execution_count]() -> std::optional<std::string> {
            ++execution_count;
            return std::nullopt;
        });

    scheduler.start();

    // Initially, task should not execute (scheduled for future)
    std::this_thread::sleep_for(200ms);
    CHECK(execution_count.load() == 0);

    // Trigger immediate execution
    CHECK(scheduler.trigger_task(task_id) == true);

    // Wait for triggered execution
    for (int i = 0; i < 50 && execution_count.load() == 0; ++i) {
        std::this_thread::sleep_for(50ms);
    }

    scheduler.stop();

    CHECK(execution_count.load() == 1);
}
