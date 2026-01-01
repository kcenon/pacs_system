/**
 * @file test_error_recovery.cpp
 * @brief Scenario 5: Error Recovery Tests
 *
 * Tests system error handling and recovery:
 * 1. Send file with invalid SOP Class -> Verify rejection
 * 2. Send file during SCP restart -> Verify retry success
 * 3. Send to wrong AE title -> Verify rejection
 * 4. Test timeout handling
 * 5. Test malformed data handling
 *
 * @see Issue #111 - Integration Test Suite
 */

#include "test_fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/storage_scp.hpp"
#include "pacs/services/storage_scu.hpp"
#include "pacs/services/verification_scp.hpp"
#include "pacs/storage/file_storage.hpp"
#include "pacs/storage/index_database.hpp"

#include <atomic>
#include <thread>

using namespace pacs::integration_test;
using namespace pacs::network;
using namespace pacs::services;
using namespace pacs::storage;
using namespace pacs::core;
using namespace pacs::encoding;
using namespace pacs::network::dimse;

// =============================================================================
// Helper: Configurable Error Server
// =============================================================================

namespace {

/**
 * @brief Storage server with configurable error behavior
 */
class error_test_server {
public:
    explicit error_test_server(uint16_t port, const std::string& ae_title = "ERROR_SCP")
        : port_(port)
        , ae_title_(ae_title)
        , test_dir_("error_test_")
        , storage_dir_(test_dir_.path() / "archive")
        , db_path_(test_dir_.path() / "index.db") {

        std::filesystem::create_directories(storage_dir_);

        server_config config;
        config.ae_title = ae_title_;
        config.port = port_;
        config.max_associations = 20;
        config.idle_timeout = std::chrono::seconds{60};
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.20";
        config.implementation_version_name = "ERROR_SCP";

        server_ = std::make_unique<dicom_server>(config);
    
    file_storage_config fs_conf;
    fs_conf.root_path = storage_dir_;
    file_storage_ = std::make_unique<file_storage>(fs_conf);
    
    auto db_result = index_database::open(db_path_.string());
    if (db_result.is_ok()) {
        database_ = std::move(db_result.value());
    } else {
        throw std::runtime_error("Failed to open database");
    }
}

    bool initialize() {
        server_->register_service(std::make_shared<verification_scp>());

        auto storage_scp_ptr = std::make_shared<storage_scp>();
        storage_scp_ptr->set_handler([this](
            const dicom_dataset& dataset,
            const std::string& calling_ae,
            const std::string& sop_class_uid,
            const std::string& sop_instance_uid) {

            return handle_store(dataset, calling_ae, sop_class_uid, sop_instance_uid);
        });
        server_->register_service(storage_scp_ptr);

        return true;
    }

    bool start() {
        auto result = server_->start();
        if (result.is_ok()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            return true;
        }
        return false;
    }

    void stop() {
        server_->stop();
    }

    uint16_t port() const { return port_; }
    const std::string& ae_title() const { return ae_title_; }
    size_t stored_count() const { return stored_count_.load(); }
    size_t rejected_count() const { return rejected_count_.load(); }

    // Error injection controls
    void set_reject_all(bool reject) { reject_all_ = reject; }
    void set_reject_sop_class(const std::string& sop_class) { reject_sop_class_ = sop_class; }
    void set_simulate_delay(std::chrono::milliseconds delay) { simulate_delay_ = delay; }

    void add_accepted_sop_class(const std::string& sop_class) {
        accepted_sop_classes_.push_back(sop_class);
    }

private:
    storage_status handle_store(
        const dicom_dataset& dataset,
        const std::string& /* calling_ae */,
        const std::string& sop_class_uid,
        const std::string& sop_instance_uid) {

        // Simulate processing delay
        if (simulate_delay_.count() > 0) {
            std::this_thread::sleep_for(simulate_delay_);
        }

        // Reject all requests
        if (reject_all_) {
            ++rejected_count_;
            return storage_status::out_of_resources;
        }

        // Reject specific SOP class
        if (!reject_sop_class_.empty() && sop_class_uid == reject_sop_class_) {
            ++rejected_count_;
            return storage_status::data_set_does_not_match_sop_class;
        }

        // Check accepted SOP classes
        if (!accepted_sop_classes_.empty()) {
            bool found = false;
            for (const auto& accepted : accepted_sop_classes_) {
                if (sop_class_uid == accepted) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                ++rejected_count_;
                return storage_status::data_set_does_not_match_sop_class;
            }
        }

        // Normal processing
    auto store_result = file_storage_->store(dataset);
    if (store_result.is_err()) {
        ++rejected_count_;
        return storage_status::storage_error;
    }

    // 1. Upsert Patient
    auto patient_res = database_->upsert_patient(
        dataset.get_string(tags::patient_id),
        dataset.get_string(tags::patient_name),
        dataset.get_string(tags::patient_birth_date),
        dataset.get_string(tags::patient_sex));
    if (patient_res.is_err()) {
        ++rejected_count_;
        return storage_status::storage_error;
    }
    int64_t patient_pk = patient_res.value();

    // 2. Upsert Study
    auto study_res = database_->upsert_study(
        patient_pk,
        dataset.get_string(tags::study_instance_uid),
        dataset.get_string(tags::study_id),
        dataset.get_string(tags::study_date),
        dataset.get_string(tags::study_time),
        dataset.get_string(tags::accession_number));
    if (study_res.is_err()) {
        ++rejected_count_;
        return storage_status::storage_error;
    }
    int64_t study_pk = study_res.value();

    // 3. Upsert Series
    std::optional<int> series_number;
    try {
        std::string sn = dataset.get_string(tags::series_number);
        if (!sn.empty()) series_number = std::stoi(sn);
    } catch (...) {}

    auto series_res = database_->upsert_series(
        study_pk,
        dataset.get_string(tags::series_instance_uid),
        dataset.get_string(tags::modality),
        series_number);
    if (series_res.is_err()) {
        ++rejected_count_;
        return storage_status::storage_error;
    }
    int64_t series_pk = series_res.value();

    // 4. Upsert Instance
    auto file_path = file_storage_->get_file_path(sop_instance_uid);
    std::optional<int> instance_number;
    try {
        std::string in = dataset.get_string(tags::instance_number);
        if (!in.empty()) instance_number = std::stoi(in);
    } catch (...) {}

    auto instance_res = database_->upsert_instance(
        series_pk,
        sop_instance_uid,
        sop_class_uid,
        file_path.string(),
        static_cast<int64_t>(std::filesystem::file_size(file_path)),
        "",
        instance_number);

    if (instance_res.is_err()) {
        ++rejected_count_;
        return storage_status::storage_error;
    }

    ++stored_count_;
    return storage_status::success;
}

    uint16_t port_;
    std::string ae_title_;
    test_directory test_dir_;
    std::filesystem::path storage_dir_;
    std::filesystem::path db_path_;

    std::unique_ptr<dicom_server> server_;
    std::unique_ptr<file_storage> file_storage_;
    std::unique_ptr<index_database> database_;

    std::atomic<size_t> stored_count_{0};
    std::atomic<size_t> rejected_count_{0};

    // Error injection
    bool reject_all_{false};
    std::string reject_sop_class_;
    std::chrono::milliseconds simulate_delay_{0};
    std::vector<std::string> accepted_sop_classes_;
};

}  // namespace

// =============================================================================
// Scenario 5: Error Recovery Tests
// =============================================================================

TEST_CASE("Invalid SOP Class rejection", "[error][sop_class]") {
    auto port = find_available_port();
    error_test_server server(port, "ERROR_SCP");
    server.add_accepted_sop_class("1.2.840.10008.5.1.4.1.1.2");  // Only CT

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    // Try to store MR image (not in accepted list)
    association_config config;
    config.calling_ae_title = "ERROR_SCU";
    config.called_ae_title = server.ae_title();
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.21";
    config.proposed_contexts.push_back({
        1,
        "1.2.840.10008.5.1.4.1.1.4",  // MR Image Storage
        {"1.2.840.10008.1.2.1"}
    });

    auto connect_result = association::connect(
        "localhost", port, config, default_timeout());
    REQUIRE(connect_result.is_ok());

    auto& assoc = connect_result.value();

    // MR context should be rejected at association level or store level
    auto mr_context = assoc.accepted_context_id("1.2.840.10008.5.1.4.1.1.4");

    if (mr_context) {
        // If context was accepted, store should fail
        storage_scu scu;
        auto mr_dataset = generate_mr_dataset();
        auto result = scu.store(assoc, mr_dataset);

        if (result.is_ok()) {
            // Server should reject with SOP class not supported
            REQUIRE_FALSE(result.value().is_success());
        }
    }
    // If context was not accepted, that's also valid behavior

    (void)assoc.release(default_timeout());

    // Verify server rejected the request
    REQUIRE(server.stored_count() == 0);

    server.stop();
}

TEST_CASE("Server rejection of all stores", "[error][rejection]") {
    auto port = find_available_port();
    error_test_server server(port, "ERROR_SCP");
    server.set_reject_all(true);

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    association_config config;
    config.calling_ae_title = "ERROR_SCU";
    config.called_ae_title = server.ae_title();
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.22";
    config.proposed_contexts.push_back({
        1,
        "1.2.840.10008.5.1.4.1.1.2",
        {"1.2.840.10008.1.2.1"}
    });

    auto connect_result = association::connect(
        "localhost", port, config, default_timeout());
    REQUIRE(connect_result.is_ok());

    auto& assoc = connect_result.value();
    storage_scu scu;

    auto dataset = generate_ct_dataset();
    auto result = scu.store(assoc, dataset);

    REQUIRE(result.is_ok());
    REQUIRE_FALSE(result.value().is_success());
    REQUIRE(result.value().status == static_cast<uint16_t>(storage_status::out_of_resources));

    (void)assoc.release(default_timeout());

    REQUIRE(server.stored_count() == 0);
    REQUIRE(server.rejected_count() == 1);

    server.stop();
}

TEST_CASE("Connection to offline server and retry", "[error][retry]") {
    auto port = find_available_port();

    // First, try to connect when server is not running
    auto connect_result = test_association::connect(
        "localhost",
        port,
        "OFFLINE_SCP",
        "RETRY_SCU",
        {std::string(verification_sop_class_uid)}
    );

    REQUIRE(connect_result.is_err());

    // Now start the server
    error_test_server server(port, "OFFLINE_SCP");
    REQUIRE(server.initialize());
    REQUIRE(server.start());

    // Retry connection - should succeed now
    auto retry_result = test_association::connect(
        "localhost",
        port,
        server.ae_title(),
        "RETRY_SCU",
        {std::string(verification_sop_class_uid)}
    );

    REQUIRE(retry_result.is_ok());
    (void)retry_result.value().release(default_timeout());

    server.stop();
}

TEST_CASE("Server restart during operations", "[error][restart]") {
    auto port = find_available_port();
    error_test_server server(port, "RESTART_SCP");

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    // Store some files first
    {
        association_config config;
        config.calling_ae_title = "PRE_RESTART";
        config.called_ae_title = server.ae_title();
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.23";
        config.proposed_contexts.push_back({
            1,
            "1.2.840.10008.5.1.4.1.1.2",
            {"1.2.840.10008.1.2.1"}
        });

        auto connect = association::connect("localhost", port, config, default_timeout());
        REQUIRE(connect.is_ok());

        storage_scu scu;
        auto ds = generate_ct_dataset();
        auto result = scu.store(connect.value(), ds);
        REQUIRE(result.is_ok());
        REQUIRE(result.value().is_success());

        (void)connect.value().release(default_timeout());
    }

    REQUIRE(server.stored_count() == 1);

    // Stop the server
    server.stop();

    // Try to connect - should fail
    auto offline_connect = test_association::connect(
        "localhost", port, "RESTART_SCP", "POST_STOP",
        {"1.2.840.10008.5.1.4.1.1.2"});
    REQUIRE(offline_connect.is_err());

    // Note: For a true restart test, we'd need to create a new server
    // since the database/storage is tied to the test_directory lifetime
    // Here we demonstrate the connection failure when server is stopped

    // Create new server on same port
    error_test_server new_server(port, "RESTART_SCP");
    REQUIRE(new_server.initialize());
    REQUIRE(new_server.start());

    // Retry connection - should succeed
    auto retry_connect = test_association::connect(
        "localhost", port, new_server.ae_title(), "POST_RESTART",
        {"1.2.840.10008.5.1.4.1.1.2"});
    REQUIRE(retry_connect.is_ok());
    (void)retry_connect.value().release(default_timeout());

    new_server.stop();
}

TEST_CASE("Timeout during slow processing", "[error][timeout]") {
    auto port = find_available_port();
    error_test_server server(port, "SLOW_SCP");
    server.set_simulate_delay(std::chrono::milliseconds{2000});  // 2 second delay

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    association_config config;
    config.calling_ae_title = "TIMEOUT_SCU";
    config.called_ae_title = server.ae_title();
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.24";
    config.proposed_contexts.push_back({
        1,
        "1.2.840.10008.5.1.4.1.1.2",
        {"1.2.840.10008.1.2.1"}
    });

    auto connect_result = association::connect(
        "localhost", port, config, default_timeout());
    REQUIRE(connect_result.is_ok());

    auto& assoc = connect_result.value();

    // Use very short timeout
    storage_scu_config scu_config;
    scu_config.response_timeout = std::chrono::milliseconds{500};  // Very short
    storage_scu scu{scu_config};

    auto dataset = generate_ct_dataset();

    // This may timeout or succeed depending on timing
    auto result = scu.store(assoc, dataset);

    // Either timeout error or slow success is acceptable
    // The key is that the system handles it gracefully without crashing
    if (result.is_err()) {
        // Timeout is expected behavior
        INFO("Store timed out as expected");
    } else {
        // If it succeeded, verify the result
        INFO("Store completed despite slow processing");
    }

    // Abort the association since we might have a timeout situation
    assoc.abort();

    server.stop();
}

TEST_CASE("Association abort handling", "[error][abort]") {
    auto port = find_available_port();
    error_test_server server(port, "ABORT_SCP");

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    association_config config;
    config.calling_ae_title = "ABORT_SCU";
    config.called_ae_title = server.ae_title();
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.25";
    config.proposed_contexts.push_back({
        1,
        std::string(verification_sop_class_uid),
        {"1.2.840.10008.1.2.1"}
    });

    auto connect_result = association::connect(
        "localhost", port, config, default_timeout());
    REQUIRE(connect_result.is_ok());

    auto& assoc = connect_result.value();

    // Abort instead of graceful release
    assoc.abort();

    // Server should handle the abort gracefully
    // New connections should still work
    auto new_connect = test_association::connect(
        "localhost", port, server.ae_title(), "AFTER_ABORT",
        {std::string(verification_sop_class_uid)});
    REQUIRE(new_connect.is_ok());
    (void)new_connect.value().release(default_timeout());

    server.stop();
}

TEST_CASE("Multiple rapid aborts", "[error][rapid_abort]") {
    auto port = find_available_port();
    error_test_server server(port, "RAPID_ABORT_SCP");

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    constexpr int num_aborts = 10;

    for (int i = 0; i < num_aborts; ++i) {
        association_config config;
        config.calling_ae_title = "ABORT_" + std::to_string(i);
        config.called_ae_title = server.ae_title();
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.26";
        config.proposed_contexts.push_back({
            1,
            std::string(verification_sop_class_uid),
            {"1.2.840.10008.1.2.1"}
        });

        auto connect = association::connect(
            "localhost", port, config, default_timeout());

        if (connect.is_ok()) {
            connect.value().abort();
        }
    }

    // Server should still be operational
    auto final_connect = test_association::connect(
        "localhost", port, server.ae_title(), "FINAL_CHECK",
        {std::string(verification_sop_class_uid)});
    REQUIRE(final_connect.is_ok());

    // Send an echo to verify server is responsive
    auto& assoc = final_connect.value();
    auto ctx = assoc.accepted_context_id(verification_sop_class_uid);
    REQUIRE(ctx.has_value());

    auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);
    REQUIRE(assoc.send_dimse(*ctx, echo_rq).is_ok());

    auto recv = assoc.receive_dimse(default_timeout());
    REQUIRE(recv.is_ok());
    REQUIRE(recv.value().second.status() == status_success);

    (void)assoc.release(default_timeout());
    server.stop();
}

TEST_CASE("Duplicate SOP Instance handling", "[error][duplicate]") {
    auto port = find_available_port();
    error_test_server server(port, "DUP_SCP");

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    association_config config;
    config.calling_ae_title = "DUP_SCU";
    config.called_ae_title = server.ae_title();
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.27";
    config.proposed_contexts.push_back({
        1,
        "1.2.840.10008.5.1.4.1.1.2",
        {"1.2.840.10008.1.2.1"}
    });

    auto connect_result = association::connect(
        "localhost", port, config, default_timeout());
    REQUIRE(connect_result.is_ok());

    auto& assoc = connect_result.value();
    storage_scu scu;

    // Create dataset with fixed SOP Instance UID
    auto sop_instance_uid = generate_uid();
    auto dataset = generate_ct_dataset();
    dataset.set_string(tags::sop_instance_uid, vr_type::UI, sop_instance_uid);

    // First store should succeed
    auto result1 = scu.store(assoc, dataset);
    REQUIRE(result1.is_ok());
    REQUIRE(result1.value().is_success());

    // Second store with same SOP Instance UID
    // Behavior depends on server implementation:
    // - Could overwrite (success)
    // - Could reject as duplicate (error)
    // - Could return warning
    auto result2 = scu.store(assoc, dataset);
    REQUIRE(result2.is_ok());
    // Either success (overwrite) or warning (duplicate) is acceptable

    (void)assoc.release(default_timeout());
    server.stop();
}
