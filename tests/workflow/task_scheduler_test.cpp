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
