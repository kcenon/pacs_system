/**
 * @file job_manager_test.cpp
 * @brief Unit tests for Job Manager
 *
 * @see Issue #537 - Implement Job Manager for Async DICOM Operations
 * @see Issue #554 - Part 3: Job Execution and Tests
 */

#include <pacs/client/job_manager.hpp>
#include <pacs/client/job_types.hpp>
#include <pacs/client/remote_node_manager.hpp>
#include <pacs/storage/job_repository.hpp>
#include <pacs/di/ilogger.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace pacs::client;

// =============================================================================
// Mock Logger for Testing
// =============================================================================

namespace {

class MockLogger final : public pacs::di::ILogger {
public:
    void trace(std::string_view) override {}
    void debug(std::string_view) override {}
    void info(std::string_view message) override {
        info_count_.fetch_add(1, std::memory_order_relaxed);
        last_info_message_ = std::string(message);
    }
    void warn(std::string_view) override {}
    void error(std::string_view message) override {
        error_count_.fetch_add(1, std::memory_order_relaxed);
        last_error_message_ = std::string(message);
    }
    void fatal(std::string_view) override {}

    [[nodiscard]] bool is_enabled(pacs::integration::log_level) const noexcept override {
        return true;
    }

    [[nodiscard]] size_t info_count() const noexcept {
        return info_count_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] size_t error_count() const noexcept {
        return error_count_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] const std::string& last_info() const noexcept {
        return last_info_message_;
    }

    void reset() {
        info_count_.store(0);
        error_count_.store(0);
        last_info_message_.clear();
        last_error_message_.clear();
    }

private:
    std::atomic<size_t> info_count_{0};
    std::atomic<size_t> error_count_{0};
    std::string last_info_message_;
    std::string last_error_message_;
};

}  // namespace

// =============================================================================
// Job Types Tests
// =============================================================================

TEST_CASE("job_type conversion", "[job_types]") {
    SECTION("to_string conversion") {
        CHECK(std::string_view(to_string(job_type::query)) == "query");
        CHECK(std::string_view(to_string(job_type::retrieve)) == "retrieve");
        CHECK(std::string_view(to_string(job_type::store)) == "store");
        CHECK(std::string_view(to_string(job_type::export_)) == "export");
        CHECK(std::string_view(to_string(job_type::import_)) == "import");
        CHECK(std::string_view(to_string(job_type::prefetch)) == "prefetch");
        CHECK(std::string_view(to_string(job_type::sync)) == "sync");
    }

    SECTION("from_string conversion") {
        CHECK(job_type_from_string("query") == job_type::query);
        CHECK(job_type_from_string("retrieve") == job_type::retrieve);
        CHECK(job_type_from_string("store") == job_type::store);
        CHECK(job_type_from_string("export") == job_type::export_);
        CHECK(job_type_from_string("import") == job_type::import_);
        CHECK(job_type_from_string("prefetch") == job_type::prefetch);
        CHECK(job_type_from_string("sync") == job_type::sync);
        CHECK(job_type_from_string("invalid") == job_type::query);  // Default
    }
}

TEST_CASE("job_status conversion", "[job_types]") {
    SECTION("to_string conversion") {
        CHECK(std::string_view(to_string(job_status::pending)) == "pending");
        CHECK(std::string_view(to_string(job_status::queued)) == "queued");
        CHECK(std::string_view(to_string(job_status::running)) == "running");
        CHECK(std::string_view(to_string(job_status::completed)) == "completed");
        CHECK(std::string_view(to_string(job_status::failed)) == "failed");
        CHECK(std::string_view(to_string(job_status::cancelled)) == "cancelled");
        CHECK(std::string_view(to_string(job_status::paused)) == "paused");
    }

    SECTION("from_string conversion") {
        CHECK(job_status_from_string("pending") == job_status::pending);
        CHECK(job_status_from_string("queued") == job_status::queued);
        CHECK(job_status_from_string("running") == job_status::running);
        CHECK(job_status_from_string("completed") == job_status::completed);
        CHECK(job_status_from_string("failed") == job_status::failed);
        CHECK(job_status_from_string("cancelled") == job_status::cancelled);
        CHECK(job_status_from_string("paused") == job_status::paused);
        CHECK(job_status_from_string("unknown") == job_status::pending);  // Default
    }

    SECTION("is_terminal_status") {
        CHECK_FALSE(is_terminal_status(job_status::pending));
        CHECK_FALSE(is_terminal_status(job_status::queued));
        CHECK_FALSE(is_terminal_status(job_status::running));
        CHECK(is_terminal_status(job_status::completed));
        CHECK(is_terminal_status(job_status::failed));
        CHECK(is_terminal_status(job_status::cancelled));
        CHECK_FALSE(is_terminal_status(job_status::paused));
    }
}

TEST_CASE("job_priority conversion", "[job_types]") {
    SECTION("to_string conversion") {
        CHECK(std::string_view(to_string(job_priority::low)) == "low");
        CHECK(std::string_view(to_string(job_priority::normal)) == "normal");
        CHECK(std::string_view(to_string(job_priority::high)) == "high");
        CHECK(std::string_view(to_string(job_priority::urgent)) == "urgent");
    }

    SECTION("from_string conversion") {
        CHECK(job_priority_from_string("low") == job_priority::low);
        CHECK(job_priority_from_string("normal") == job_priority::normal);
        CHECK(job_priority_from_string("high") == job_priority::high);
        CHECK(job_priority_from_string("urgent") == job_priority::urgent);
        CHECK(job_priority_from_string("invalid") == job_priority::normal);  // Default
    }

    SECTION("from_int conversion") {
        CHECK(job_priority_from_int(0) == job_priority::low);
        CHECK(job_priority_from_int(1) == job_priority::normal);
        CHECK(job_priority_from_int(2) == job_priority::high);
        CHECK(job_priority_from_int(3) == job_priority::urgent);
        CHECK(job_priority_from_int(-1) == job_priority::low);
        CHECK(job_priority_from_int(100) == job_priority::urgent);
    }
}

// =============================================================================
// Job Progress Tests
// =============================================================================

TEST_CASE("job_progress calculation", "[job_types]") {
    job_progress progress;

    SECTION("initial state") {
        CHECK(progress.total_items == 0);
        CHECK(progress.completed_items == 0);
        CHECK(progress.failed_items == 0);
        CHECK(progress.skipped_items == 0);
        CHECK(progress.percent_complete == 0.0f);
    }

    SECTION("calculate_percent with items") {
        progress.total_items = 100;
        progress.completed_items = 25;
        progress.failed_items = 5;
        progress.skipped_items = 10;
        progress.calculate_percent();

        CHECK(progress.percent_complete == 40.0f);
    }

    SECTION("calculate_percent with zero total") {
        progress.total_items = 0;
        progress.completed_items = 0;
        progress.calculate_percent();

        CHECK(progress.percent_complete == 0.0f);
    }

    SECTION("is_complete") {
        progress.total_items = 10;
        progress.completed_items = 5;
        CHECK_FALSE(progress.is_complete());

        progress.completed_items = 10;
        CHECK(progress.is_complete());

        // Complete can include failed/skipped
        progress.completed_items = 7;
        progress.failed_items = 2;
        progress.skipped_items = 1;
        CHECK(progress.is_complete());
    }
}

// =============================================================================
// Job Record Tests
// =============================================================================

TEST_CASE("job_record state checks", "[job_types]") {
    job_record job;
    job.job_id = "test-job-1";
    job.type = job_type::retrieve;

    SECTION("is_finished") {
        job.status = job_status::pending;
        CHECK_FALSE(job.is_finished());

        job.status = job_status::running;
        CHECK_FALSE(job.is_finished());

        job.status = job_status::completed;
        CHECK(job.is_finished());

        job.status = job_status::failed;
        CHECK(job.is_finished());

        job.status = job_status::cancelled;
        CHECK(job.is_finished());
    }

    SECTION("can_start") {
        job.status = job_status::pending;
        CHECK(job.can_start());

        job.status = job_status::queued;
        CHECK(job.can_start());

        job.status = job_status::paused;
        CHECK(job.can_start());

        job.status = job_status::running;
        CHECK_FALSE(job.can_start());

        job.status = job_status::completed;
        CHECK_FALSE(job.can_start());
    }

    SECTION("can_cancel") {
        job.status = job_status::pending;
        CHECK(job.can_cancel());

        job.status = job_status::running;
        CHECK(job.can_cancel());

        job.status = job_status::completed;
        CHECK_FALSE(job.can_cancel());

        job.status = job_status::cancelled;
        CHECK_FALSE(job.can_cancel());
    }

    SECTION("can_pause") {
        job.status = job_status::running;
        CHECK(job.can_pause());

        job.status = job_status::queued;
        CHECK(job.can_pause());

        job.status = job_status::pending;
        CHECK_FALSE(job.can_pause());

        job.status = job_status::completed;
        CHECK_FALSE(job.can_pause());
    }

    SECTION("can_retry") {
        job.status = job_status::failed;
        job.retry_count = 0;
        job.max_retries = 3;
        CHECK(job.can_retry());

        job.retry_count = 3;
        CHECK_FALSE(job.can_retry());

        job.status = job_status::completed;
        job.retry_count = 0;
        CHECK_FALSE(job.can_retry());
    }

    SECTION("duration calculation") {
        CHECK(job.duration() == std::chrono::milliseconds{0});

        job.started_at = std::chrono::system_clock::now() - std::chrono::seconds(5);
        auto d = job.duration();
        CHECK(d >= std::chrono::seconds(4));
        CHECK(d <= std::chrono::seconds(6));

        job.completed_at = *job.started_at + std::chrono::seconds(2);
        d = job.duration();
        CHECK(d >= std::chrono::milliseconds(1900));
        CHECK(d <= std::chrono::milliseconds(2100));
    }
}

// =============================================================================
// Job Manager Config Tests
// =============================================================================

TEST_CASE("job_manager_config defaults", "[job_manager]") {
    job_manager_config config;

    CHECK(config.worker_count == 4);
    CHECK(config.max_queue_size == 1000);
    CHECK(config.job_timeout == std::chrono::seconds{3600});
    CHECK(config.persist_jobs == true);
    CHECK(config.auto_retry_failed == true);
    CHECK(config.retry_delay == std::chrono::seconds{60});
    CHECK(config.local_ae_title == "PACS_CLIENT");
}

// =============================================================================
// Priority Queue Ordering Tests
// =============================================================================

TEST_CASE("job priority ordering", "[job_manager]") {
    SECTION("higher priority comes first") {
        CHECK(static_cast<int>(job_priority::urgent) > static_cast<int>(job_priority::high));
        CHECK(static_cast<int>(job_priority::high) > static_cast<int>(job_priority::normal));
        CHECK(static_cast<int>(job_priority::normal) > static_cast<int>(job_priority::low));
    }
}

// =============================================================================
// Callback Tests
// =============================================================================

TEST_CASE("callback types are callable", "[job_types]") {
    SECTION("progress_callback") {
        bool called = false;
        job_progress_callback callback = [&called](const std::string& id, const job_progress& p) {
            called = true;
            CHECK_FALSE(id.empty());
            CHECK(p.total_items >= 0);
        };

        job_progress progress;
        progress.total_items = 10;
        callback("test-job", progress);
        CHECK(called);
    }

    SECTION("completion_callback") {
        bool called = false;
        job_completion_callback callback = [&called](const std::string& id, const job_record& r) {
            called = true;
            CHECK_FALSE(id.empty());
            CHECK(r.is_finished());
        };

        job_record job;
        job.job_id = "test-job";
        job.status = job_status::completed;
        callback("test-job", job);
        CHECK(called);
    }
}
