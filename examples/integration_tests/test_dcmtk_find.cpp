/**
 * @file test_dcmtk_find.cpp
 * @brief C-FIND (Query) interoperability tests with DCMTK
 *
 * Tests bidirectional C-FIND compatibility between pacs_system and DCMTK:
 * - Scenario A: pacs_system SCP <- DCMTK findscu
 * - Scenario B: DCMTK SCP <- pacs_system SCU (using association)
 *
 * @see Issue #453 - C-FIND Interoperability Test with DCMTK
 * @see Issue #449 - DCMTK Interoperability Test Automation Epic
 */

#include <catch2/catch_test_macros.hpp>

#include "dcmtk_tool.hpp"
#include "test_fixtures.hpp"

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/transfer_syntax.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/query_scp.hpp"
#include "pacs/services/storage_scp.hpp"
#include "pacs/services/verification_scp.hpp"

#include <future>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

using namespace pacs::integration_test;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::services;
using namespace pacs::core;
using namespace pacs::encoding;

// =============================================================================
// Test Fixture: Query Response Database
// =============================================================================

namespace {

/**
 * @brief Simple in-memory database for query test responses
 */
class test_query_database {
public:
    void add_study(const dicom_dataset& ds) {
        std::lock_guard<std::mutex> lock(mutex_);
        studies_.push_back(ds);
    }

    std::vector<dicom_dataset> find_studies(const dicom_dataset& query_keys) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<dicom_dataset> results;

        auto query_patient_id = query_keys.get_string(tags::patient_id);
        auto query_patient_name = query_keys.get_string(tags::patient_name);
        auto query_study_date = query_keys.get_string(tags::study_date);
        auto query_modality = query_keys.get_string(tags::modalities_in_study);

        for (const auto& study : studies_) {
            bool match = true;

            // Match PatientID (exact or wildcard)
            if (!query_patient_id.empty()) {
                auto study_patient_id = study.get_string(tags::patient_id);
                if (!matches_wildcard(study_patient_id, query_patient_id)) {
                    match = false;
                }
            }

            // Match PatientName (exact or wildcard)
            if (match && !query_patient_name.empty()) {
                auto study_patient_name = study.get_string(tags::patient_name);
                if (!matches_wildcard(study_patient_name, query_patient_name)) {
                    match = false;
                }
            }

            // Match StudyDate (exact or range)
            if (match && !query_study_date.empty()) {
                auto study_date = study.get_string(tags::study_date);
                if (!matches_date_range(study_date, query_study_date)) {
                    match = false;
                }
            }

            // Match Modality
            if (match && !query_modality.empty()) {
                auto study_modality = study.get_string(tags::modalities_in_study);
                if (study_modality.find(query_modality) == std::string::npos) {
                    match = false;
                }
            }

            if (match) {
                results.push_back(study);
            }
        }

        return results;
    }

    std::vector<dicom_dataset> find_patients(const dicom_dataset& query_keys) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<dicom_dataset> results;
        std::set<std::string> seen_patients;

        auto query_patient_id = query_keys.get_string(tags::patient_id);
        auto query_patient_name = query_keys.get_string(tags::patient_name);

        for (const auto& study : studies_) {
            auto patient_id = study.get_string(tags::patient_id);

            // Skip duplicates
            if (seen_patients.count(patient_id) > 0) continue;

            bool match = true;

            if (!query_patient_id.empty()) {
                if (!matches_wildcard(patient_id, query_patient_id)) {
                    match = false;
                }
            }

            if (match && !query_patient_name.empty()) {
                auto patient_name = study.get_string(tags::patient_name);
                if (!matches_wildcard(patient_name, query_patient_name)) {
                    match = false;
                }
            }

            if (match) {
                dicom_dataset patient_ds;
                patient_ds.set_string(tags::patient_id, vr_type::LO,
                    study.get_string(tags::patient_id));
                patient_ds.set_string(tags::patient_name, vr_type::PN,
                    study.get_string(tags::patient_name));
                patient_ds.set_string(tags::patient_birth_date, vr_type::DA,
                    study.get_string(tags::patient_birth_date));
                patient_ds.set_string(tags::patient_sex, vr_type::CS,
                    study.get_string(tags::patient_sex));
                patient_ds.set_string(tags::query_retrieve_level, vr_type::CS, "PATIENT");

                results.push_back(std::move(patient_ds));
                seen_patients.insert(patient_id);
            }
        }

        return results;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        studies_.clear();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return studies_.size();
    }

private:
    static bool matches_wildcard(const std::string& value, const std::string& pattern) {
        if (pattern.empty()) return true;
        if (pattern == "*") return true;

        // Simple wildcard matching (* at end)
        if (pattern.back() == '*') {
            auto prefix = pattern.substr(0, pattern.size() - 1);
            return value.substr(0, prefix.size()) == prefix;
        }

        // Single character wildcard (?)
        if (pattern.find('?') != std::string::npos) {
            if (value.size() != pattern.size()) return false;
            for (size_t i = 0; i < pattern.size(); ++i) {
                if (pattern[i] != '?' && pattern[i] != value[i]) {
                    return false;
                }
            }
            return true;
        }

        // Exact match
        return value == pattern;
    }

    static bool matches_date_range(const std::string& value, const std::string& range) {
        if (range.empty()) return true;

        // Date range format: YYYYMMDD-YYYYMMDD or -YYYYMMDD or YYYYMMDD-
        auto dash_pos = range.find('-');
        if (dash_pos == std::string::npos) {
            // Exact date match
            return value == range;
        }

        auto start_date = range.substr(0, dash_pos);
        auto end_date = range.substr(dash_pos + 1);

        if (!start_date.empty() && value < start_date) return false;
        if (!end_date.empty() && value > end_date) return false;

        return true;
    }

    mutable std::mutex mutex_;
    std::vector<dicom_dataset> studies_;
};

/**
 * @brief Create a test study dataset
 */
dicom_dataset create_test_study(
    const std::string& patient_id,
    const std::string& patient_name,
    const std::string& study_date,
    const std::string& modality) {

    dicom_dataset ds;

    // Patient level
    ds.set_string(tags::patient_id, vr_type::LO, patient_id);
    ds.set_string(tags::patient_name, vr_type::PN, patient_name);
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19700101");
    ds.set_string(tags::patient_sex, vr_type::CS, "M");

    // Study level
    ds.set_string(tags::study_instance_uid, vr_type::UI, generate_uid());
    ds.set_string(tags::study_date, vr_type::DA, study_date);
    ds.set_string(tags::study_time, vr_type::TM, "120000");
    ds.set_string(tags::accession_number, vr_type::SH, "ACC" + patient_id);
    ds.set_string(tags::study_id, vr_type::SH, "STUDY001");
    ds.set_string(tags::study_description, vr_type::LO, "Test Study");
    ds.set_string(tags::modalities_in_study, vr_type::CS, modality);
    ds.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");

    return ds;
}

}  // namespace

// =============================================================================
// Test: pacs_system SCP with DCMTK findscu
// =============================================================================

TEST_CASE("C-FIND: pacs_system SCP with DCMTK findscu", "[dcmtk][interop][find]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed - skipping interoperability test");
    }

    // Skip if real TCP DICOM connections are not supported yet
    if (!supports_real_tcp_dicom()) {
        SKIP("pacs_system does not support real TCP DICOM connections yet");
    }

    // Setup: Start pacs_system query server with test data
    auto port = find_available_port();
    const std::string ae_title = "PACS_FIND_SCP";

    test_query_database db;

    // Populate test data
    db.add_study(create_test_study("PAT001", "SMITH^JOHN", "20231201", "CT"));
    db.add_study(create_test_study("PAT002", "SMITH^JANE", "20231215", "MR"));
    db.add_study(create_test_study("PAT003", "JONES^WILLIAM", "20240101", "CT"));
    db.add_study(create_test_study("PAT004", "BROWN^ALICE", "20240115", "XA"));

    test_server server(port, ae_title);

    // Register query SCP with handler
    auto query_scp_ptr = std::make_shared<query_scp>();
    query_scp_ptr->set_handler([&db](
        query_level level,
        const dicom_dataset& query_keys,
        const std::string& /* calling_ae */) -> std::vector<dicom_dataset> {

        if (level == query_level::patient) {
            return db.find_patients(query_keys);
        }
        return db.find_studies(query_keys);
    });
    server.register_service(query_scp_ptr);
    server.register_service(std::make_shared<verification_scp>());

    REQUIRE(server.start());

    // Wait for server to be ready
    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(port);
    }, server_ready_timeout()));

    SECTION("Basic study-level query succeeds") {
        std::vector<std::pair<std::string, std::string>> keys = {
            {"PatientID", ""},
            {"PatientName", ""},
            {"StudyDate", ""},
            {"StudyInstanceUID", ""}
        };

        auto result = dcmtk_tool::findscu(
            "localhost", port, ae_title, "STUDY", keys);

        INFO("stdout: " << result.stdout_output);
        INFO("stderr: " << result.stderr_output);

        REQUIRE(result.success());
    }

    SECTION("Query with PatientID filter") {
        std::vector<std::pair<std::string, std::string>> keys = {
            {"PatientID", "PAT001"},
            {"PatientName", ""},
            {"StudyInstanceUID", ""}
        };

        auto result = dcmtk_tool::findscu(
            "localhost", port, ae_title, "STUDY", keys);

        INFO("stdout: " << result.stdout_output);
        INFO("stderr: " << result.stderr_output);

        REQUIRE(result.success());
    }

    SECTION("Query with wildcard PatientName") {
        std::vector<std::pair<std::string, std::string>> keys = {
            {"PatientName", "SMITH*"},
            {"PatientID", ""},
            {"StudyInstanceUID", ""}
        };

        auto result = dcmtk_tool::findscu(
            "localhost", port, ae_title, "STUDY", keys);

        INFO("stdout: " << result.stdout_output);
        INFO("stderr: " << result.stderr_output);

        REQUIRE(result.success());
    }

    SECTION("Query with date range") {
        std::vector<std::pair<std::string, std::string>> keys = {
            {"StudyDate", "20231201-20231231"},
            {"PatientID", ""},
            {"PatientName", ""},
            {"StudyInstanceUID", ""}
        };

        auto result = dcmtk_tool::findscu(
            "localhost", port, ae_title, "STUDY", keys);

        INFO("stdout: " << result.stdout_output);
        INFO("stderr: " << result.stderr_output);

        REQUIRE(result.success());
    }

    SECTION("Query with no matching results") {
        std::vector<std::pair<std::string, std::string>> keys = {
            {"PatientID", "NONEXISTENT"},
            {"StudyInstanceUID", ""}
        };

        auto result = dcmtk_tool::findscu(
            "localhost", port, ae_title, "STUDY", keys);

        INFO("stdout: " << result.stdout_output);
        INFO("stderr: " << result.stderr_output);

        // Should succeed even with no results (returns 0 matches)
        REQUIRE(result.success());
    }

    SECTION("Multiple consecutive queries") {
        for (int i = 0; i < 3; ++i) {
            std::vector<std::pair<std::string, std::string>> keys = {
                {"PatientID", ""},
                {"StudyInstanceUID", ""}
            };

            auto result = dcmtk_tool::findscu(
                "localhost", port, ae_title, "STUDY", keys,
                "FINDSCU_" + std::to_string(i));

            INFO("Iteration: " << i);
            INFO("stdout: " << result.stdout_output);
            INFO("stderr: " << result.stderr_output);

            REQUIRE(result.success());
        }
    }

    SECTION("Patient-level query") {
        std::vector<std::pair<std::string, std::string>> keys = {
            {"PatientID", ""},
            {"PatientName", ""},
            {"PatientBirthDate", ""}
        };

        auto result = dcmtk_tool::findscu(
            "localhost", port, ae_title, "PATIENT", keys);

        INFO("stdout: " << result.stdout_output);
        INFO("stderr: " << result.stderr_output);

        REQUIRE(result.success());
    }
}

// =============================================================================
// Test: DCMTK SCP with pacs_system SCU (no DCMTK SCP for C-FIND available)
// =============================================================================

TEST_CASE("C-FIND: pacs_system SCU query operations", "[dcmtk][interop][find]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed - skipping interoperability test");
    }

    // Skip if real TCP DICOM connections are not supported yet
    if (!supports_real_tcp_dicom()) {
        SKIP("pacs_system does not support real TCP DICOM connections yet");
    }

    // Setup: Start pacs_system server with query capability
    auto port = find_available_port();
    const std::string ae_title = "QUERY_SCP";

    test_query_database db;
    db.add_study(create_test_study("PAT001", "DOE^JOHN", "20240101", "CT"));
    db.add_study(create_test_study("PAT002", "DOE^JANE", "20240115", "MR"));

    test_server server(port, ae_title);

    auto query_scp_ptr = std::make_shared<query_scp>();
    query_scp_ptr->set_handler([&db](
        query_level level,
        const dicom_dataset& query_keys,
        const std::string& /* calling_ae */) -> std::vector<dicom_dataset> {

        if (level == query_level::patient) {
            return db.find_patients(query_keys);
        }
        return db.find_studies(query_keys);
    });
    server.register_service(query_scp_ptr);
    REQUIRE(server.start());

    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(port);
    }, server_ready_timeout()));

    SECTION("pacs_system SCU sends C-FIND successfully") {
        auto connect_result = test_association::connect(
            "localhost", port, ae_title, "PACS_SCU",
            {std::string(study_root_find_sop_class_uid)});

        REQUIRE(connect_result.is_ok());
        auto& assoc = connect_result.value();

        REQUIRE(assoc.has_accepted_context(study_root_find_sop_class_uid));
        auto context_id = assoc.accepted_context_id(study_root_find_sop_class_uid);
        REQUIRE(context_id.has_value());

        // Create query keys
        dicom_dataset query_keys;
        query_keys.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");
        query_keys.set_string(tags::patient_id, vr_type::LO, "");
        query_keys.set_string(tags::patient_name, vr_type::PN, "");
        query_keys.set_string(tags::study_instance_uid, vr_type::UI, "");

        // Send C-FIND request
        auto find_rq = make_c_find_rq(1, study_root_find_sop_class_uid);
        find_rq.set_dataset(std::move(query_keys));
        auto send_result = assoc.send_dimse(*context_id, find_rq);
        REQUIRE(send_result.is_ok());

        // Receive responses
        std::vector<dicom_dataset> results;
        while (true) {
            auto recv_result = assoc.receive_dimse();
            REQUIRE(recv_result.is_ok());

            auto& [recv_ctx, rsp] = recv_result.value();
            if (rsp.status() == status_success) {
                break;  // Final response
            } else if (rsp.status() == status_pending) {
                if (rsp.has_dataset()) {
                    auto ds_result = rsp.dataset();
                    if (ds_result.is_ok()) {
                        results.push_back(ds_result.value().get());
                    }
                }
            } else {
                FAIL("Unexpected C-FIND response status");
            }
        }

        // Should get 2 study results
        REQUIRE(results.size() == 2);
    }

    SECTION("Query with specific PatientName filter") {
        auto connect_result = test_association::connect(
            "localhost", port, ae_title, "PACS_SCU",
            {std::string(study_root_find_sop_class_uid)});

        REQUIRE(connect_result.is_ok());
        auto& assoc = connect_result.value();

        auto context_id = assoc.accepted_context_id(study_root_find_sop_class_uid);
        REQUIRE(context_id.has_value());

        dicom_dataset query_keys;
        query_keys.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");
        query_keys.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
        query_keys.set_string(tags::study_instance_uid, vr_type::UI, "");

        auto find_rq = make_c_find_rq(1, study_root_find_sop_class_uid);
        find_rq.set_dataset(std::move(query_keys));
        auto send_result = assoc.send_dimse(*context_id, find_rq);
        REQUIRE(send_result.is_ok());

        std::vector<dicom_dataset> results;
        while (true) {
            auto recv_result = assoc.receive_dimse();
            REQUIRE(recv_result.is_ok());

            auto& [recv_ctx, rsp] = recv_result.value();
            if (rsp.status() == status_success) break;
            if (rsp.status() == status_pending && rsp.has_dataset()) {
                auto ds_result = rsp.dataset();
                if (ds_result.is_ok()) {
                    results.push_back(ds_result.value().get());
                }
            }
        }

        // Should find exactly 1 matching study
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].get_string(tags::patient_name) == "DOE^JOHN");
    }

    SECTION("Query with wildcard pattern") {
        auto connect_result = test_association::connect(
            "localhost", port, ae_title, "PACS_SCU",
            {std::string(study_root_find_sop_class_uid)});

        REQUIRE(connect_result.is_ok());
        auto& assoc = connect_result.value();

        auto context_id = assoc.accepted_context_id(study_root_find_sop_class_uid);
        REQUIRE(context_id.has_value());

        dicom_dataset query_keys;
        query_keys.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");
        query_keys.set_string(tags::patient_name, vr_type::PN, "DOE*");
        query_keys.set_string(tags::study_instance_uid, vr_type::UI, "");

        auto find_rq = make_c_find_rq(1, study_root_find_sop_class_uid);
        find_rq.set_dataset(std::move(query_keys));
        (void)assoc.send_dimse(*context_id, find_rq);

        std::vector<dicom_dataset> results;
        while (true) {
            auto recv_result = assoc.receive_dimse();
            if (recv_result.is_err()) break;

            auto& [recv_ctx, rsp] = recv_result.value();
            if (rsp.status() == status_success) break;
            if (rsp.status() == status_pending && rsp.has_dataset()) {
                auto ds_result = rsp.dataset();
                if (ds_result.is_ok()) {
                    results.push_back(ds_result.value().get());
                }
            }
        }

        // Both DOE^JOHN and DOE^JANE should match
        REQUIRE(results.size() == 2);
    }
}

// =============================================================================
// Test: Concurrent query operations
// =============================================================================

TEST_CASE("C-FIND: Concurrent query operations", "[dcmtk][interop][find][stress]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    // Skip if real TCP DICOM connections are not supported yet
    if (!supports_real_tcp_dicom()) {
        SKIP("pacs_system does not support real TCP DICOM connections yet");
    }

    auto port = find_available_port();
    const std::string ae_title = "STRESS_FIND_SCP";

    test_query_database db;
    for (int i = 0; i < 10; ++i) {
        db.add_study(create_test_study(
            "PAT" + std::to_string(i),
            "PATIENT^" + std::to_string(i),
            "20240" + std::to_string(100 + i),
            "CT"));
    }

    test_server server(port, ae_title);

    auto query_scp_ptr = std::make_shared<query_scp>();
    query_scp_ptr->set_handler([&db](
        query_level /* level */,
        const dicom_dataset& query_keys,
        const std::string& /* calling_ae */) -> std::vector<dicom_dataset> {
        return db.find_studies(query_keys);
    });
    server.register_service(query_scp_ptr);
    REQUIRE(server.start());

    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(port);
    }, server_ready_timeout()));

    SECTION("3 concurrent DCMTK findscu clients") {
        constexpr int num_clients = 3;
        std::vector<std::future<dcmtk_result>> futures;
        futures.reserve(num_clients);

        for (int i = 0; i < num_clients; ++i) {
            futures.push_back(std::async(std::launch::async, [&, i]() {
                std::vector<std::pair<std::string, std::string>> keys = {
                    {"PatientID", ""},
                    {"StudyInstanceUID", ""}
                };
                return dcmtk_tool::findscu(
                    "localhost", port, ae_title, "STUDY", keys,
                    "CLIENT_" + std::to_string(i));
            }));
        }

        // All should succeed
        for (size_t i = 0; i < futures.size(); ++i) {
            auto result = futures[i].get();

            INFO("Client " << i << " stdout: " << result.stdout_output);
            INFO("Client " << i << " stderr: " << result.stderr_output);

            REQUIRE(result.success());
        }
    }

    SECTION("3 concurrent pacs_system SCU clients") {
        constexpr int num_clients = 3;
        std::vector<std::future<bool>> futures;
        futures.reserve(num_clients);

        for (int i = 0; i < num_clients; ++i) {
            futures.push_back(std::async(std::launch::async, [&, i]() {
                auto connect_result = test_association::connect(
                    "localhost", port, ae_title,
                    "PACS_CLIENT_" + std::to_string(i),
                    {std::string(study_root_find_sop_class_uid)});

                if (!connect_result.is_ok()) return false;

                auto& assoc = connect_result.value();
                auto context_id = assoc.accepted_context_id(study_root_find_sop_class_uid);
                if (!context_id.has_value()) return false;

                dicom_dataset query_keys;
                query_keys.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");
                query_keys.set_string(tags::patient_id, vr_type::LO, "");
                query_keys.set_string(tags::study_instance_uid, vr_type::UI, "");

                auto find_rq = make_c_find_rq(1, study_root_find_sop_class_uid);
                find_rq.set_dataset(std::move(query_keys));
                auto send_result = assoc.send_dimse(*context_id, find_rq);
                if (!send_result.is_ok()) return false;

                // Receive all responses
                while (true) {
                    auto recv_result = assoc.receive_dimse();
                    if (!recv_result.is_ok()) return false;

                    auto& [recv_ctx, rsp] = recv_result.value();
                    if (rsp.status() == status_success) break;
                    if (rsp.status() != status_pending) return false;
                }

                return true;
            }));
        }

        // All should succeed
        for (size_t i = 0; i < futures.size(); ++i) {
            bool success = futures[i].get();
            INFO("Client " << i);
            REQUIRE(success);
        }
    }
}

// =============================================================================
// Test: Connection error handling
// =============================================================================

TEST_CASE("C-FIND: Connection error handling", "[dcmtk][interop][find][error]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    // Skip if real TCP DICOM connections are not supported yet
    if (!supports_real_tcp_dicom()) {
        SKIP("pacs_system does not support real TCP DICOM connections yet");
    }

    SECTION("findscu to non-existent server fails gracefully") {
        auto port = find_available_port();

        // Ensure nothing is listening
        REQUIRE_FALSE(process_launcher::is_port_listening(port));

        std::vector<std::pair<std::string, std::string>> keys = {
            {"PatientID", ""},
            {"StudyInstanceUID", ""}
        };

        auto result = dcmtk_tool::findscu(
            "localhost", port, "NONEXISTENT", "STUDY", keys,
            "FINDSCU", std::chrono::seconds{5});

        // Should fail - no server listening
        REQUIRE_FALSE(result.success());
    }

    SECTION("pacs_system SCU to non-existent server fails gracefully") {
        // Use a high port range that's less likely to have conflicts
        auto port = find_available_port(59000);

        // Wait briefly and re-verify the port is truly free
        std::this_thread::sleep_for(std::chrono::milliseconds{100});

        // Ensure nothing is listening
        if (process_launcher::is_port_listening(port)) {
            SKIP("Port " + std::to_string(port) + " is unexpectedly in use");
        }

        auto connect_result = test_association::connect(
            "localhost", port, "NONEXISTENT", "PACS_SCU",
            {std::string(study_root_find_sop_class_uid)});

        // Should fail - no server listening
        REQUIRE_FALSE(connect_result.is_ok());
    }
}

// =============================================================================
// Test: Query level variations
// =============================================================================

TEST_CASE("C-FIND: Query level variations", "[dcmtk][interop][find][levels]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    // Skip if real TCP DICOM connections are not supported yet
    if (!supports_real_tcp_dicom()) {
        SKIP("pacs_system does not support real TCP DICOM connections yet");
    }

    auto port = find_available_port();
    const std::string ae_title = "LEVEL_TEST_SCP";

    test_query_database db;
    db.add_study(create_test_study("PAT001", "TEST^PATIENT", "20240101", "CT"));

    test_server server(port, ae_title);

    auto query_scp_ptr = std::make_shared<query_scp>();
    query_scp_ptr->set_handler([&db](
        query_level level,
        const dicom_dataset& query_keys,
        const std::string& /* calling_ae */) -> std::vector<dicom_dataset> {

        if (level == query_level::patient) {
            return db.find_patients(query_keys);
        }
        return db.find_studies(query_keys);
    });
    server.register_service(query_scp_ptr);
    REQUIRE(server.start());

    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(port);
    }, server_ready_timeout()));

    SECTION("STUDY level query") {
        std::vector<std::pair<std::string, std::string>> keys = {
            {"PatientID", "PAT001"},
            {"StudyInstanceUID", ""}
        };

        auto result = dcmtk_tool::findscu(
            "localhost", port, ae_title, "STUDY", keys);

        REQUIRE(result.success());
    }

    SECTION("PATIENT level query") {
        std::vector<std::pair<std::string, std::string>> keys = {
            {"PatientID", ""},
            {"PatientName", ""}
        };

        auto result = dcmtk_tool::findscu(
            "localhost", port, ae_title, "PATIENT", keys);

        REQUIRE(result.success());
    }
}

// =============================================================================
// Test: Special character handling
// =============================================================================

TEST_CASE("C-FIND: Special character handling", "[dcmtk][interop][find][special]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    // Skip if real TCP DICOM connections are not supported yet
    if (!supports_real_tcp_dicom()) {
        SKIP("pacs_system does not support real TCP DICOM connections yet");
    }

    auto port = find_available_port();
    const std::string ae_title = "SPECIAL_CHAR_SCP";

    test_query_database db;
    // Add study with special characters in patient name
    db.add_study(create_test_study("PAT001", "O'BRIEN^MARY", "20240101", "CT"));
    db.add_study(create_test_study("PAT002", "MÜLLER^HANS", "20240101", "MR"));

    test_server server(port, ae_title);

    auto query_scp_ptr = std::make_shared<query_scp>();
    query_scp_ptr->set_handler([&db](
        query_level /* level */,
        const dicom_dataset& query_keys,
        const std::string& /* calling_ae */) -> std::vector<dicom_dataset> {
        return db.find_studies(query_keys);
    });
    server.register_service(query_scp_ptr);
    REQUIRE(server.start());

    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(port);
    }, server_ready_timeout()));

    SECTION("Query with special characters in response") {
        std::vector<std::pair<std::string, std::string>> keys = {
            {"PatientID", "PAT001"},
            {"PatientName", ""},
            {"StudyInstanceUID", ""}
        };

        auto result = dcmtk_tool::findscu(
            "localhost", port, ae_title, "STUDY", keys);

        INFO("stdout: " << result.stdout_output);
        INFO("stderr: " << result.stderr_output);

        // Should succeed despite special characters
        REQUIRE(result.success());
    }
}
