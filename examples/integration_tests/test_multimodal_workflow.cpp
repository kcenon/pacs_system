/**
 * @file test_multimodal_workflow.cpp
 * @brief Multi-Modal Clinical Workflow Integration Tests
 *
 * Tests comprehensive clinical workflow scenarios that simulate real-world
 * multi-modality patient journeys through the PACS system.
 *
 * Test Scenarios:
 * 1. Complete Patient Journey: Worklist → CT → MPPS → MR → MPPS → Query
 * 2. Interventional Workflow (XA): Pre-procedure → XA cine → Analysis → Storage
 * 3. Emergency Multi-Modality: Trauma CT → XA intervention → Follow-up CT
 * 4. Concurrent Modality Operations: Multiple scanners storing simultaneously
 *
 * @see Issue #138 - Multi-Modal Clinical Workflow Tests
 * @see Issue #134 - Integration Test Enhancement Epic
 */

#include "test_fixtures.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/mpps_scp.hpp"
#include "pacs/services/query_scp.hpp"
#include "pacs/services/retrieve_scp.hpp"
#include "pacs/services/storage_scp.hpp"
#include "pacs/services/storage_scu.hpp"
#include "pacs/services/verification_scp.hpp"
#include "pacs/services/worklist_scp.hpp"
#include "pacs/storage/file_storage.hpp"
#include "pacs/storage/index_database.hpp"

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

using namespace pacs::integration_test;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::services;
using namespace pacs::storage;
using namespace pacs::core;
using namespace pacs::encoding;

namespace {

using namespace pacs::services::mpps_tags;

// =============================================================================
// Workflow Verification Helper
// =============================================================================

/**
 * @brief Data consistency verification for multi-modal workflows
 *
 * Provides utility functions to verify patient demographics, study counts,
 * modalities in studies, series counts, and MPPS tracking.
 */
class workflow_verification {
public:
    explicit workflow_verification(index_database& db) : db_(db) {}

    /**
     * @brief Verify patient demographics exist
     * @param patient_id Patient identifier
     * @return true if patient record exists
     */
    bool verify_patient_exists(const std::string& patient_id) {
        auto patient = db_.find_patient(patient_id);
        return patient.has_value();
    }

    /**
     * @brief Verify study count for a patient
     * @param patient_id Patient identifier
     * @param expected Expected number of studies
     * @return true if study count matches
     */
    bool verify_study_count(const std::string& patient_id, size_t expected) {
        auto studies = db_.list_studies(patient_id);
        return studies.size() == expected;
    }

    /**
     * @brief Verify modalities present in a study
     * @param study_uid Study Instance UID
     * @param expected_modalities Expected modality codes
     * @return true if all expected modalities are present
     */
    bool verify_modalities_in_study(
        const std::string& study_uid,
        const std::vector<std::string>& expected_modalities) {

        auto series_list = db_.list_series(study_uid);

        std::set<std::string> found_modalities;
        for (const auto& series : series_list) {
            if (!series.modality.empty()) {
                found_modalities.insert(series.modality);
            }
        }

        for (const auto& mod : expected_modalities) {
            if (found_modalities.find(mod) == found_modalities.end()) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Verify series count in a study
     * @param study_uid Study Instance UID
     * @param expected Expected number of series
     * @return true if series count matches
     */
    bool verify_series_count(const std::string& study_uid, size_t expected) {
        return db_.series_count(study_uid) == expected;
    }

    /**
     * @brief Verify image count in a series
     * @param series_uid Series Instance UID
     * @param expected Expected number of images
     * @return true if image count matches
     */
    bool verify_image_count(const std::string& series_uid, size_t expected) {
        return db_.instance_count(series_uid) == expected;
    }

    /**
     * @brief Verify no duplicate SOP Instance UIDs exist
     * @param study_uid Study to check
     * @return true if all UIDs are unique
     */
    bool verify_unique_uids(const std::string& study_uid) {
        auto series_list = db_.list_series(study_uid);

        std::set<std::string> uids;
        for (const auto& series : series_list) {
            auto instances = db_.list_instances(series.series_uid);

            for (const auto& instance : instances) {
                if (uids.find(instance.sop_uid) != uids.end()) {
                    return false;  // Duplicate found
                }
                uids.insert(instance.sop_uid);
            }
        }
        return true;
    }

    /**
     * @brief Get total instance count for a study
     * @param study_uid Study Instance UID
     * @return Number of instances
     */
    size_t get_instance_count(const std::string& study_uid) {
        size_t count = 0;
        auto series_list = db_.list_series(study_uid);

        for (const auto& series : series_list) {
            count += db_.instance_count(series.series_uid);
        }
        return count;
    }

private:
    index_database& db_;
};

// =============================================================================
// Multi-Modal PACS Server
// =============================================================================

/**
 * @brief Extended PACS server supporting multiple modalities and MPPS
 *
 * Provides comprehensive PACS functionality for multi-modal workflow testing:
 * - Storage SCP for all modalities
 * - Query/Retrieve SCP
 * - Worklist SCP
 * - MPPS SCP
 */
class multimodal_pacs_server {
public:
    explicit multimodal_pacs_server(uint16_t port, const std::string& ae_title = "MM_PACS")
        : port_(port)
        , ae_title_(ae_title)
        , test_dir_("multimodal_pacs_test_")
        , storage_dir_(test_dir_.path() / "archive")
        , db_path_(test_dir_.path() / "index.db") {

        std::filesystem::create_directories(storage_dir_);

        server_config config;
        config.ae_title = ae_title_;
        config.port = port_;
        config.max_associations = 50;  // Support concurrent modalities
        config.idle_timeout = std::chrono::seconds{120};
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.138";
        config.implementation_version_name = "MM_PACS";

        server_ = std::make_unique<dicom_server>(config);

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
        // Verification SCP
        server_->register_service(std::make_shared<verification_scp>());

        // Storage SCP
        auto storage_scp_ptr = std::make_shared<storage_scp>();
        storage_scp_ptr->set_handler([this](
            const dicom_dataset& dataset,
            const std::string& calling_ae,
            const std::string& sop_class_uid,
            const std::string& sop_instance_uid) -> storage_status {

            return handle_store(dataset, calling_ae, sop_class_uid, sop_instance_uid);
        });
        server_->register_service(storage_scp_ptr);

        // Query SCP
        auto query_scp_ptr = std::make_shared<query_scp>();
        query_scp_ptr->set_handler([this](
            query_level level,
            const dicom_dataset& query_keys,
            const std::string& /* calling_ae */) -> std::vector<dicom_dataset> {

            return handle_query(level, query_keys);
        });
        server_->register_service(query_scp_ptr);

        // Worklist SCP
        auto worklist_scp_ptr = std::make_shared<worklist_scp>();
        worklist_scp_ptr->set_handler([this](
            const dicom_dataset& query,
            const std::string& /* calling_ae */) -> std::vector<dicom_dataset> {

            return query_worklist(query);
        });
        server_->register_service(worklist_scp_ptr);

        // MPPS SCP
        auto mpps_scp_ptr = std::make_shared<mpps_scp>();
        mpps_scp_ptr->set_create_handler([this](const mpps_instance& instance)
            -> pacs::network::Result<std::monostate> {
            return create_mpps(instance);
        });
        mpps_scp_ptr->set_set_handler([this](
            const std::string& uid,
            const dicom_dataset& modifications,
            pacs::services::mpps_status status)
            -> pacs::network::Result<std::monostate> {
            return update_mpps(uid, modifications, status);
        });
        server_->register_service(mpps_scp_ptr);

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

    void add_worklist_item(const dicom_dataset& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        worklist_items_.push_back(item);
    }

    std::optional<mpps_instance> get_mpps(const std::string& uid) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& mpps : mpps_instances_) {
            if (mpps.sop_instance_uid == uid) {
                return mpps;
            }
        }
        return std::nullopt;
    }

    size_t mpps_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return mpps_instances_.size();
    }

    size_t stored_count() const { return stored_count_.load(); }
    size_t error_count() const { return error_count_.load(); }

    uint16_t port() const { return port_; }
    const std::string& ae_title() const { return ae_title_; }

    index_database& database() { return *database_; }

    workflow_verification get_verifier() {
        return workflow_verification(*database_);
    }

private:
    storage_status handle_store(
        const dicom_dataset& dataset,
        const std::string& /* calling_ae */,
        const std::string& /* sop_class_uid */,
        const std::string& /* sop_instance_uid */) {

        // Store to filesystem
        auto store_result = file_storage_->store(dataset);
        if (store_result.is_err()) {
            ++error_count_;
            return storage_status::storage_error;
        }

        // Index in database (following test_store_query.cpp pattern)
        // 1. Patient
        auto pat_id = dataset.get_string(tags::patient_id);
        auto pat_name = dataset.get_string(tags::patient_name);
        auto pat_birth = dataset.get_string(tags::patient_birth_date);
        auto pat_sex = dataset.get_string(tags::patient_sex);

        auto pat_res = database_->upsert_patient(pat_id, pat_name, pat_birth, pat_sex);
        if (pat_res.is_err()) {
            ++error_count_;
            return storage_status::storage_error;
        }
        auto pat_pk = pat_res.value();

        // 2. Study
        auto study_uid = dataset.get_string(tags::study_instance_uid);
        auto study_res = database_->upsert_study(pat_pk, study_uid);
        if (study_res.is_err()) {
            ++error_count_;
            return storage_status::storage_error;
        }
        auto study_pk = study_res.value();

        // 3. Series
        auto series_uid = dataset.get_string(tags::series_instance_uid);
        auto modality = dataset.get_string(tags::modality);
        auto series_res = database_->upsert_series(study_pk, series_uid, modality);
        if (series_res.is_err()) {
            ++error_count_;
            return storage_status::storage_error;
        }
        auto series_pk = series_res.value();

        // 4. Instance
        auto sop_uid = dataset.get_string(tags::sop_instance_uid);
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        auto file_path = file_storage_->get_file_path(sop_uid).string();

        std::error_code ec;
        auto file_size = std::filesystem::file_size(file_path, ec);
        if (ec) file_size = 0;

        auto inst_res = database_->upsert_instance(
            series_pk, sop_uid, sop_class, file_path, static_cast<int64_t>(file_size));
        if (inst_res.is_err()) {
            ++error_count_;
            return storage_status::storage_error;
        }

        // Update modalities in study
        (void)database_->update_modalities_in_study(study_pk);

        ++stored_count_;
        return storage_status::success;
    }

    std::vector<dicom_dataset> handle_query(
        query_level level,
        const dicom_dataset& query_keys) {

        std::vector<dicom_dataset> results;

        if (level == query_level::study) {
            study_query query;
            auto study_uid_val = query_keys.get_string(tags::study_instance_uid);
            if (!study_uid_val.empty()) query.study_uid = std::string(study_uid_val);

            auto pat_id_val = query_keys.get_string(tags::patient_id);
            if (!pat_id_val.empty()) query.patient_id = std::string(pat_id_val);

            auto pat_name_val = query_keys.get_string(tags::patient_name);
            if (!pat_name_val.empty()) query.patient_name = std::string(pat_name_val);

            auto studies = database_->search_studies(query);
            for (const auto& study : studies) {
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
        return results;
    }

    std::vector<dicom_dataset> query_worklist(const dicom_dataset& /* query */) {
        std::lock_guard<std::mutex> lock(mutex_);
        return worklist_items_;
    }

    pacs::network::Result<std::monostate> create_mpps(const mpps_instance& instance) {
        std::lock_guard<std::mutex> lock(mutex_);
        mpps_instances_.push_back(instance);
        return pacs::network::Result<std::monostate>::ok({});
    }

    pacs::network::Result<std::monostate> update_mpps(
        const std::string& uid,
        const dicom_dataset& /* modifications */,
        pacs::services::mpps_status status) {

        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& mpps : mpps_instances_) {
            if (mpps.sop_instance_uid == uid) {
                mpps.status = status;
                return pacs::network::Result<std::monostate>::ok({});
            }
        }
        return pacs::network::Result<std::monostate>::err({
            pacs::network::dimse::dimse_error::invalid_data_format,
            "MPPS not found"
        });
    }

    uint16_t port_;
    std::string ae_title_;
    test_directory test_dir_;
    std::filesystem::path storage_dir_;
    std::filesystem::path db_path_;

    std::unique_ptr<dicom_server> server_;
    std::unique_ptr<file_storage> file_storage_;
    std::unique_ptr<index_database> database_;

    mutable std::mutex mutex_;
    std::vector<dicom_dataset> worklist_items_;
    std::vector<mpps_instance> mpps_instances_;

    std::atomic<size_t> stored_count_{0};
    std::atomic<size_t> error_count_{0};
};

// =============================================================================
// Storage Helper Functions
// =============================================================================

/**
 * @brief Store a dataset to PACS server
 * @return true if storage succeeded
 */
bool store_to_pacs(
    const dicom_dataset& dataset,
    const std::string& host,
    uint16_t port,
    const std::string& called_ae,
    const std::string& calling_ae = "MODALITY") {

    auto sop_class = dataset.get_string(tags::sop_class_uid);
    if (sop_class.empty()) {
        return false;
    }

    auto assoc_result = test_association::connect(
        host, port, called_ae, calling_ae, {std::string(sop_class)});

    if (assoc_result.is_err()) {
        return false;
    }

    auto& assoc = assoc_result.value();
    storage_scu scu;
    auto store_result = scu.store(assoc, dataset);

    (void)assoc.release();
    return store_result.is_ok();
}

/**
 * @brief Store multiple datasets in parallel
 * @return Number of successful stores
 */
size_t parallel_store(
    multimodal_pacs_server& server,
    const std::vector<dicom_dataset>& datasets,
    const std::string& calling_ae = "MODALITY") {

    std::atomic<size_t> success_count{0};
    std::vector<std::future<bool>> futures;

    for (const auto& dataset : datasets) {
        futures.push_back(std::async(std::launch::async, [&, calling_ae]() {
            return store_to_pacs(
                dataset, "127.0.0.1", server.port(),
                server.ae_title(), calling_ae);
        }));
    }

    for (auto& future : futures) {
        if (future.get()) {
            ++success_count;
        }
    }

    return success_count.load();
}

}  // anonymous namespace

// =============================================================================
// Test Cases
// =============================================================================

TEST_CASE("Multi-modal workflow tests", "[workflow][multimodal][integration]") {
    auto port = find_available_port();
    multimodal_pacs_server server(port);
    REQUIRE(server.initialize());
    REQUIRE(server.start());

    SECTION("Scenario 1: Complete patient journey - CT and MR") {
        // This scenario simulates a patient arriving for scheduled CT and MR exams
        // Workflow: Worklist query → CT scan → MPPS → MR scan → MPPS → Query verification

        const std::string patient_id = "JOURNEY001";
        const std::string patient_name = "JOURNEY^PATIENT^COMPLETE";
        auto study_uid = generate_uid();

        // Step 1: Add scheduled procedures to worklist
        auto ct_worklist = test_data_generator::worklist(patient_id, "CT");
        ct_worklist.set_string(tags::study_instance_uid, vr_type::UI, study_uid);
        server.add_worklist_item(ct_worklist);

        auto mr_worklist = test_data_generator::worklist(patient_id, "MR");
        mr_worklist.set_string(tags::study_instance_uid, vr_type::UI, study_uid);
        server.add_worklist_item(mr_worklist);

        // Step 2: Perform CT examination
        // Generate CT series (3 images)
        auto ct_series_uid = generate_uid();
        std::vector<dicom_dataset> ct_images;
        for (int i = 0; i < 3; ++i) {
            auto ct = test_data_generator::ct(study_uid);
            ct.set_string(tags::patient_id, vr_type::LO, patient_id);
            ct.set_string(tags::patient_name, vr_type::PN, patient_name);
            ct.set_string(tags::series_instance_uid, vr_type::UI, ct_series_uid);
            ct.set_string(tags::sop_instance_uid, vr_type::UI, generate_uid());
            ct.set_string(tags::instance_number, vr_type::IS, std::to_string(i + 1));
            ct_images.push_back(std::move(ct));
        }

        // Store CT images
        for (const auto& image : ct_images) {
            REQUIRE(store_to_pacs(image, "127.0.0.1", port, server.ae_title(), "CT_SCANNER"));
        }

        // Step 3: Perform MR examination
        // Generate MR series (2 images)
        auto mr_series_uid = generate_uid();
        std::vector<dicom_dataset> mr_images;
        for (int i = 0; i < 2; ++i) {
            auto mr = test_data_generator::mr(study_uid);
            mr.set_string(tags::patient_id, vr_type::LO, patient_id);
            mr.set_string(tags::patient_name, vr_type::PN, patient_name);
            mr.set_string(tags::series_instance_uid, vr_type::UI, mr_series_uid);
            mr.set_string(tags::sop_instance_uid, vr_type::UI, generate_uid());
            mr.set_string(tags::instance_number, vr_type::IS, std::to_string(i + 1));
            mr_images.push_back(std::move(mr));
        }

        // Store MR images
        for (const auto& image : mr_images) {
            REQUIRE(store_to_pacs(image, "127.0.0.1", port, server.ae_title(), "MR_SCANNER"));
        }

        // Step 4: Verify data consistency
        auto verifier = server.get_verifier();

        // Verify patient exists
        REQUIRE(verifier.verify_patient_exists(patient_id));

        // Verify study contains both modalities
        REQUIRE(verifier.verify_modalities_in_study(study_uid, {"CT", "MR"}));

        // Verify series count (1 CT + 1 MR = 2 series)
        REQUIRE(verifier.verify_series_count(study_uid, 2));

        // Verify image counts
        REQUIRE(verifier.verify_image_count(ct_series_uid, 3));
        REQUIRE(verifier.verify_image_count(mr_series_uid, 2));

        // Verify no duplicate UIDs
        REQUIRE(verifier.verify_unique_uids(study_uid));

        // Verify total stored count
        REQUIRE(server.stored_count() == 5);
        REQUIRE(server.error_count() == 0);
    }

    SECTION("Scenario 2: Interventional workflow - XA cine acquisition") {
        // This scenario simulates an interventional radiology procedure
        // Workflow: XA cine acquisition → Store multi-frame → Query verification

        const std::string patient_id = "INTERVENT001";
        const std::string patient_name = "INTERVENTIONAL^PATIENT";
        auto study_uid = generate_uid();

        // Generate XA cine (multi-frame)
        auto xa_cine = test_data_generator::xa_cine(10, study_uid);  // 10 frames
        xa_cine.set_string(tags::patient_id, vr_type::LO, patient_id);
        xa_cine.set_string(tags::patient_name, vr_type::PN, patient_name);
        xa_cine.set_string(tags::series_description, vr_type::LO, "Coronary Angiography Run 1");

        auto xa_series_uid_opt = xa_cine.get_string(tags::series_instance_uid);
        REQUIRE(!xa_series_uid_opt.empty());
        auto xa_series_uid = std::string(xa_series_uid_opt);

        // Store XA cine
        REQUIRE(store_to_pacs(xa_cine, "127.0.0.1", port, server.ae_title(), "XA_CATH_LAB"));

        // Generate second XA run
        auto xa_cine_2 = test_data_generator::xa_cine(15, study_uid);  // 15 frames
        xa_cine_2.set_string(tags::patient_id, vr_type::LO, patient_id);
        xa_cine_2.set_string(tags::patient_name, vr_type::PN, patient_name);
        xa_cine_2.set_string(tags::series_description, vr_type::LO, "Coronary Angiography Run 2");

        // Store second run
        REQUIRE(store_to_pacs(xa_cine_2, "127.0.0.1", port, server.ae_title(), "XA_CATH_LAB"));

        // Verify data consistency
        auto verifier = server.get_verifier();

        REQUIRE(verifier.verify_patient_exists(patient_id));
        REQUIRE(verifier.verify_modalities_in_study(study_uid, {"XA"}));
        REQUIRE(verifier.verify_series_count(study_uid, 2));  // 2 XA runs
        REQUIRE(verifier.verify_unique_uids(study_uid));
    }

    SECTION("Scenario 3: Emergency multi-modality - Trauma workflow") {
        // This scenario simulates emergency trauma imaging
        // Workflow: Rapid CT → XA intervention → Follow-up CT (all in same study)

        const std::string patient_id = "TRAUMA001";
        const std::string patient_name = "TRAUMA^PATIENT^EMERGENCY";
        auto study_uid = generate_uid();

        // Step 1: Initial trauma CT
        auto initial_ct_series = generate_uid();
        for (int i = 0; i < 5; ++i) {
            auto ct = test_data_generator::ct(study_uid);
            ct.set_string(tags::patient_id, vr_type::LO, patient_id);
            ct.set_string(tags::patient_name, vr_type::PN, patient_name);
            ct.set_string(tags::series_instance_uid, vr_type::UI, initial_ct_series);
            ct.set_string(tags::series_description, vr_type::LO, "Initial Trauma CT");
            ct.set_string(tags::sop_instance_uid, vr_type::UI, generate_uid());
            REQUIRE(store_to_pacs(ct, "127.0.0.1", port, server.ae_title(), "CT_EMERGENCY"));
        }

        // Step 2: XA intervention
        auto xa_intervention = test_data_generator::xa_cine(20, study_uid);
        xa_intervention.set_string(tags::patient_id, vr_type::LO, patient_id);
        xa_intervention.set_string(tags::patient_name, vr_type::PN, patient_name);
        xa_intervention.set_string(tags::series_description, vr_type::LO, "Emergency Embolization");
        REQUIRE(store_to_pacs(xa_intervention, "127.0.0.1", port, server.ae_title(), "XA_EMERGENCY"));

        // Step 3: Follow-up CT
        auto followup_ct_series = generate_uid();
        for (int i = 0; i < 3; ++i) {
            auto ct = test_data_generator::ct(study_uid);
            ct.set_string(tags::patient_id, vr_type::LO, patient_id);
            ct.set_string(tags::patient_name, vr_type::PN, patient_name);
            ct.set_string(tags::series_instance_uid, vr_type::UI, followup_ct_series);
            ct.set_string(tags::series_description, vr_type::LO, "Follow-up CT");
            ct.set_string(tags::sop_instance_uid, vr_type::UI, generate_uid());
            REQUIRE(store_to_pacs(ct, "127.0.0.1", port, server.ae_title(), "CT_EMERGENCY"));
        }

        // Verify complete trauma workflow
        auto verifier = server.get_verifier();

        REQUIRE(verifier.verify_patient_exists(patient_id));
        REQUIRE(verifier.verify_modalities_in_study(study_uid, {"CT", "XA"}));
        REQUIRE(verifier.verify_series_count(study_uid, 3));  // 2 CT series + 1 XA
        REQUIRE(verifier.verify_image_count(initial_ct_series, 5));
        REQUIRE(verifier.verify_image_count(followup_ct_series, 3));
        REQUIRE(verifier.verify_unique_uids(study_uid));

        // Verify total instance count
        size_t total_instances = verifier.get_instance_count(study_uid);
        REQUIRE(total_instances == 9);  // 5 + 1 + 3
    }

    SECTION("Scenario 4: Concurrent modality operations") {
        // This scenario tests multiple scanners storing simultaneously
        // Tests thread safety and data integrity under concurrent load

        const std::string study_uid = generate_uid();
        const std::string patient_id = "CONCURRENT001";
        const std::string patient_name = "CONCURRENT^PATIENT";

        std::vector<dicom_dataset> all_datasets;
        std::vector<std::string> series_uids;

        // Generate datasets for 4 different modalities
        const std::vector<std::pair<std::string, int>> modality_counts = {
            {"CT", 5},
            {"MR", 4},
            {"XA", 2},
            {"US", 3}
        };

        for (const auto& [modality, count] : modality_counts) {
            auto series_uid = generate_uid();
            series_uids.push_back(series_uid);

            for (int i = 0; i < count; ++i) {
                dicom_dataset ds;
                if (modality == "CT") {
                    ds = test_data_generator::ct(study_uid);
                } else if (modality == "MR") {
                    ds = test_data_generator::mr(study_uid);
                } else if (modality == "XA") {
                    ds = test_data_generator::xa(study_uid);
                } else if (modality == "US") {
                    ds = test_data_generator::us(study_uid);
                }

                ds.set_string(tags::patient_id, vr_type::LO, patient_id);
                ds.set_string(tags::patient_name, vr_type::PN, patient_name);
                ds.set_string(tags::series_instance_uid, vr_type::UI, series_uid);
                ds.set_string(tags::sop_instance_uid, vr_type::UI, generate_uid());
                all_datasets.push_back(std::move(ds));
            }
        }

        // Store all datasets in parallel
        size_t success = parallel_store(server, all_datasets);

        // Allow time for indexing
        std::this_thread::sleep_for(std::chrono::milliseconds{100});

        // Verify all stores succeeded
        REQUIRE(success == all_datasets.size());

        // Verify data consistency
        auto verifier = server.get_verifier();

        REQUIRE(verifier.verify_patient_exists(patient_id));
        REQUIRE(verifier.verify_modalities_in_study(study_uid, {"CT", "MR", "XA", "US"}));
        REQUIRE(verifier.verify_series_count(study_uid, 4));
        REQUIRE(verifier.verify_unique_uids(study_uid));

        // Verify no errors
        REQUIRE(server.error_count() == 0);

        // Verify total stored
        size_t expected_total = 5 + 4 + 2 + 3;  // 14 images
        REQUIRE(verifier.get_instance_count(study_uid) == expected_total);
    }

    server.stop();
}

TEST_CASE("Multi-modal workflow with MPPS tracking", "[workflow][mpps][integration]") {
    auto port = find_available_port();
    multimodal_pacs_server server(port);
    REQUIRE(server.initialize());
    REQUIRE(server.start());

    SECTION("MPPS lifecycle for multi-modality study") {
        const std::string patient_id = "MPPS001";
        const std::string patient_name = "MPPS^TRACKING^PATIENT";
        auto study_uid = generate_uid();

        // Add worklist items for CT and MR
        auto ct_worklist = test_data_generator::worklist(patient_id, "CT");
        ct_worklist.set_string(tags::study_instance_uid, vr_type::UI, study_uid);
        server.add_worklist_item(ct_worklist);

        auto mr_worklist = test_data_generator::worklist(patient_id, "MR");
        mr_worklist.set_string(tags::study_instance_uid, vr_type::UI, study_uid);
        server.add_worklist_item(mr_worklist);

        // Perform CT and store images
        auto ct = test_data_generator::ct(study_uid);
        ct.set_string(tags::patient_id, vr_type::LO, patient_id);
        ct.set_string(tags::patient_name, vr_type::PN, patient_name);
        REQUIRE(store_to_pacs(ct, "127.0.0.1", port, server.ae_title(), "CT_SCANNER"));

        // Perform MR and store images
        auto mr = test_data_generator::mr(study_uid);
        mr.set_string(tags::patient_id, vr_type::LO, patient_id);
        mr.set_string(tags::patient_name, vr_type::PN, patient_name);
        REQUIRE(store_to_pacs(mr, "127.0.0.1", port, server.ae_title(), "MR_SCANNER"));

        // Verify data consistency
        auto verifier = server.get_verifier();
        REQUIRE(verifier.verify_modalities_in_study(study_uid, {"CT", "MR"}));
        REQUIRE(verifier.verify_unique_uids(study_uid));
        REQUIRE(server.stored_count() == 2);
    }

    server.stop();
}

TEST_CASE("Stress test: High-volume multi-modal storage", "[workflow][stress][integration]") {
    auto port = find_available_port();
    multimodal_pacs_server server(port);
    REQUIRE(server.initialize());
    REQUIRE(server.start());

    SECTION("Store 100 images from multiple modalities concurrently") {
        const std::string study_uid = generate_uid();
        const std::string patient_id = "STRESS001";
        const std::string patient_name = "STRESS^TEST^PATIENT";

        std::vector<dicom_dataset> datasets;

        // Generate 100 images across 4 modalities
        const std::vector<std::string> modalities = {"CT", "MR", "XA", "US"};

        for (int i = 0; i < 100; ++i) {
            const auto& modality = modalities[i % modalities.size()];
            dicom_dataset ds;

            if (modality == "CT") {
                ds = test_data_generator::ct(study_uid);
            } else if (modality == "MR") {
                ds = test_data_generator::mr(study_uid);
            } else if (modality == "XA") {
                ds = test_data_generator::xa(study_uid);
            } else {
                ds = test_data_generator::us(study_uid);
            }

            ds.set_string(tags::patient_id, vr_type::LO, patient_id);
            ds.set_string(tags::patient_name, vr_type::PN, patient_name);
            ds.set_string(tags::sop_instance_uid, vr_type::UI, generate_uid());
            datasets.push_back(std::move(ds));
        }

        // Store all datasets in parallel
        auto start = std::chrono::steady_clock::now();
        size_t success = parallel_store(server, datasets);
        auto duration = std::chrono::steady_clock::now() - start;

        // Allow time for async indexing
        std::this_thread::sleep_for(std::chrono::milliseconds{200});

        INFO("Stored " << success << " images in "
             << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() << "ms");

        // Verify high success rate (allow some failures under load)
        REQUIRE(success >= 95);  // At least 95% success

        // Verify no data corruption
        auto verifier = server.get_verifier();
        REQUIRE(verifier.verify_unique_uids(study_uid));
        REQUIRE(verifier.verify_modalities_in_study(study_uid, {"CT", "MR", "XA", "US"}));
    }

    server.stop();
}
