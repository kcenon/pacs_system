/**
 * @file test_dcmtk_move.cpp
 * @brief C-MOVE (Retrieve) interoperability tests with DCMTK
 *
 * Tests bidirectional C-MOVE compatibility between pacs_system and DCMTK:
 * - Scenario A: pacs_system SCP <- DCMTK movescu (with DCMTK storescp destination)
 * - Scenario B: pacs_system as both SCP and destination
 *
 * @see Issue #454 - C-MOVE Interoperability Test with DCMTK
 * @see Issue #449 - DCMTK Interoperability Test Automation Epic
 */

#include <catch2/catch_test_macros.hpp>

#include "dcmtk_tool.hpp"
#include "test_fixtures.hpp"

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/transfer_syntax.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/query_scp.hpp"
#include "pacs/services/retrieve_scp.hpp"
#include "pacs/services/storage_scp.hpp"
#include "pacs/services/storage_scu.hpp"
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
// Test Fixture: DICOM File Repository
// =============================================================================

namespace {

/**
 * @brief In-memory repository of DICOM files for C-MOVE tests
 */
class test_file_repository {
public:
    void add_file(const dicom_file& file) {
        std::lock_guard<std::mutex> lock(mutex_);
        files_.push_back(file);
    }

    void add_file(dicom_file&& file) {
        std::lock_guard<std::mutex> lock(mutex_);
        files_.push_back(std::move(file));
    }

    std::vector<dicom_file> find_by_patient_id(const std::string& patient_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<dicom_file> results;

        for (const auto& file : files_) {
            auto file_patient_id = file.dataset().get_string(tags::patient_id);
            if (file_patient_id == patient_id || patient_id.empty()) {
                results.push_back(file);
            }
        }

        return results;
    }

    std::vector<dicom_file> find_by_study_uid(const std::string& study_uid) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<dicom_file> results;

        for (const auto& file : files_) {
            auto file_study_uid = file.dataset().get_string(tags::study_instance_uid);
            if (file_study_uid == study_uid || study_uid.empty()) {
                results.push_back(file);
            }
        }

        return results;
    }

    std::vector<dicom_file> find_all(const dicom_dataset& query_keys) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<dicom_file> results;

        auto query_patient_id = query_keys.get_string(tags::patient_id);
        auto query_study_uid = query_keys.get_string(tags::study_instance_uid);

        for (const auto& file : files_) {
            bool match = true;

            if (!query_patient_id.empty()) {
                auto file_patient_id = file.dataset().get_string(tags::patient_id);
                if (file_patient_id != query_patient_id) {
                    match = false;
                }
            }

            if (match && !query_study_uid.empty()) {
                auto file_study_uid = file.dataset().get_string(tags::study_instance_uid);
                if (file_study_uid != query_study_uid) {
                    match = false;
                }
            }

            if (match) {
                results.push_back(file);
            }
        }

        return results;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        files_.clear();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return files_.size();
    }

private:
    mutable std::mutex mutex_;
    std::vector<dicom_file> files_;
};

/**
 * @brief Create a test DICOM file
 */
dicom_file create_test_dicom_file(
    const std::string& patient_id,
    const std::string& patient_name,
    const std::string& study_uid) {

    dicom_dataset ds;

    // SOP Common
    auto sop_instance_uid = generate_uid();
    ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");  // CT
    ds.set_string(tags::sop_instance_uid, vr_type::UI, sop_instance_uid);

    // Patient
    ds.set_string(tags::patient_id, vr_type::LO, patient_id);
    ds.set_string(tags::patient_name, vr_type::PN, patient_name);
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19700101");
    ds.set_string(tags::patient_sex, vr_type::CS, "M");

    // Study
    ds.set_string(tags::study_instance_uid, vr_type::UI, study_uid);
    ds.set_string(tags::study_date, vr_type::DA, "20240101");
    ds.set_string(tags::study_time, vr_type::TM, "120000");
    ds.set_string(tags::study_id, vr_type::SH, "STUDY001");
    ds.set_string(tags::accession_number, vr_type::SH, "ACC001");

    // Series
    ds.set_string(tags::series_instance_uid, vr_type::UI, generate_uid());
    ds.set_string(tags::series_number, vr_type::IS, "1");
    ds.set_string(tags::modality, vr_type::CS, "CT");

    // Instance
    ds.set_string(tags::instance_number, vr_type::IS, "1");

    return dicom_file::create(std::move(ds), transfer_syntax::explicit_vr_little_endian);
}

/**
 * @brief Track received files in destination storage
 */
class received_file_tracker {
public:
    void on_file_received(
        const dicom_dataset& dataset,
        const std::string& /* calling_ae */,
        const std::string& /* sop_class_uid */,
        const std::string& /* sop_instance_uid */) {

        std::lock_guard<std::mutex> lock(mutex_);
        auto sop_uid = dataset.get_string(tags::sop_instance_uid);
        received_sop_uids_.insert(sop_uid);
    }

    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return received_sop_uids_.size();
    }

    bool received(const std::string& sop_uid) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return received_sop_uids_.count(sop_uid) > 0;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        received_sop_uids_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::set<std::string> received_sop_uids_;
};

/**
 * @brief Count DICOM files in directory
 */
size_t count_dicom_files(const std::filesystem::path& dir) {
    size_t count = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            // DCMTK storescp may save with .dcm or without extension
            if (ext == ".dcm" || ext.empty()) {
                ++count;
            }
        }
    }
    return count;
}

/**
 * @brief Create C-MOVE request message
 */
dimse_message make_c_move_rq(
    uint16_t message_id,
    std::string_view sop_class_uid,
    const std::string& move_destination) {

    dimse_message msg{command_field::c_move_rq, message_id};
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_priority(priority_medium);

    // Set Move Destination AE
    msg.command_set().set_string(
        tag_move_destination,
        vr_type::AE,
        move_destination);

    return msg;
}

}  // namespace

// =============================================================================
// Test: pacs_system Move SCP with DCMTK movescu
// =============================================================================

TEST_CASE("C-MOVE: pacs_system SCP with DCMTK movescu", "[dcmtk][interop][move]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed - skipping interoperability test");
    }

    // Setup: Ports and AE titles
    auto move_port = find_available_port();
    auto dest_port = find_available_port(move_port + 1);
    const std::string move_ae = "MOVE_SCP";
    const std::string dest_ae = "DEST_SCP";

    // Setup: File repository with test data
    test_file_repository repository;
    auto study_uid = generate_uid();
    auto file1 = create_test_dicom_file("PAT001", "DOE^JOHN", study_uid);
    auto file2 = create_test_dicom_file("PAT001", "DOE^JOHN", study_uid);
    repository.add_file(std::move(file1));
    repository.add_file(std::move(file2));

    // Setup: Create destination directory for DCMTK storescp
    test_directory dest_dir;

    // Start DCMTK storescp as destination
    auto dcmtk_dest = dcmtk_tool::storescp(dest_port, dest_ae, dest_dir.path());
    REQUIRE(dcmtk_dest.is_running());

    // Wait for destination to be ready
    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(dest_port);
    }, std::chrono::milliseconds{10000}));

    // Setup: pacs_system Move SCP
    test_server server(move_port, move_ae);

    auto retrieve_scp_ptr = std::make_shared<retrieve_scp>();

    // Set retrieve handler
    retrieve_scp_ptr->set_retrieve_handler([&repository](
        const dicom_dataset& query_keys) -> std::vector<dicom_file> {
        return repository.find_all(query_keys);
    });

    // Set destination resolver
    retrieve_scp_ptr->set_destination_resolver([dest_port, dest_ae](
        const std::string& ae_title) -> std::optional<std::pair<std::string, uint16_t>> {
        if (ae_title == dest_ae) {
            return std::make_pair("localhost", dest_port);
        }
        return std::nullopt;
    });

    server.register_service(retrieve_scp_ptr);
    server.register_service(std::make_shared<verification_scp>());

    REQUIRE(server.start());

    // Wait for server to be ready
    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(move_port);
    }, std::chrono::milliseconds{5000}));

    SECTION("C-MOVE by StudyInstanceUID succeeds") {
        std::vector<std::pair<std::string, std::string>> keys = {
            {"StudyInstanceUID", study_uid}
        };

        auto result = dcmtk_tool::movescu(
            "localhost", move_port, move_ae, dest_ae,
            "STUDY", keys);

        INFO("stdout: " << result.stdout_output);
        INFO("stderr: " << result.stderr_output);

        REQUIRE(result.success());

        // Wait for files to be received
        std::this_thread::sleep_for(std::chrono::seconds{2});

        // Verify files were received
        auto received_count = count_dicom_files(dest_dir.path());
        REQUIRE(received_count >= 2);
    }

    SECTION("C-MOVE by PatientID succeeds") {
        std::vector<std::pair<std::string, std::string>> keys = {
            {"PatientID", "PAT001"}
        };

        auto result = dcmtk_tool::movescu(
            "localhost", move_port, move_ae, dest_ae,
            "PATIENT", keys);

        INFO("stdout: " << result.stdout_output);
        INFO("stderr: " << result.stderr_output);

        REQUIRE(result.success());
    }

    SECTION("C-MOVE with empty result set completes gracefully") {
        std::vector<std::pair<std::string, std::string>> keys = {
            {"StudyInstanceUID", "1.2.3.4.5.6.7.8.9.999999"}
        };

        auto result = dcmtk_tool::movescu(
            "localhost", move_port, move_ae, dest_ae,
            "STUDY", keys);

        INFO("stdout: " << result.stdout_output);
        INFO("stderr: " << result.stderr_output);

        // Should succeed even with no matching results
        REQUIRE(result.success());
    }
}

// =============================================================================
// Test: Unknown destination AE handling
// =============================================================================

TEST_CASE("C-MOVE: Unknown destination AE rejection", "[dcmtk][interop][move][error]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    auto port = find_available_port();
    const std::string ae_title = "MOVE_SCP";

    test_file_repository repository;
    auto study_uid = generate_uid();
    repository.add_file(create_test_dicom_file("PAT001", "DOE^JOHN", study_uid));

    test_server server(port, ae_title);

    auto retrieve_scp_ptr = std::make_shared<retrieve_scp>();
    retrieve_scp_ptr->set_retrieve_handler([&repository](
        const dicom_dataset& query_keys) -> std::vector<dicom_file> {
        return repository.find_all(query_keys);
    });

    // Only resolve known AE titles
    retrieve_scp_ptr->set_destination_resolver([](
        const std::string& ae) -> std::optional<std::pair<std::string, uint16_t>> {
        if (ae == "KNOWN_DEST") {
            return std::make_pair("localhost", 11113);
        }
        return std::nullopt;  // Unknown AE
    });

    server.register_service(retrieve_scp_ptr);
    REQUIRE(server.start());

    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(port);
    }, std::chrono::milliseconds{5000}));

    SECTION("Unknown destination AE is rejected") {
        std::vector<std::pair<std::string, std::string>> keys = {
            {"StudyInstanceUID", study_uid}
        };

        // Use unknown destination AE
        auto result = dcmtk_tool::movescu(
            "localhost", port, ae_title, "UNKNOWN_DEST",
            "STUDY", keys);

        INFO("stdout: " << result.stdout_output);
        INFO("stderr: " << result.stderr_output);

        // Should fail due to unknown destination
        REQUIRE_FALSE(result.success());
    }
}

// =============================================================================
// Test: Connection error handling
// =============================================================================

TEST_CASE("C-MOVE: Connection error handling", "[dcmtk][interop][move][error]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    SECTION("movescu to non-existent server fails gracefully") {
        auto port = find_available_port();

        // Ensure nothing is listening
        REQUIRE_FALSE(process_launcher::is_port_listening(port));

        std::vector<std::pair<std::string, std::string>> keys = {
            {"StudyInstanceUID", "1.2.3.4.5"}
        };

        auto result = dcmtk_tool::movescu(
            "localhost", port, "NONEXISTENT", "DEST",
            "STUDY", keys,
            "MOVESCU", std::chrono::seconds{10});

        // Should fail - no server listening
        REQUIRE_FALSE(result.success());
    }
}

// =============================================================================
// Test: Concurrent move operations
// =============================================================================

TEST_CASE("C-MOVE: Concurrent operations", "[dcmtk][interop][move][stress]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    auto move_port = find_available_port();
    auto dest_port = find_available_port(move_port + 1);
    const std::string move_ae = "STRESS_MOVE_SCP";
    const std::string dest_ae = "STRESS_DEST";

    // Create multiple studies
    test_file_repository repository;
    std::vector<std::string> study_uids;
    for (int i = 0; i < 3; ++i) {
        auto study_uid = generate_uid();
        study_uids.push_back(study_uid);
        repository.add_file(create_test_dicom_file(
            "PAT00" + std::to_string(i),
            "PATIENT^" + std::to_string(i),
            study_uid));
    }

    test_directory dest_dir;

    // Start DCMTK storescp
    auto dcmtk_dest = dcmtk_tool::storescp(dest_port, dest_ae, dest_dir.path());
    REQUIRE(dcmtk_dest.is_running());

    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(dest_port);
    }, std::chrono::milliseconds{10000}));

    // Setup pacs_system Move SCP
    test_server server(move_port, move_ae);

    auto retrieve_scp_ptr = std::make_shared<retrieve_scp>();
    retrieve_scp_ptr->set_retrieve_handler([&repository](
        const dicom_dataset& query_keys) -> std::vector<dicom_file> {
        return repository.find_all(query_keys);
    });

    retrieve_scp_ptr->set_destination_resolver([dest_port, dest_ae](
        const std::string& ae) -> std::optional<std::pair<std::string, uint16_t>> {
        if (ae == dest_ae) {
            return std::make_pair("localhost", dest_port);
        }
        return std::nullopt;
    });

    server.register_service(retrieve_scp_ptr);
    REQUIRE(server.start());

    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(move_port);
    }, std::chrono::milliseconds{5000}));

    SECTION("Multiple concurrent move requests") {
        constexpr int num_requests = 2;
        std::vector<std::future<dcmtk_result>> futures;
        futures.reserve(num_requests);

        for (int i = 0; i < num_requests; ++i) {
            futures.push_back(std::async(std::launch::async, [&, i]() {
                std::vector<std::pair<std::string, std::string>> keys = {
                    {"StudyInstanceUID", study_uids[i]}
                };
                return dcmtk_tool::movescu(
                    "localhost", move_port, move_ae, dest_ae,
                    "STUDY", keys,
                    "MOVESCU_" + std::to_string(i));
            }));
        }

        // All should succeed
        for (size_t i = 0; i < futures.size(); ++i) {
            auto result = futures[i].get();

            INFO("Request " << i << " stdout: " << result.stdout_output);
            INFO("Request " << i << " stderr: " << result.stderr_output);

            REQUIRE(result.success());
        }
    }
}

// =============================================================================
// Test: pacs_system as Move SCU (with DCMTK SCP - limited test)
// =============================================================================

TEST_CASE("C-MOVE: pacs_system SCU basic operation", "[dcmtk][interop][move]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    // Note: DCMTK doesn't provide a simple move SCP for testing.
    // This section tests pacs_system's move SCU capability by connecting
    // to our own pacs_system move SCP (which has been validated above).

    auto move_port = find_available_port();
    auto dest_port = find_available_port(move_port + 1);
    const std::string move_ae = "MOVE_SCP";
    const std::string dest_ae = "DEST_SCP";

    test_file_repository repository;
    auto study_uid = generate_uid();
    repository.add_file(create_test_dicom_file("PAT001", "DOE^JOHN", study_uid));

    // Received file tracker for destination
    received_file_tracker tracker;
    test_directory dest_dir;

    // Start pacs_system as destination storage SCP
    test_server dest_server(dest_port, dest_ae);
    auto storage_scp_ptr = std::make_shared<storage_scp>();
    storage_scp_ptr->set_handler([&tracker](
        const dicom_dataset& dataset,
        const std::string& calling_ae,
        const std::string& sop_class_uid,
        const std::string& sop_instance_uid) -> storage_status {
        tracker.on_file_received(dataset, calling_ae, sop_class_uid, sop_instance_uid);
        return storage_status::success;
    });
    dest_server.register_service(storage_scp_ptr);
    REQUIRE(dest_server.start());

    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(dest_port);
    }, std::chrono::milliseconds{5000}));

    // Start Move SCP
    test_server move_server(move_port, move_ae);
    auto retrieve_scp_ptr = std::make_shared<retrieve_scp>();
    retrieve_scp_ptr->set_retrieve_handler([&repository](
        const dicom_dataset& query_keys) -> std::vector<dicom_file> {
        return repository.find_all(query_keys);
    });

    retrieve_scp_ptr->set_destination_resolver([dest_port, dest_ae](
        const std::string& ae) -> std::optional<std::pair<std::string, uint16_t>> {
        if (ae == dest_ae) {
            return std::make_pair("localhost", dest_port);
        }
        return std::nullopt;
    });

    move_server.register_service(retrieve_scp_ptr);
    REQUIRE(move_server.start());

    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(move_port);
    }, std::chrono::milliseconds{5000}));

    SECTION("pacs_system SCU sends C-MOVE request") {
        // Connect to Move SCP
        auto connect_result = test_association::connect(
            "localhost", move_port, move_ae, "PACS_SCU",
            {std::string(study_root_move_sop_class_uid)});

        REQUIRE(connect_result.is_ok());
        auto& assoc = connect_result.value();

        REQUIRE(assoc.has_accepted_context(study_root_move_sop_class_uid));
        auto context_id = assoc.accepted_context_id(study_root_move_sop_class_uid);
        REQUIRE(context_id.has_value());

        // Create move request
        dicom_dataset move_keys;
        move_keys.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");
        move_keys.set_string(tags::study_instance_uid, vr_type::UI, study_uid);

        auto move_rq = make_c_move_rq(1, study_root_move_sop_class_uid, dest_ae);
        move_rq.set_dataset(std::move(move_keys));

        auto send_result = assoc.send_dimse(*context_id, move_rq);
        REQUIRE(send_result.is_ok());

        // Receive move responses
        bool final_received = false;
        while (!final_received) {
            auto recv_result = assoc.receive_dimse(std::chrono::seconds{30});
            REQUIRE(recv_result.is_ok());

            auto& [recv_ctx, rsp] = recv_result.value();
            REQUIRE(rsp.command() == command_field::c_move_rsp);

            if (rsp.status() == status_success) {
                final_received = true;
            } else if (rsp.status() != status_pending) {
                INFO("Unexpected status: " << static_cast<int>(rsp.status()));
                FAIL("Unexpected C-MOVE response status");
            }
        }

        // Wait for destination to receive files
        std::this_thread::sleep_for(std::chrono::seconds{1});

        // Verify file was received
        REQUIRE(tracker.count() >= 1);
    }
}

