/**
 * @file routing_manager_test.cpp
 * @brief Unit tests for Routing Manager
 *
 * @see Issue #539 - Implement Routing Manager for Auto-Forwarding
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 */

#include <pacs/client/routing_manager.hpp>
#include <pacs/client/routing_types.hpp>
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag_constants.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace pacs::client;

// =============================================================================
// Routing Types Tests
// =============================================================================

TEST_CASE("routing_field conversion", "[routing_types]") {
    SECTION("to_string conversion") {
        CHECK(std::string_view(to_string(routing_field::modality)) == "modality");
        CHECK(std::string_view(to_string(routing_field::station_ae)) == "station_ae");
        CHECK(std::string_view(to_string(routing_field::institution)) == "institution");
        CHECK(std::string_view(to_string(routing_field::department)) == "department");
        CHECK(std::string_view(to_string(routing_field::referring_physician)) == "referring_physician");
        CHECK(std::string_view(to_string(routing_field::study_description)) == "study_description");
        CHECK(std::string_view(to_string(routing_field::series_description)) == "series_description");
        CHECK(std::string_view(to_string(routing_field::body_part)) == "body_part");
        CHECK(std::string_view(to_string(routing_field::patient_id_pattern)) == "patient_id_pattern");
        CHECK(std::string_view(to_string(routing_field::sop_class_uid)) == "sop_class_uid");
    }

    SECTION("from_string conversion") {
        CHECK(routing_field_from_string("modality") == routing_field::modality);
        CHECK(routing_field_from_string("station_ae") == routing_field::station_ae);
        CHECK(routing_field_from_string("institution") == routing_field::institution);
        CHECK(routing_field_from_string("department") == routing_field::department);
        CHECK(routing_field_from_string("referring_physician") == routing_field::referring_physician);
        CHECK(routing_field_from_string("study_description") == routing_field::study_description);
        CHECK(routing_field_from_string("series_description") == routing_field::series_description);
        CHECK(routing_field_from_string("body_part") == routing_field::body_part);
        CHECK(routing_field_from_string("patient_id_pattern") == routing_field::patient_id_pattern);
        CHECK(routing_field_from_string("sop_class_uid") == routing_field::sop_class_uid);
        CHECK(routing_field_from_string("unknown") == routing_field::modality);  // Default
    }
}

// =============================================================================
// Routing Condition Tests
// =============================================================================

TEST_CASE("routing_condition construction", "[routing_types]") {
    SECTION("default construction") {
        routing_condition cond;
        CHECK(cond.pattern.empty());
        CHECK_FALSE(cond.case_sensitive);
        CHECK_FALSE(cond.negate);
    }

    SECTION("parameterized construction") {
        routing_condition cond{routing_field::modality, "CT", true, false};
        CHECK(cond.match_field == routing_field::modality);
        CHECK(cond.pattern == "CT");
        CHECK(cond.case_sensitive);
        CHECK_FALSE(cond.negate);
    }

    SECTION("negation construction") {
        routing_condition cond{routing_field::modality, "PT", false, true};
        CHECK(cond.negate);
    }
}

// =============================================================================
// Routing Action Tests
// =============================================================================

TEST_CASE("routing_action construction", "[routing_types]") {
    SECTION("default construction") {
        routing_action action;
        CHECK(action.destination_node_id.empty());
        CHECK(action.priority == job_priority::normal);
        CHECK(action.delay.count() == 0);
        CHECK_FALSE(action.delete_after_send);
        CHECK(action.notify_on_failure);
    }

    SECTION("parameterized construction") {
        routing_action action{"archive-1", job_priority::high, std::chrono::minutes{5}};
        CHECK(action.destination_node_id == "archive-1");
        CHECK(action.priority == job_priority::high);
        CHECK(action.delay.count() == 5);
    }
}

// =============================================================================
// Routing Rule Tests
// =============================================================================

TEST_CASE("routing_rule construction", "[routing_types]") {
    SECTION("default values") {
        routing_rule rule;
        CHECK(rule.rule_id.empty());
        CHECK(rule.name.empty());
        CHECK(rule.enabled);
        CHECK(rule.priority == 0);
        CHECK(rule.conditions.empty());
        CHECK(rule.actions.empty());
        CHECK(rule.pk == 0);
    }

    SECTION("is_effective_now") {
        routing_rule rule;
        rule.enabled = true;

        SECTION("enabled without time constraints") {
            CHECK(rule.is_effective_now());
        }

        SECTION("disabled rule") {
            rule.enabled = false;
            CHECK_FALSE(rule.is_effective_now());
        }

        SECTION("with past effective_until") {
            rule.effective_until = std::chrono::system_clock::now() - std::chrono::hours{1};
            CHECK_FALSE(rule.is_effective_now());
        }

        SECTION("with future effective_from") {
            rule.effective_from = std::chrono::system_clock::now() + std::chrono::hours{1};
            CHECK_FALSE(rule.is_effective_now());
        }
    }
}

// =============================================================================
// Routing Manager Config Tests
// =============================================================================

TEST_CASE("routing_manager_config defaults", "[routing_types]") {
    routing_manager_config config;
    CHECK(config.enabled);
    CHECK(config.max_rules == 100);
    CHECK(config.evaluation_timeout == std::chrono::seconds{5});
}

// =============================================================================
// Routing Statistics Tests
// =============================================================================

TEST_CASE("routing_statistics defaults", "[routing_types]") {
    routing_statistics stats;
    CHECK(stats.total_evaluated == 0);
    CHECK(stats.total_matched == 0);
    CHECK(stats.total_forwarded == 0);
    CHECK(stats.total_failed == 0);
}

// =============================================================================
// Routing Test Result Tests
// =============================================================================

TEST_CASE("routing_test_result defaults", "[routing_types]") {
    routing_test_result result;
    CHECK_FALSE(result.matched);
    CHECK(result.matched_rule_id.empty());
    CHECK(result.actions.empty());
}

// =============================================================================
// Complete Rule Configuration Tests
// =============================================================================

TEST_CASE("complete rule configuration", "[routing_types]") {
    routing_rule rule;
    rule.rule_id = "ct-to-archive";
    rule.name = "Forward CT to Archive";
    rule.description = "Route all CT images to archive server";
    rule.enabled = true;
    rule.priority = 10;

    // Add condition: modality must be CT
    rule.conditions.push_back({routing_field::modality, "CT"});

    // Add action: forward to archive server
    routing_action action;
    action.destination_node_id = "archive-server-1";
    action.priority = job_priority::normal;
    rule.actions.push_back(action);

    CHECK(rule.rule_id == "ct-to-archive");
    CHECK(rule.conditions.size() == 1);
    CHECK(rule.conditions[0].match_field == routing_field::modality);
    CHECK(rule.conditions[0].pattern == "CT");
    CHECK(rule.actions.size() == 1);
    CHECK(rule.actions[0].destination_node_id == "archive-server-1");
}

TEST_CASE("multiple conditions rule", "[routing_types]") {
    routing_rule rule;
    rule.rule_id = "mr-brain-to-neuro";
    rule.name = "Forward MR Brain to Neuro PACS";
    rule.priority = 20;

    // Multiple conditions: modality=MR AND body_part=BRAIN
    rule.conditions.push_back({routing_field::modality, "MR"});
    rule.conditions.push_back({routing_field::body_part, "BRAIN", false, false});

    // Action with delay
    rule.actions.push_back({"neuro-pacs", job_priority::high, std::chrono::minutes{2}});

    CHECK(rule.conditions.size() == 2);
    CHECK(rule.conditions[0].match_field == routing_field::modality);
    CHECK(rule.conditions[1].match_field == routing_field::body_part);
    CHECK(rule.actions[0].delay.count() == 2);
}

TEST_CASE("wildcard pattern conditions", "[routing_types]") {
    routing_rule rule;
    rule.rule_id = "test-wildcard";

    // Wildcard patterns
    SECTION("star wildcard") {
        rule.conditions.push_back({routing_field::institution, "Hospital*"});
        CHECK(rule.conditions[0].pattern == "Hospital*");
    }

    SECTION("question mark wildcard") {
        rule.conditions.push_back({routing_field::patient_id_pattern, "P?????"});
        CHECK(rule.conditions[0].pattern == "P?????");
    }

    SECTION("complex wildcard") {
        rule.conditions.push_back({routing_field::study_description, "*CT*CHEST*"});
        CHECK(rule.conditions[0].pattern == "*CT*CHEST*");
    }
}

TEST_CASE("negation conditions", "[routing_types]") {
    routing_rule rule;
    rule.rule_id = "exclude-pr";
    rule.name = "Forward everything except PR";

    // Exclude Presentation State modality
    routing_condition exclude_pr{routing_field::modality, "PR", false, true};
    rule.conditions.push_back(exclude_pr);

    CHECK(rule.conditions.size() == 1);
    CHECK(rule.conditions[0].negate);
}

TEST_CASE("multiple destinations", "[routing_types]") {
    routing_rule rule;
    rule.rule_id = "multi-dest";
    rule.name = "Forward to multiple destinations";

    // Forward to multiple archives
    rule.actions.push_back({"primary-archive"});
    rule.actions.push_back({"backup-archive", job_priority::low, std::chrono::minutes{30}});
    rule.actions.push_back({"cloud-archive", job_priority::low, std::chrono::minutes{60}});

    CHECK(rule.actions.size() == 3);
    CHECK(rule.actions[0].destination_node_id == "primary-archive");
    CHECK(rule.actions[1].destination_node_id == "backup-archive");
    CHECK(rule.actions[2].destination_node_id == "cloud-archive");
    CHECK(rule.actions[2].delay.count() == 60);
}
