/**
 * @file prefetch_manager_test.cpp
 * @brief Unit tests for Prefetch Manager
 *
 * @see Issue #541 - Implement Prefetch Manager for Proactive Data Loading
 */

#include <pacs/client/prefetch_manager.hpp>
#include <pacs/client/prefetch_types.hpp>
#include <pacs/di/ilogger.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

using namespace pacs::client;

// =============================================================================
// Mock Logger for Testing
// =============================================================================

namespace {

class MockLogger final : public pacs::di::ILogger {
public:
    void trace(std::string_view) override {}
    void debug(std::string_view) override { debug_count_++; }
    void info(std::string_view message) override {
        info_count_++;
        last_info_ = std::string(message);
    }
    void warn(std::string_view message) override {
        warn_count_++;
        last_warn_ = std::string(message);
    }
    void error(std::string_view) override { error_count_++; }
    void fatal(std::string_view) override {}

    [[nodiscard]] bool is_enabled(pacs::integration::log_level) const noexcept override {
        return true;
    }

    [[nodiscard]] size_t debug_count() const noexcept { return debug_count_; }
    [[nodiscard]] size_t info_count() const noexcept { return info_count_; }
    [[nodiscard]] size_t warn_count() const noexcept { return warn_count_; }
    [[nodiscard]] size_t error_count() const noexcept { return error_count_; }
    [[nodiscard]] const std::string& last_info() const noexcept { return last_info_; }
    [[nodiscard]] const std::string& last_warn() const noexcept { return last_warn_; }

    void reset() {
        debug_count_ = 0;
        info_count_ = 0;
        warn_count_ = 0;
        error_count_ = 0;
        last_info_.clear();
        last_warn_.clear();
    }

private:
    size_t debug_count_{0};
    size_t info_count_{0};
    size_t warn_count_{0};
    size_t error_count_{0};
    std::string last_info_;
    std::string last_warn_;
};

}  // namespace

// =============================================================================
// Prefetch Types Tests
// =============================================================================

TEST_CASE("prefetch_trigger conversion", "[prefetch_types]") {
    SECTION("to_string conversion") {
        CHECK(std::string_view(to_string(prefetch_trigger::worklist_match)) == "worklist_match");
        CHECK(std::string_view(to_string(prefetch_trigger::prior_studies)) == "prior_studies");
        CHECK(std::string_view(to_string(prefetch_trigger::scheduled_exam)) == "scheduled_exam");
        CHECK(std::string_view(to_string(prefetch_trigger::manual)) == "manual");
    }

    SECTION("from_string conversion") {
        CHECK(prefetch_trigger_from_string("worklist_match") == prefetch_trigger::worklist_match);
        CHECK(prefetch_trigger_from_string("prior_studies") == prefetch_trigger::prior_studies);
        CHECK(prefetch_trigger_from_string("scheduled_exam") == prefetch_trigger::scheduled_exam);
        CHECK(prefetch_trigger_from_string("manual") == prefetch_trigger::manual);
        CHECK(prefetch_trigger_from_string("invalid") == prefetch_trigger::manual);  // Default
    }
}

TEST_CASE("prefetch_rule default values", "[prefetch_types]") {
    prefetch_rule rule;

    SECTION("initial state") {
        CHECK(rule.rule_id.empty());
        CHECK(rule.name.empty());
        CHECK(rule.enabled == true);
        CHECK(rule.modality_filter.empty());
        CHECK(rule.body_part_filter.empty());
        CHECK(rule.station_ae_filter.empty());
        CHECK(rule.prior_lookback == std::chrono::hours{8760});  // 1 year
        CHECK(rule.max_prior_studies == 3);
        CHECK(rule.prior_modalities.empty());
        CHECK(rule.source_node_ids.empty());
        CHECK(rule.schedule_cron.empty());
        CHECK(rule.advance_time == std::chrono::minutes{60});
        CHECK(rule.triggered_count == 0);
        CHECK(rule.studies_prefetched == 0);
        CHECK(rule.pk == 0);
    }
}

TEST_CASE("prefetch_result success check", "[prefetch_types]") {
    prefetch_result result;

    SECTION("empty result is not successful") {
        CHECK_FALSE(result.is_success());
    }

    SECTION("result with prefetched studies is successful") {
        result.studies_prefetched = 1;
        CHECK(result.is_success());
    }

    SECTION("result with already local studies is successful") {
        result.studies_already_local = 1;
        CHECK(result.is_success());
    }

    SECTION("result with both is successful") {
        result.studies_prefetched = 2;
        result.studies_already_local = 3;
        CHECK(result.is_success());
    }
}

TEST_CASE("prefetch_manager_config default values", "[prefetch_types]") {
    prefetch_manager_config config;

    CHECK(config.enabled == true);
    CHECK(config.worklist_check_interval == std::chrono::seconds{300});
    CHECK(config.max_concurrent_prefetch == 4);
    CHECK(config.deduplicate_requests == true);
}

// =============================================================================
// Prefetch Manager Construction Tests
// =============================================================================

TEST_CASE("prefetch_manager construction", "[prefetch_manager]") {
    auto logger = std::make_shared<MockLogger>();

    SECTION("construction with nullptr dependencies") {
        // Should not crash with nullptr dependencies
        REQUIRE_NOTHROW(
            prefetch_manager(nullptr, nullptr, nullptr, nullptr, logger)
        );
    }

    SECTION("construction with custom config") {
        prefetch_manager_config config;
        config.enabled = false;
        config.worklist_check_interval = std::chrono::seconds{60};
        config.max_concurrent_prefetch = 2;

        auto manager = std::make_unique<prefetch_manager>(
            config, nullptr, nullptr, nullptr, nullptr, logger);

        CHECK_FALSE(manager->config().enabled);
        CHECK(manager->config().worklist_check_interval == std::chrono::seconds{60});
        CHECK(manager->config().max_concurrent_prefetch == 2);
    }
}

// =============================================================================
// Rule Management Tests (without repository)
// =============================================================================

TEST_CASE("prefetch_manager rule management without repository", "[prefetch_manager]") {
    auto logger = std::make_shared<MockLogger>();
    auto manager = std::make_unique<prefetch_manager>(
        nullptr, nullptr, nullptr, nullptr, logger);

    SECTION("add rule") {
        prefetch_rule rule;
        rule.name = "Test Rule";
        rule.trigger = prefetch_trigger::prior_studies;
        rule.source_node_ids = {"node1"};

        auto result = manager->add_rule(rule);
        CHECK(result.is_ok());

        auto rules = manager->list_rules();
        CHECK(rules.size() == 1);
        CHECK(rules[0].name == "Test Rule");
        CHECK_FALSE(rules[0].rule_id.empty());  // Should generate UUID
    }

    SECTION("add rule with existing ID") {
        prefetch_rule rule;
        rule.rule_id = "existing-rule-id";
        rule.name = "Test Rule";
        rule.trigger = prefetch_trigger::manual;

        auto result = manager->add_rule(rule);
        CHECK(result.is_ok());

        auto rules = manager->list_rules();
        CHECK(rules.size() == 1);
        CHECK(rules[0].rule_id == "existing-rule-id");
    }

    SECTION("update rule") {
        prefetch_rule rule;
        rule.rule_id = "rule-to-update";
        rule.name = "Original Name";
        rule.trigger = prefetch_trigger::prior_studies;

        manager->add_rule(rule);

        rule.name = "Updated Name";
        auto result = manager->update_rule(rule);
        CHECK(result.is_ok());

        auto updated = manager->get_rule("rule-to-update");
        REQUIRE(updated.has_value());
        CHECK(updated->name == "Updated Name");
    }

    SECTION("update rule without ID fails") {
        prefetch_rule rule;
        rule.name = "No ID Rule";

        auto result = manager->update_rule(rule);
        CHECK(result.is_err());
    }

    SECTION("remove rule") {
        prefetch_rule rule;
        rule.rule_id = "rule-to-remove";
        rule.name = "To Remove";
        rule.trigger = prefetch_trigger::manual;

        manager->add_rule(rule);
        CHECK(manager->list_rules().size() == 1);

        auto result = manager->remove_rule("rule-to-remove");
        CHECK(result.is_ok());
        CHECK(manager->list_rules().empty());
    }

    SECTION("get non-existent rule") {
        auto rule = manager->get_rule("non-existent");
        CHECK_FALSE(rule.has_value());
    }
}

// =============================================================================
// Scheduler Control Tests
// =============================================================================

TEST_CASE("prefetch_manager scheduler control", "[prefetch_manager]") {
    auto logger = std::make_shared<MockLogger>();
    auto manager = std::make_unique<prefetch_manager>(
        nullptr, nullptr, nullptr, nullptr, logger);

    SECTION("initial state") {
        CHECK_FALSE(manager->is_scheduler_running());
    }

    SECTION("start and stop scheduler") {
        manager->start_scheduler();
        CHECK(manager->is_scheduler_running());

        manager->stop_scheduler();
        CHECK_FALSE(manager->is_scheduler_running());
    }

    SECTION("multiple start calls are safe") {
        manager->start_scheduler();
        manager->start_scheduler();  // Should be no-op
        CHECK(manager->is_scheduler_running());

        manager->stop_scheduler();
        CHECK_FALSE(manager->is_scheduler_running());
    }

    SECTION("multiple stop calls are safe") {
        manager->stop_scheduler();  // Not running
        manager->stop_scheduler();  // Still not running
        CHECK_FALSE(manager->is_scheduler_running());
    }
}

// =============================================================================
// Worklist Monitor Control Tests
// =============================================================================

TEST_CASE("prefetch_manager worklist monitor control", "[prefetch_manager]") {
    auto logger = std::make_shared<MockLogger>();
    auto manager = std::make_unique<prefetch_manager>(
        nullptr, nullptr, nullptr, nullptr, logger);

    SECTION("initial state") {
        CHECK_FALSE(manager->is_worklist_monitor_running());
    }

    SECTION("start and stop worklist monitor") {
        manager->start_worklist_monitor("test-node");
        CHECK(manager->is_worklist_monitor_running());

        manager->stop_worklist_monitor();
        CHECK_FALSE(manager->is_worklist_monitor_running());
    }

    SECTION("multiple start calls are safe") {
        manager->start_worklist_monitor("test-node");
        manager->start_worklist_monitor("other-node");  // Should be no-op
        CHECK(manager->is_worklist_monitor_running());

        manager->stop_worklist_monitor();
    }
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_CASE("prefetch_manager statistics", "[prefetch_manager]") {
    auto logger = std::make_shared<MockLogger>();
    auto manager = std::make_unique<prefetch_manager>(
        nullptr, nullptr, nullptr, nullptr, logger);

    SECTION("initial statistics") {
        CHECK(manager->pending_prefetches() == 0);
        CHECK(manager->completed_today() == 0);  // No repository
        CHECK(manager->failed_today() == 0);     // No repository
    }

    SECTION("get statistics for non-existent rule") {
        auto stats = manager->get_rule_statistics("non-existent");
        CHECK(stats.triggered_count == 0);
        CHECK(stats.studies_prefetched == 0);
        CHECK(stats.bytes_prefetched == 0);
    }

    SECTION("get statistics for existing rule") {
        prefetch_rule rule;
        rule.rule_id = "stats-rule";
        rule.name = "Stats Test";
        rule.trigger = prefetch_trigger::manual;
        rule.triggered_count = 10;
        rule.studies_prefetched = 50;

        manager->add_rule(rule);

        auto stats = manager->get_rule_statistics("stats-rule");
        CHECK(stats.triggered_count == 10);
        CHECK(stats.studies_prefetched == 50);
    }
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_CASE("prefetch_manager configuration", "[prefetch_manager]") {
    auto logger = std::make_shared<MockLogger>();
    auto manager = std::make_unique<prefetch_manager>(
        nullptr, nullptr, nullptr, nullptr, logger);

    SECTION("get default config") {
        const auto& config = manager->config();
        CHECK(config.enabled == true);
        CHECK(config.deduplicate_requests == true);
    }

    SECTION("update config") {
        prefetch_manager_config new_config;
        new_config.enabled = false;
        new_config.max_concurrent_prefetch = 8;

        manager->set_config(new_config);

        CHECK_FALSE(manager->config().enabled);
        CHECK(manager->config().max_concurrent_prefetch == 8);
    }
}

// =============================================================================
// Prior Study Prefetch Tests
// =============================================================================

TEST_CASE("prefetch_manager prior study prefetch", "[prefetch_manager]") {
    auto logger = std::make_shared<MockLogger>();
    auto manager = std::make_unique<prefetch_manager>(
        nullptr, nullptr, nullptr, nullptr, logger);

    SECTION("prefetch priors with no matching rules") {
        auto result = manager->prefetch_priors("PATIENT123", "CT");

        CHECK(result.patient_id == "PATIENT123");
        CHECK(result.studies_prefetched == 0);
        CHECK(result.studies_found == 0);
    }

    SECTION("prefetch priors async") {
        auto future = manager->prefetch_priors_async("PATIENT456", "MR", "BRAIN");

        auto result = future.get();
        CHECK(result.patient_id == "PATIENT456");
    }
}

// =============================================================================
// Manual Prefetch Tests
// =============================================================================

TEST_CASE("prefetch_manager manual prefetch", "[prefetch_manager]") {
    auto logger = std::make_shared<MockLogger>();
    auto manager = std::make_unique<prefetch_manager>(
        nullptr, nullptr, nullptr, nullptr, logger);

    SECTION("prefetch study without job manager returns empty") {
        auto job_id = manager->prefetch_study("node1", "1.2.3.4.5.6.7.8.9");
        CHECK(job_id.empty());  // No job_manager
    }

    SECTION("prefetch patient without job manager returns empty") {
        auto job_id = manager->prefetch_patient("node1", "PATIENT123");
        CHECK(job_id.empty());  // No job_manager
    }
}

// =============================================================================
// Destructor Tests
// =============================================================================

TEST_CASE("prefetch_manager cleanup on destruction", "[prefetch_manager]") {
    auto logger = std::make_shared<MockLogger>();

    SECTION("destructor stops running threads") {
        auto manager = std::make_unique<prefetch_manager>(
            nullptr, nullptr, nullptr, nullptr, logger);

        manager->start_scheduler();
        manager->start_worklist_monitor("test-node");

        CHECK(manager->is_scheduler_running());
        CHECK(manager->is_worklist_monitor_running());

        // Destructor should stop threads
        manager.reset();

        // If we get here without hanging, threads were stopped properly
        CHECK(true);
    }
}
