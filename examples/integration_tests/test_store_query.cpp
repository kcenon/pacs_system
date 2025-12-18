/**
 * @file test_store_query.cpp
 * @brief Scenario 2: Store and Query Workflow Tests
 *
 * Tests the complete storage and query workflow:
 * 1. Start PACS Server
 * 2. Store DICOM files via Storage SCU
 * 3. Query via Query SCU -> Verify results
 * 4. Retrieve via Retrieve SCU -> Verify files match
 * 5. Stop PACS Server
 *
 * @see Issue #111 - Integration Test Suite
 */

#include "test_fixtures.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/query_scp.hpp"
#include "pacs/services/retrieve_scp.hpp"
#include "pacs/services/storage_scp.hpp"
#include "pacs/services/storage_scu.hpp"
#include "pacs/services/verification_scp.hpp"
#include "pacs/storage/file_storage.hpp"
#include "pacs/storage/index_database.hpp"

#include <memory>

using namespace pacs::integration_test;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::services;
using namespace pacs::storage;
using namespace pacs::core;
using namespace pacs::encoding;

// =============================================================================
// Helper: Simple PACS Server for Testing
// =============================================================================

namespace {

/**
 * @brief Simplified PACS server for integration testing
 *
 * Provides storage, query, and retrieve services backed by
 * in-memory or file-based storage.
 */
class simple_pacs_server {
public:
    explicit simple_pacs_server(uint16_t port, const std::string& ae_title = "TEST_PACS")
        : port_(port)
        , ae_title_(ae_title)
        , test_dir_("pacs_server_test_")
        , storage_dir_(test_dir_.path() / "archive")
        , db_path_(test_dir_.path() / "index.db") {

        std::filesystem::create_directories(storage_dir_);

        // Initialize server
        server_config config;
        config.ae_title = ae_title_;
        config.port = port_;
        config.max_associations = 20;
        config.idle_timeout = std::chrono::seconds{60};
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.1";
        config.implementation_version_name = "TEST_PACS";

        server_ = std::make_unique<dicom_server>(config);

        // Initialize storage and database
        file_storage_config fs_config;
        fs_config.root_path = storage_dir_;
        file_storage_ = std::make_unique<file_storage>(fs_config);
        auto db_result = index_database::open(db_path_.string());
        if (db_result.is_err()) {
            throw std::runtime_error("Failed to open database: " + db_result.error().message);
        }
        database_ = std::move(db_result.value());
    }

    bool initialize() {
        // Register services
        server_->register_service(std::make_shared<verification_scp>());

        // Storage SCP with callback to store files
        auto storage_scp_ptr = std::make_shared<storage_scp>();
        storage_scp_ptr->set_handler([this](
            const dicom_dataset& dataset,
            const std::string& calling_ae,
            const std::string& sop_class_uid,
            const std::string& sop_instance_uid) -> storage_status {

            return handle_store(dataset, calling_ae, sop_class_uid, sop_instance_uid);
        });
        server_->register_service(storage_scp_ptr);

        // Query SCP with callback
        auto query_scp_ptr = std::make_shared<query_scp>();
        query_scp_ptr->set_handler([this](
            query_level level,
            const dicom_dataset& query_keys,
            const std::string& calling_ae) -> std::vector<dicom_dataset> {

            return handle_query(level, query_keys, calling_ae);
        });
        server_->register_service(query_scp_ptr);

        // Retrieve SCP with callback
        auto retrieve_scp_ptr = std::make_shared<retrieve_scp>();
        retrieve_scp_ptr->set_retrieve_handler([this](
            const dicom_dataset& query_keys) -> std::vector<dicom_file> {

            return handle_retrieve(query_keys);
        });
        server_->register_service(retrieve_scp_ptr);

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
    size_t stored_count() const { return stored_count_; }

private:
    storage_status handle_store(
        const dicom_dataset& dataset,
        const std::string& /* calling_ae */,
        const std::string& /* sop_class_uid */,
        const std::string& /* sop_instance_uid */) {

        // Store to filesystem
        auto store_result = file_storage_->store(dataset);
        if (store_result.is_err()) {
            return storage_status::storage_error;
        }

        // Index in database
        // 1. Patient
        auto pat_id = dataset.get_string(tags::patient_id);
        auto pat_name = dataset.get_string(tags::patient_name);
        auto pat_birth = dataset.get_string(tags::patient_birth_date);
        auto pat_sex = dataset.get_string(tags::patient_sex);
        
        auto pat_res = database_->upsert_patient(pat_id, pat_name, pat_birth, pat_sex);
        if (pat_res.is_err()) return storage_status::storage_error;
        auto pat_pk = pat_res.value();

        // 2. Study
        auto study_uid = dataset.get_string(tags::study_instance_uid);
        auto study_res = database_->upsert_study(pat_pk, study_uid);
        if (study_res.is_err()) return storage_status::storage_error;
        auto study_pk = study_res.value();

        // 3. Series
        auto series_uid = dataset.get_string(tags::series_instance_uid);
        auto series_res = database_->upsert_series(study_pk, series_uid);
        if (series_res.is_err()) return storage_status::storage_error;
        auto series_pk = series_res.value();

        // 4. Instance
        auto sop_uid = dataset.get_string(tags::sop_instance_uid);
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        auto file_path = file_storage_->get_file_path(sop_uid).string();
        
        std::error_code ec;
        auto file_size = std::filesystem::file_size(file_path, ec);
        if (ec) file_size = 0;

        auto inst_res = database_->upsert_instance(series_pk, sop_uid, sop_class, file_path, static_cast<int64_t>(file_size));
        if (inst_res.is_err()) return storage_status::storage_error;

        ++stored_count_;
        return storage_status::success;
    }

    std::vector<dicom_dataset> handle_query(
        query_level level,
        const dicom_dataset& query_keys,
        const std::string& /* calling_ae */) {

        std::vector<dicom_dataset> results;

        if (level == query_level::study) {
            study_query query;
            auto study_uid_val = query_keys.get_string(tags::study_instance_uid);
            if (!study_uid_val.empty()) query.study_uid = study_uid_val;
            
            auto pat_id_val = query_keys.get_string(tags::patient_id);
            if (!pat_id_val.empty()) query.patient_id = pat_id_val;
            
            auto pat_name_val = query_keys.get_string(tags::patient_name);
            if (!pat_name_val.empty()) query.patient_name = pat_name_val;

            auto studies_result = database_->search_studies(query);
            if (studies_result.is_ok()) {
                for (const auto& study : studies_result.value()) {
                    dicom_dataset ds;
                    ds.set_string(tags::study_instance_uid, vr_type::UI, study.study_uid);
                    ds.set_string(tags::study_id, vr_type::SH, study.study_id);
                    ds.set_string(tags::study_date, vr_type::DA, study.study_date);
                    ds.set_string(tags::study_time, vr_type::TM, study.study_time);
                    ds.set_string(tags::accession_number, vr_type::SH, study.accession_number);
                    ds.set_string(tags::study_description, vr_type::LO, study.study_description);
                    ds.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");

                    auto patient = database_->find_patient_by_pk(study.patient_pk);
                    if (patient) {
                        ds.set_string(tags::patient_name, vr_type::PN, patient->patient_name);
                        ds.set_string(tags::patient_id, vr_type::LO, patient->patient_id);
                        ds.set_string(tags::patient_birth_date, vr_type::DA, patient->birth_date);
                        ds.set_string(tags::patient_sex, vr_type::CS, patient->sex);
                    }

                    results.push_back(std::move(ds));
                }
            }
        }
        return results;
    }

    std::vector<dicom_file> handle_retrieve(const dicom_dataset& query_keys) {
        std::vector<dicom_file> results;

        auto study_uid = query_keys.get_string(tags::study_instance_uid);
        if (!study_uid.empty()) {
            auto series_list_result = database_->list_series(study_uid);
            if (series_list_result.is_ok()) {
                for (const auto& series : series_list_result.value()) {
                    auto instance_list_result = database_->list_instances(series.series_uid);
                    if (instance_list_result.is_ok()) {
                        for (const auto& inst : instance_list_result.value()) {
                            auto path = file_storage_->get_file_path(inst.sop_uid);
                            auto file_result = dicom_file::open(path);
                            if (file_result.is_ok()) {
                                results.push_back(std::move(file_result.value()));
                            }
                        }
                    }
                }
            }
        }
        return results;
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
};

}  // namespace

// =============================================================================
// Scenario 2: Store and Query
// =============================================================================

TEST_CASE("Store single DICOM file and query", "[store_query][basic]") {
    auto port = find_available_port();
    simple_pacs_server server(port, "TEST_PACS");

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    SECTION("Store CT image and query at study level") {
        // Generate test dataset
        auto study_uid = generate_uid();
        auto dataset = generate_ct_dataset(study_uid);

        // Configure association for storage
        association_config config;
        config.calling_ae_title = "STORE_SCU";
        config.called_ae_title = server.ae_title();
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.2";
        config.proposed_contexts.push_back({
            1,
            "1.2.840.10008.5.1.4.1.1.2",  // CT Image Storage
            {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
        });

        auto connect_result = association::connect(
            "localhost", port, config, default_timeout);
        REQUIRE(connect_result.is_ok());

        auto& assoc = connect_result.value();

        // Create storage SCU and send
        storage_scu_config scu_config;
        scu_config.response_timeout = default_timeout;
        storage_scu scu{scu_config};

        auto store_result = scu.store(assoc, dataset);
        REQUIRE(store_result.is_ok());
        REQUIRE(store_result.value().is_success());

        (void)assoc.release(default_timeout);

        // Verify stored count
        REQUIRE(server.stored_count() == 1);

        // Query the stored study
        association_config query_config;
        query_config.calling_ae_title = "QUERY_SCU";
        query_config.called_ae_title = server.ae_title();
        query_config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.3";
        query_config.proposed_contexts.push_back({
            1,
            std::string(study_root_find_sop_class_uid),
            {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
        });

        auto query_connect = association::connect(
            "localhost", port, query_config, default_timeout);
        REQUIRE(query_connect.is_ok());

        auto& query_assoc = query_connect.value();

        // Create query keys
        dicom_dataset query_keys;
        query_keys.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");
        query_keys.set_string(tags::study_instance_uid, vr_type::UI, study_uid);
        query_keys.set_string(tags::patient_name, vr_type::PN, "");  // Return all

        auto context_id = *query_assoc.accepted_context_id(
            study_root_find_sop_class_uid);

        // Send C-FIND
        auto find_rq = make_c_find_rq(
            1,
            study_root_find_sop_class_uid
        );
        find_rq.set_dataset(std::move(query_keys));
        auto send_result = query_assoc.send_dimse(context_id, find_rq);
        REQUIRE(send_result.is_ok());

        // Receive responses
        std::vector<dicom_dataset> query_results;
        while (true) {
            auto recv_result = query_assoc.receive_dimse(default_timeout);
            REQUIRE(recv_result.is_ok());

            auto& [recv_ctx, rsp] = recv_result.value();
            if (rsp.status() == status_success) {
                break;  // Final response
            } else if (rsp.status() == status_pending) {
                if (rsp.has_dataset()) {
                    query_results.push_back(rsp.dataset());
                }
            } else {
                FAIL("Unexpected query status");
            }
        }

        REQUIRE(query_results.size() == 1);
        REQUIRE(query_results[0].get_string(tags::study_instance_uid) == study_uid);

        (void)query_assoc.release(default_timeout);
    }

    server.stop();
}

TEST_CASE("Store multiple files from same study", "[store_query][multi]") {
    auto port = find_available_port();
    simple_pacs_server server(port, "TEST_PACS");

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    // Generate test data - multiple images in same study
    auto study_uid = generate_uid();
    auto series_uid = generate_uid();
    constexpr int num_images = 5;

    std::vector<dicom_dataset> datasets;
    for (int i = 0; i < num_images; ++i) {
        auto ds = generate_ct_dataset(study_uid, series_uid);
        ds.set_string(tags::instance_number, vr_type::IS, std::to_string(i + 1));
        datasets.push_back(std::move(ds));
    }

    // Store all images
    association_config config;
    config.calling_ae_title = "STORE_SCU";
    config.called_ae_title = server.ae_title();
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.2";
    config.proposed_contexts.push_back({
        1,
        "1.2.840.10008.5.1.4.1.1.2",  // CT Image Storage
        {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
    });

    auto connect_result = association::connect(
        "localhost", port, config, default_timeout);
    REQUIRE(connect_result.is_ok());

    auto& assoc = connect_result.value();

    storage_scu_config scu_config;
    storage_scu scu{scu_config};

    for (const auto& ds : datasets) {
        auto result = scu.store(assoc, ds);
        REQUIRE(result.is_ok());
        REQUIRE(result.value().is_success());
    }

    (void)assoc.release(default_timeout);

    REQUIRE(server.stored_count() == num_images);

    // Query at series level - should return 1 series with num_images
    association_config query_config;
    query_config.calling_ae_title = "QUERY_SCU";
    query_config.called_ae_title = server.ae_title();
    query_config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.3";
    query_config.proposed_contexts.push_back({
        1,
        std::string(study_root_find_sop_class_uid),
        {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
    });

    auto query_connect = association::connect(
        "localhost", port, query_config, default_timeout);
    REQUIRE(query_connect.is_ok());

    auto& query_assoc = query_connect.value();

    dicom_dataset query_keys;
    query_keys.set_string(tags::query_retrieve_level, vr_type::CS, "SERIES");
    query_keys.set_string(tags::study_instance_uid, vr_type::UI, study_uid);
    query_keys.set_string(tags::series_instance_uid, vr_type::UI, "");
    query_keys.set_string(tags::number_of_series_related_instances, vr_type::IS, "");

    auto context_id = *query_assoc.accepted_context_id(
        study_root_find_sop_class_uid);

    auto find_rq = make_c_find_rq(1, study_root_find_sop_class_uid);
    find_rq.set_dataset(std::move(query_keys));
    (void)query_assoc.send_dimse(context_id, find_rq);

    std::vector<dicom_dataset> results;
    while (true) {
        auto recv_result = query_assoc.receive_dimse(default_timeout);
        if (recv_result.is_err()) break;

        auto& [recv_ctx, rsp] = recv_result.value();
        if (rsp.status() == status_success) break;
        if (rsp.status() == status_pending && rsp.has_dataset()) {
            results.push_back(rsp.dataset());
        }
    }

    REQUIRE(results.size() == 1);
    REQUIRE(results[0].get_string(tags::series_instance_uid) == series_uid);

    // Check number of instances in series
    auto num_instances_str = results[0].get_string(tags::number_of_series_related_instances);
    if (!num_instances_str.empty()) {
        REQUIRE(std::stoi(num_instances_str) == num_images);
    }

    (void)query_assoc.release(default_timeout);
    server.stop();
}

TEST_CASE("Store files from multiple modalities", "[store_query][modality]") {
    auto port = find_available_port();
    simple_pacs_server server(port, "TEST_PACS");

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    // Store CT and MR images
    auto ct_dataset = generate_ct_dataset();
    auto mr_dataset = generate_mr_dataset();

    association_config config;
    config.calling_ae_title = "STORE_SCU";
    config.called_ae_title = server.ae_title();
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.2";
    config.proposed_contexts.push_back({
        1,
        "1.2.840.10008.5.1.4.1.1.2",  // CT Image Storage
        {"1.2.840.10008.1.2.1"}
    });
    config.proposed_contexts.push_back({
        3,
        "1.2.840.10008.5.1.4.1.1.4",  // MR Image Storage
        {"1.2.840.10008.1.2.1"}
    });

    auto connect_result = association::connect(
        "localhost", port, config, default_timeout);
    REQUIRE(connect_result.is_ok());

    auto& assoc = connect_result.value();
    storage_scu scu;

    auto ct_result = scu.store(assoc, ct_dataset);
    REQUIRE(ct_result.is_ok());

    auto mr_result = scu.store(assoc, mr_dataset);
    REQUIRE(mr_result.is_ok());

    (void)assoc.release(default_timeout);

    REQUIRE(server.stored_count() == 2);

    // Query by modality
    association_config query_config;
    query_config.calling_ae_title = "QUERY_SCU";
    query_config.called_ae_title = server.ae_title();
    query_config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.3";
    query_config.proposed_contexts.push_back({
        1,
        std::string(study_root_find_sop_class_uid),
        {"1.2.840.10008.1.2.1"}
    });

    auto query_connect = association::connect(
        "localhost", port, query_config, default_timeout);
    REQUIRE(query_connect.is_ok());

    auto& query_assoc = query_connect.value();

    // Query for CT studies only
    dicom_dataset ct_query;
    ct_query.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");
    ct_query.set_string(tags::modalities_in_study, vr_type::CS, "CT");
    ct_query.set_string(tags::study_instance_uid, vr_type::UI, "");

    auto context_id = *query_assoc.accepted_context_id(
        study_root_find_sop_class_uid);

    auto find_rq = make_c_find_rq(1, study_root_find_sop_class_uid);
    find_rq.set_dataset(std::move(ct_query));
    (void)query_assoc.send_dimse(context_id, find_rq);

    std::vector<dicom_dataset> ct_results;
    while (true) {
        auto recv_result = query_assoc.receive_dimse(default_timeout);
        if (recv_result.is_err()) break;

        auto& [recv_ctx, rsp] = recv_result.value();
        if (rsp.status() == status_success) break;
        if (rsp.status() == status_pending && rsp.has_dataset()) {
            ct_results.push_back(rsp.dataset());
        }
    }

    // Should find exactly 1 CT study
    REQUIRE(ct_results.size() == 1);

    (void)query_assoc.release(default_timeout);
    server.stop();
}

TEST_CASE("Query with wildcards", "[store_query][wildcard]") {
    auto port = find_available_port();
    simple_pacs_server server(port, "TEST_PACS");

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    // Store multiple patients
    std::vector<std::string> patient_names = {
        "SMITH^JOHN", "SMITH^JANE", "JONES^WILLIAM"
    };

    association_config config;
    config.calling_ae_title = "STORE_SCU";
    config.called_ae_title = server.ae_title();
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.2";
    config.proposed_contexts.push_back({
        1,
        "1.2.840.10008.5.1.4.1.1.2",
        {"1.2.840.10008.1.2.1"}
    });

    auto connect_result = association::connect(
        "localhost", port, config, default_timeout);
    REQUIRE(connect_result.is_ok());

    auto& assoc = connect_result.value();
    storage_scu scu;

    for (const auto& name : patient_names) {
        auto ds = generate_ct_dataset();
        ds.set_string(tags::patient_name, vr_type::PN, name);
        ds.set_string(tags::patient_id, vr_type::LO, "PID_" + name.substr(0, 5));

        auto result = scu.store(assoc, ds);
        REQUIRE(result.is_ok());
    }

    (void)assoc.release(default_timeout);

    // Query with wildcard
    association_config query_config;
    query_config.calling_ae_title = "QUERY_SCU";
    query_config.called_ae_title = server.ae_title();
    query_config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.3";
    query_config.proposed_contexts.push_back({
        1,
        std::string(study_root_find_sop_class_uid),
        {"1.2.840.10008.1.2.1"}
    });

    auto query_connect = association::connect(
        "localhost", port, query_config, default_timeout);
    REQUIRE(query_connect.is_ok());

    auto& query_assoc = query_connect.value();

    // Query for all SMITH patients
    dicom_dataset query_keys;
    query_keys.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");
    query_keys.set_string(tags::patient_name, vr_type::PN, "SMITH*");
    query_keys.set_string(tags::study_instance_uid, vr_type::UI, "");

    auto context_id = *query_assoc.accepted_context_id(
        study_root_find_sop_class_uid);

    auto find_rq = make_c_find_rq(1, study_root_find_sop_class_uid);
    find_rq.set_dataset(std::move(query_keys));
    (void)query_assoc.send_dimse(context_id, find_rq);

    std::vector<dicom_dataset> results;
    while (true) {
        auto recv_result = query_assoc.receive_dimse(default_timeout);
        if (recv_result.is_err()) break;

        auto& [recv_ctx, rsp] = recv_result.value();
        if (rsp.status() == status_success) break;
        if (rsp.status() == status_pending && rsp.has_dataset()) {
            results.push_back(rsp.dataset());
        }
    }

    // Should find 2 SMITH patients
    REQUIRE(results.size() == 2);

    (void)query_assoc.release(default_timeout);
    server.stop();
}
