/**
 * @file sync_manager_test.cpp
 * @brief Unit tests for Sync Manager
 *
 * @see Issue #542 - Implement Sync Manager for Bidirectional Synchronization
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 */

#include <pacs/client/sync_manager.hpp>
#include <pacs/client/sync_types.hpp>
#include <pacs/di/ilogger.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
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
// Sync Direction Tests
// =============================================================================

TEST_CASE("sync_direction conversion", "[sync_types]") {
    SECTION("to_string conversion") {
        CHECK(std::string_view(to_string(sync_direction::pull)) == "pull");
        CHECK(std::string_view(to_string(sync_direction::push)) == "push");
        CHECK(std::string_view(to_string(sync_direction::bidirectional)) == "bidirectional");
    }

    SECTION("from_string conversion") {
        CHECK(sync_direction_from_string("pull") == sync_direction::pull);
        CHECK(sync_direction_from_string("push") == sync_direction::push);
        CHECK(sync_direction_from_string("bidirectional") == sync_direction::bidirectional);
        CHECK(sync_direction_from_string("invalid") == sync_direction::pull);  // Default
    }
}

// =============================================================================
// Sync Conflict Type Tests
// =============================================================================

TEST_CASE("sync_conflict_type conversion", "[sync_types]") {
    SECTION("to_string conversion") {
        CHECK(std::string_view(to_string(sync_conflict_type::missing_local)) == "missing_local");
        CHECK(std::string_view(to_string(sync_conflict_type::missing_remote)) == "missing_remote");
        CHECK(std::string_view(to_string(sync_conflict_type::modified)) == "modified");
        CHECK(std::string_view(to_string(sync_conflict_type::count_mismatch)) == "count_mismatch");
    }

    SECTION("from_string conversion") {
        CHECK(sync_conflict_type_from_string("missing_local") == sync_conflict_type::missing_local);
        CHECK(sync_conflict_type_from_string("missing_remote") == sync_conflict_type::missing_remote);
        CHECK(sync_conflict_type_from_string("modified") == sync_conflict_type::modified);
        CHECK(sync_conflict_type_from_string("count_mismatch") == sync_conflict_type::count_mismatch);
        CHECK(sync_conflict_type_from_string("invalid") == sync_conflict_type::missing_local);  // Default
    }
}

// =============================================================================
// Conflict Resolution Tests
// =============================================================================

TEST_CASE("conflict_resolution conversion", "[sync_types]") {
    SECTION("to_string conversion") {
        CHECK(std::string_view(to_string(conflict_resolution::prefer_local)) == "prefer_local");
        CHECK(std::string_view(to_string(conflict_resolution::prefer_remote)) == "prefer_remote");
        CHECK(std::string_view(to_string(conflict_resolution::prefer_newer)) == "prefer_newer");
    }

    SECTION("from_string conversion") {
        CHECK(conflict_resolution_from_string("prefer_local") == conflict_resolution::prefer_local);
        CHECK(conflict_resolution_from_string("prefer_remote") == conflict_resolution::prefer_remote);
        CHECK(conflict_resolution_from_string("prefer_newer") == conflict_resolution::prefer_newer);
        CHECK(conflict_resolution_from_string("invalid") == conflict_resolution::prefer_remote);  // Default
    }
}

// =============================================================================
// Sync Config Tests
// =============================================================================

TEST_CASE("sync_config default values", "[sync_types]") {
    sync_config config;

    CHECK(config.config_id.empty());
    CHECK(config.source_node_id.empty());
    CHECK(config.name.empty());
    CHECK(config.enabled == true);
    CHECK(config.lookback == std::chrono::hours(24));
    CHECK(config.modalities.empty());
    CHECK(config.patient_id_patterns.empty());
    CHECK(config.direction == sync_direction::pull);
    CHECK(config.delete_missing == false);
    CHECK(config.overwrite_existing == false);
    CHECK(config.sync_metadata_only == false);
    CHECK(config.schedule_cron.empty());
    CHECK(config.total_syncs == 0);
    CHECK(config.studies_synced == 0);
    CHECK(config.pk == 0);
}

TEST_CASE("sync_config initialization", "[sync_types]") {
    sync_config config;
    config.config_id = "daily-sync";
    config.source_node_id = "archive-server";
    config.name = "Daily Sync with Archive";
    config.direction = sync_direction::bidirectional;
    config.lookback = std::chrono::hours(48);
    config.schedule_cron = "0 2 * * *";
    config.modalities = {"CT", "MR"};

    CHECK(config.config_id == "daily-sync");
    CHECK(config.source_node_id == "archive-server");
    CHECK(config.name == "Daily Sync with Archive");
    CHECK(config.direction == sync_direction::bidirectional);
    CHECK(config.lookback == std::chrono::hours(48));
    CHECK(config.schedule_cron == "0 2 * * *");
    CHECK(config.modalities.size() == 2);
    CHECK(config.modalities[0] == "CT");
    CHECK(config.modalities[1] == "MR");
}

// =============================================================================
// Sync Conflict Tests
// =============================================================================

TEST_CASE("sync_conflict default values", "[sync_types]") {
    sync_conflict conflict;

    CHECK(conflict.config_id.empty());
    CHECK(conflict.study_uid.empty());
    CHECK(conflict.patient_id.empty());
    CHECK(conflict.local_instance_count == 0);
    CHECK(conflict.remote_instance_count == 0);
    CHECK(conflict.resolved == false);
    CHECK(conflict.pk == 0);
}

TEST_CASE("sync_conflict initialization", "[sync_types]") {
    sync_conflict conflict;
    conflict.config_id = "daily-sync";
    conflict.study_uid = "1.2.3.4.5.6.7.8.9";
    conflict.patient_id = "PATIENT001";
    conflict.conflict_type = sync_conflict_type::count_mismatch;
    conflict.local_instance_count = 100;
    conflict.remote_instance_count = 105;
    conflict.detected_at = std::chrono::system_clock::now();

    CHECK(conflict.config_id == "daily-sync");
    CHECK(conflict.study_uid == "1.2.3.4.5.6.7.8.9");
    CHECK(conflict.patient_id == "PATIENT001");
    CHECK(conflict.conflict_type == sync_conflict_type::count_mismatch);
    CHECK(conflict.local_instance_count == 100);
    CHECK(conflict.remote_instance_count == 105);
    CHECK(conflict.resolved == false);
}

// =============================================================================
// Sync Result Tests
// =============================================================================

TEST_CASE("sync_result default values", "[sync_types]") {
    sync_result result;

    CHECK(result.config_id.empty());
    CHECK(result.job_id.empty());
    CHECK(result.success == false);
    CHECK(result.studies_checked == 0);
    CHECK(result.studies_synced == 0);
    CHECK(result.studies_skipped == 0);
    CHECK(result.instances_transferred == 0);
    CHECK(result.bytes_transferred == 0);
    CHECK(result.conflicts.empty());
    CHECK(result.errors.empty());
    CHECK(result.elapsed == std::chrono::milliseconds(0));
}

TEST_CASE("sync_result initialization", "[sync_types]") {
    sync_result result;
    result.config_id = "daily-sync";
    result.job_id = "abc123";
    result.success = true;
    result.studies_checked = 100;
    result.studies_synced = 50;
    result.studies_skipped = 45;
    result.instances_transferred = 500;
    result.bytes_transferred = 1024 * 1024 * 100;  // 100 MB

    CHECK(result.config_id == "daily-sync");
    CHECK(result.job_id == "abc123");
    CHECK(result.success == true);
    CHECK(result.studies_checked == 100);
    CHECK(result.studies_synced == 50);
    CHECK(result.studies_skipped == 45);
    CHECK(result.instances_transferred == 500);
    CHECK(result.bytes_transferred == 100 * 1024 * 1024);
}

// =============================================================================
// Sync Manager Config Tests
// =============================================================================

TEST_CASE("sync_manager_config default values", "[sync_types]") {
    sync_manager_config config;

    CHECK(config.max_concurrent_syncs == 2);
    CHECK(config.comparison_timeout == std::chrono::seconds(300));
    CHECK(config.auto_resolve_conflicts == false);
    CHECK(config.default_resolution == conflict_resolution::prefer_remote);
}

TEST_CASE("sync_manager_config initialization", "[sync_types]") {
    sync_manager_config config;
    config.max_concurrent_syncs = 4;
    config.comparison_timeout = std::chrono::seconds(600);
    config.auto_resolve_conflicts = true;
    config.default_resolution = conflict_resolution::prefer_newer;

    CHECK(config.max_concurrent_syncs == 4);
    CHECK(config.comparison_timeout == std::chrono::seconds(600));
    CHECK(config.auto_resolve_conflicts == true);
    CHECK(config.default_resolution == conflict_resolution::prefer_newer);
}

// =============================================================================
// Sync Statistics Tests
// =============================================================================

TEST_CASE("sync_statistics default values", "[sync_types]") {
    sync_statistics stats;

    CHECK(stats.total_syncs == 0);
    CHECK(stats.successful_syncs == 0);
    CHECK(stats.failed_syncs == 0);
    CHECK(stats.total_studies_synced == 0);
    CHECK(stats.total_bytes_transferred == 0);
    CHECK(stats.total_conflicts_detected == 0);
    CHECK(stats.total_conflicts_resolved == 0);
}

// =============================================================================
// Sync History Tests
// =============================================================================

TEST_CASE("sync_history default values", "[sync_types]") {
    sync_history history;

    CHECK(history.config_id.empty());
    CHECK(history.job_id.empty());
    CHECK(history.success == false);
    CHECK(history.studies_checked == 0);
    CHECK(history.studies_synced == 0);
    CHECK(history.conflicts_found == 0);
    CHECK(history.errors.empty());
    CHECK(history.pk == 0);
}

TEST_CASE("sync_history initialization", "[sync_types]") {
    sync_history history;
    history.config_id = "daily-sync";
    history.job_id = "job-123";
    history.success = true;
    history.studies_checked = 100;
    history.studies_synced = 50;
    history.conflicts_found = 5;
    history.started_at = std::chrono::system_clock::now();
    history.completed_at = std::chrono::system_clock::now();

    CHECK(history.config_id == "daily-sync");
    CHECK(history.job_id == "job-123");
    CHECK(history.success == true);
    CHECK(history.studies_checked == 100);
    CHECK(history.studies_synced == 50);
    CHECK(history.conflicts_found == 5);
}
