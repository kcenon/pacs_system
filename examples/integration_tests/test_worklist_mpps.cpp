/**
 * @file test_worklist_mpps.cpp
 * @brief Scenario 3: Worklist to MPPS Workflow Tests
 *
 * Tests the complete worklist and MPPS workflow:
 * 1. Start RIS Mock (Worklist + MPPS SCP)
 * 2. Insert scheduled procedure
 * 3. Query Worklist -> Verify scheduled item
 * 4. Create MPPS (IN PROGRESS)
 * 5. Update MPPS (COMPLETED)
 * 6. Verify MPPS recorded
 * 7. Stop RIS Mock
 *
 * @see Issue #111 - Integration Test Suite
 */

#include "test_fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/mpps_scp.hpp"
#include "pacs/services/verification_scp.hpp"
#include "pacs/services/worklist_scp.hpp"

#include <mutex>
#include <vector>

using namespace pacs::integration_test;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::services;
using namespace pacs::core;

// =============================================================================
// Helper: RIS Mock Server
// =============================================================================

namespace {

/**
 * @brief Mock RIS server for worklist and MPPS testing
 *
 * Simulates a Radiology Information System that provides:
 * - Modality Worklist SCP (scheduled procedures)
 * - MPPS SCP (procedure tracking)
 */
class ris_mock_server {
public:
    explicit ris_mock_server(uint16_t port, const std::string& ae_title = "RIS_MOCK")
        : port_(port)
        , ae_title_(ae_title) {

        server_config config;
        config.ae_title = ae_title_;
        config.port = port_;
        config.max_associations = 20;
        config.idle_timeout = std::chrono::seconds{60};
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.5";
        config.implementation_version_name = "RIS_MOCK";

        server_ = std::make_unique<dicom_server>(config);
    }

    bool initialize() {
        // Register Verification SCP
        server_->register_service(std::make_shared<verification_scp>());

        // Register Worklist SCP
        auto worklist_scp_ptr = std::make_shared<worklist_scp>();
        worklist_scp_ptr->on_query([this](
            const dicom_dataset& query_keys,
            const std::string& /* calling_ae */) -> std::vector<dicom_dataset> {
            return handle_worklist_query(query_keys);
        });
        server_->register_service(worklist_scp_ptr);

        // Register MPPS SCP
        auto mpps_scp_ptr = std::make_shared<mpps_scp>();
        mpps_scp_ptr->on_create([this](const mpps_instance& instance) -> Result<std::monostate> {
            return handle_mpps_create(instance);
        });
        mpps_scp_ptr->on_set([this](
            const std::string& sop_instance_uid,
            const dicom_dataset& modifications,
            mpps_status new_status) -> Result<std::monostate> {
            return handle_mpps_set(sop_instance_uid, modifications, new_status);
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

    /**
     * @brief Add a scheduled procedure to the worklist
     */
    void add_scheduled_procedure(const dicom_dataset& procedure) {
        std::lock_guard<std::mutex> lock(mutex_);
        scheduled_procedures_.push_back(procedure);
    }

    /**
     * @brief Get all MPPS instances
     */
    std::vector<mpps_instance> get_mpps_instances() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return mpps_instances_;
    }

    /**
     * @brief Get MPPS instance by SOP Instance UID
     */
    std::optional<mpps_instance> get_mpps(const std::string& sop_instance_uid) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& mpps : mpps_instances_) {
            if (mpps.sop_instance_uid == sop_instance_uid) {
                return mpps;
            }
        }
        return std::nullopt;
    }

    uint16_t port() const { return port_; }
    const std::string& ae_title() const { return ae_title_; }

    size_t scheduled_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return scheduled_procedures_.size();
    }

    size_t mpps_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return mpps_instances_.size();
    }

private:
    std::vector<dicom_dataset> handle_worklist_query(const dicom_dataset& query_keys) {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<dicom_dataset> results;

        for (const auto& procedure : scheduled_procedures_) {
            bool matches = true;

            // Check filter criteria
            auto query_modality = query_keys.get_string(tags::modality);
            if (!query_modality.empty()) {
                auto proc_modality = procedure.get_string(tags::modality);
                if (proc_modality != query_modality) {
                    matches = false;
                }
            }

            auto query_date = query_keys.get_string(tags::scheduled_procedure_step_start_date);
            if (!query_date.empty()) {
                auto proc_date = procedure.get_string(tags::scheduled_procedure_step_start_date);
                // Simple exact match (production would handle date ranges)
                if (proc_date != query_date && query_date != "*") {
                    matches = false;
                }
            }

            auto query_station = query_keys.get_string(tags::scheduled_station_ae_title);
            if (!query_station.empty()) {
                auto proc_station = procedure.get_string(tags::scheduled_station_ae_title);
                if (proc_station != query_station && query_station != "*") {
                    matches = false;
                }
            }

            if (matches) {
                results.push_back(procedure);
            }
        }

        return results;
    }

    Result<std::monostate> handle_mpps_create(const mpps_instance& instance) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Check for duplicate
        for (const auto& existing : mpps_instances_) {
            if (existing.sop_instance_uid == instance.sop_instance_uid) {
                return Result<std::monostate>::err({
                    dimse_error_code::duplicate_sop_instance,
                    "MPPS instance already exists"
                });
            }
        }

        mpps_instances_.push_back(instance);
        return Result<std::monostate>::ok({});
    }

    Result<std::monostate> handle_mpps_set(
        const std::string& sop_instance_uid,
        const dicom_dataset& modifications,
        mpps_status new_status) {

        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& mpps : mpps_instances_) {
            if (mpps.sop_instance_uid == sop_instance_uid) {
                // Validate state transition
                if (mpps.status == mpps_status::completed ||
                    mpps.status == mpps_status::discontinued) {
                    return Result<std::monostate>::err({
                        dimse_error_code::invalid_attribute_value,
                        "Cannot modify completed/discontinued MPPS"
                    });
                }

                // Update status and merge modifications
                mpps.status = new_status;
                mpps.dataset.merge(modifications);

                return Result<std::monostate>::ok({});
            }
        }

        return Result<std::monostate>::err({
            dimse_error_code::no_such_sop_instance,
            "MPPS instance not found"
        });
    }

    uint16_t port_;
    std::string ae_title_;
    std::unique_ptr<dicom_server> server_;

    mutable std::mutex mutex_;
    std::vector<dicom_dataset> scheduled_procedures_;
    std::vector<mpps_instance> mpps_instances_;
};

/**
 * @brief Create a scheduled procedure dataset
 */
dicom_dataset create_scheduled_procedure(
    const std::string& patient_name,
    const std::string& patient_id,
    const std::string& modality,
    const std::string& station_ae,
    const std::string& procedure_desc,
    const std::string& scheduled_date,
    const std::string& scheduled_time) {

    dicom_dataset ds;

    // Patient module
    ds.set_string(tags::patient_name, patient_name);
    ds.set_string(tags::patient_id, patient_id);
    ds.set_string(tags::patient_birth_date, "19800101");
    ds.set_string(tags::patient_sex, "M");

    // Scheduled Procedure Step
    ds.set_string(tags::scheduled_procedure_step_start_date, scheduled_date);
    ds.set_string(tags::scheduled_procedure_step_start_time, scheduled_time);
    ds.set_string(tags::modality, modality);
    ds.set_string(tags::scheduled_station_ae_title, station_ae);
    ds.set_string(tags::scheduled_procedure_step_description, procedure_desc);
    ds.set_string(tags::scheduled_procedure_step_id, generate_uid());

    // Requested Procedure
    ds.set_string(tags::requested_procedure_id, "RP_" + patient_id);
    ds.set_string(tags::accession_number, "ACC_" + patient_id);
    ds.set_string(tags::study_instance_uid, generate_uid());
    ds.set_string(tags::requested_procedure_description, procedure_desc);

    return ds;
}

}  // namespace

// =============================================================================
// Scenario 3: Worklist to MPPS Workflow
// =============================================================================

TEST_CASE("Worklist query returns scheduled procedures", "[worklist][query]") {
    auto port = find_available_port();
    ris_mock_server ris(port, "RIS_MOCK");

    REQUIRE(ris.initialize());
    REQUIRE(ris.start());

    // Add scheduled procedures
    ris.add_scheduled_procedure(create_scheduled_procedure(
        "TEST^PATIENT1", "P001", "CT", "CT_SCANNER", "CT Chest", "20240201", "090000"));
    ris.add_scheduled_procedure(create_scheduled_procedure(
        "TEST^PATIENT2", "P002", "MR", "MR_SCANNER", "MR Brain", "20240201", "100000"));
    ris.add_scheduled_procedure(create_scheduled_procedure(
        "TEST^PATIENT3", "P003", "CT", "CT_SCANNER", "CT Abdomen", "20240202", "080000"));

    SECTION("Query all scheduled procedures") {
        association_config config;
        config.calling_ae_title = "MODALITY";
        config.called_ae_title = ris.ae_title();
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.6";
        config.proposed_contexts.push_back({
            1,
            modality_worklist_information_model_find,
            {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
        });

        auto connect_result = association::connect(
            "localhost", port, config, default_timeout);
        REQUIRE(connect_result.is_ok());

        auto& assoc = connect_result.value();

        // Query all (empty criteria = return all)
        dicom_dataset query_keys;
        query_keys.set_string(tags::patient_name, "");
        query_keys.set_string(tags::patient_id, "");
        query_keys.set_string(tags::modality, "");
        query_keys.set_string(tags::scheduled_procedure_step_start_date, "");
        query_keys.set_string(tags::scheduled_station_ae_title, "");

        auto context_id = *assoc.accepted_context_id(modality_worklist_information_model_find);
        auto find_rq = make_c_find_rq(1, modality_worklist_information_model_find, query_keys);
        (void)assoc.send_dimse(context_id, find_rq);

        std::vector<dicom_dataset> results;
        while (true) {
            auto recv_result = assoc.receive_dimse(default_timeout);
            if (recv_result.is_err()) break;

            auto& [recv_ctx, rsp] = recv_result.value();
            if (rsp.status() == status_success) break;
            if (rsp.status() == status_pending && rsp.has_dataset()) {
                results.push_back(rsp.dataset());
            }
        }

        REQUIRE(results.size() == 3);

        (void)assoc.release(default_timeout);
    }

    SECTION("Query by modality filter") {
        association_config config;
        config.calling_ae_title = "CT_SCANNER";
        config.called_ae_title = ris.ae_title();
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.6";
        config.proposed_contexts.push_back({
            1,
            modality_worklist_information_model_find,
            {"1.2.840.10008.1.2.1"}
        });

        auto connect_result = association::connect(
            "localhost", port, config, default_timeout);
        REQUIRE(connect_result.is_ok());

        auto& assoc = connect_result.value();

        // Query CT only
        dicom_dataset query_keys;
        query_keys.set_string(tags::patient_name, "");
        query_keys.set_string(tags::modality, "CT");
        query_keys.set_string(tags::scheduled_station_ae_title, "");

        auto context_id = *assoc.accepted_context_id(modality_worklist_information_model_find);
        auto find_rq = make_c_find_rq(1, modality_worklist_information_model_find, query_keys);
        (void)assoc.send_dimse(context_id, find_rq);

        std::vector<dicom_dataset> results;
        while (true) {
            auto recv_result = assoc.receive_dimse(default_timeout);
            if (recv_result.is_err()) break;

            auto& [recv_ctx, rsp] = recv_result.value();
            if (rsp.status() == status_success) break;
            if (rsp.status() == status_pending && rsp.has_dataset()) {
                results.push_back(rsp.dataset());
            }
        }

        // Should return 2 CT procedures
        REQUIRE(results.size() == 2);
        for (const auto& result : results) {
            REQUIRE(result.get_string(tags::modality) == "CT");
        }

        (void)assoc.release(default_timeout);
    }

    ris.stop();
}

TEST_CASE("Complete MPPS workflow", "[worklist][mpps][workflow]") {
    auto port = find_available_port();
    ris_mock_server ris(port, "RIS_MOCK");

    REQUIRE(ris.initialize());
    REQUIRE(ris.start());

    // Add a scheduled procedure
    auto procedure = create_scheduled_procedure(
        "MPPS^TEST", "MPPS001", "CT", "CT_SCANNER", "CT Head", "20240201", "090000");
    ris.add_scheduled_procedure(procedure);

    auto study_uid = procedure.get_string(tags::study_instance_uid);
    auto mpps_uid = generate_uid();

    // Step 1: Query worklist to get scheduled procedure
    association_config wl_config;
    wl_config.calling_ae_title = "CT_SCANNER";
    wl_config.called_ae_title = ris.ae_title();
    wl_config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.6";
    wl_config.proposed_contexts.push_back({
        1,
        modality_worklist_information_model_find,
        {"1.2.840.10008.1.2.1"}
    });

    auto wl_connect = association::connect("localhost", port, wl_config, default_timeout);
    REQUIRE(wl_connect.is_ok());

    auto& wl_assoc = wl_connect.value();

    dicom_dataset wl_query;
    wl_query.set_string(tags::patient_id, "MPPS001");
    wl_query.set_string(tags::modality, "CT");

    auto wl_ctx = *wl_assoc.accepted_context_id(modality_worklist_information_model_find);
    auto wl_rq = make_c_find_rq(1, modality_worklist_information_model_find, wl_query);
    (void)wl_assoc.send_dimse(wl_ctx, wl_rq);

    std::vector<dicom_dataset> wl_results;
    while (true) {
        auto recv = wl_assoc.receive_dimse(default_timeout);
        if (recv.is_err()) break;
        auto& [ctx, rsp] = recv.value();
        if (rsp.status() == status_success) break;
        if (rsp.status() == status_pending && rsp.has_dataset()) {
            wl_results.push_back(rsp.dataset());
        }
    }

    REQUIRE(wl_results.size() == 1);
    (void)wl_assoc.release(default_timeout);

    // Step 2: Create MPPS (IN PROGRESS)
    association_config mpps_config;
    mpps_config.calling_ae_title = "CT_SCANNER";
    mpps_config.called_ae_title = ris.ae_title();
    mpps_config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.6";
    mpps_config.proposed_contexts.push_back({
        1,
        mpps_sop_class_uid,
        {"1.2.840.10008.1.2.1"}
    });

    auto mpps_connect = association::connect("localhost", port, mpps_config, default_timeout);
    REQUIRE(mpps_connect.is_ok());

    auto& mpps_assoc = mpps_connect.value();

    // Create N-CREATE for MPPS IN PROGRESS
    dicom_dataset mpps_create_ds;
    mpps_create_ds.set_string(tags::performed_procedure_step_status, "IN PROGRESS");
    mpps_create_ds.set_string(tags::performed_procedure_step_start_date, "20240201");
    mpps_create_ds.set_string(tags::performed_procedure_step_start_time, "091500");
    mpps_create_ds.set_string(tags::performed_station_ae_title, "CT_SCANNER");
    mpps_create_ds.set_string(tags::modality, "CT");
    mpps_create_ds.set_string(tags::study_instance_uid, study_uid);
    mpps_create_ds.set_string(tags::patient_name, "MPPS^TEST");
    mpps_create_ds.set_string(tags::patient_id, "MPPS001");

    auto mpps_ctx = *mpps_assoc.accepted_context_id(mpps_sop_class_uid);
    auto n_create_rq = make_n_create_rq(1, mpps_sop_class_uid, mpps_uid, mpps_create_ds);
    (void)mpps_assoc.send_dimse(mpps_ctx, n_create_rq);

    auto create_recv = mpps_assoc.receive_dimse(default_timeout);
    REQUIRE(create_recv.is_ok());
    auto& [create_ctx, create_rsp] = create_recv.value();
    REQUIRE(create_rsp.command() == command_field::n_create_rsp);
    REQUIRE(create_rsp.status() == status_success);

    // Verify MPPS was created
    REQUIRE(ris.mpps_count() == 1);
    auto mpps_opt = ris.get_mpps(mpps_uid);
    REQUIRE(mpps_opt.has_value());
    REQUIRE(mpps_opt->status == mpps_status::in_progress);

    // Step 3: Update MPPS (COMPLETED)
    dicom_dataset mpps_set_ds;
    mpps_set_ds.set_string(tags::performed_procedure_step_status, "COMPLETED");
    mpps_set_ds.set_string(tags::performed_procedure_step_end_date, "20240201");
    mpps_set_ds.set_string(tags::performed_procedure_step_end_time, "093000");

    auto n_set_rq = make_n_set_rq(2, mpps_sop_class_uid, mpps_uid, mpps_set_ds);
    (void)mpps_assoc.send_dimse(mpps_ctx, n_set_rq);

    auto set_recv = mpps_assoc.receive_dimse(default_timeout);
    REQUIRE(set_recv.is_ok());
    auto& [set_ctx, set_rsp] = set_recv.value();
    REQUIRE(set_rsp.command() == command_field::n_set_rsp);
    REQUIRE(set_rsp.status() == status_success);

    // Verify MPPS was updated
    mpps_opt = ris.get_mpps(mpps_uid);
    REQUIRE(mpps_opt.has_value());
    REQUIRE(mpps_opt->status == mpps_status::completed);

    (void)mpps_assoc.release(default_timeout);
    ris.stop();
}

TEST_CASE("MPPS discontinue workflow", "[worklist][mpps][discontinue]") {
    auto port = find_available_port();
    ris_mock_server ris(port, "RIS_MOCK");

    REQUIRE(ris.initialize());
    REQUIRE(ris.start());

    auto mpps_uid = generate_uid();

    association_config config;
    config.calling_ae_title = "CT_SCANNER";
    config.called_ae_title = ris.ae_title();
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.6";
    config.proposed_contexts.push_back({
        1,
        mpps_sop_class_uid,
        {"1.2.840.10008.1.2.1"}
    });

    auto connect_result = association::connect("localhost", port, config, default_timeout);
    REQUIRE(connect_result.is_ok());

    auto& assoc = connect_result.value();

    // Create MPPS IN PROGRESS
    dicom_dataset mpps_ds;
    mpps_ds.set_string(tags::performed_procedure_step_status, "IN PROGRESS");
    mpps_ds.set_string(tags::performed_procedure_step_start_date, "20240201");
    mpps_ds.set_string(tags::performed_procedure_step_start_time, "100000");
    mpps_ds.set_string(tags::performed_station_ae_title, "CT_SCANNER");
    mpps_ds.set_string(tags::modality, "CT");
    mpps_ds.set_string(tags::study_instance_uid, generate_uid());
    mpps_ds.set_string(tags::patient_name, "DISCONTINUE^TEST");
    mpps_ds.set_string(tags::patient_id, "DISC001");

    auto ctx = *assoc.accepted_context_id(mpps_sop_class_uid);
    auto n_create = make_n_create_rq(1, mpps_sop_class_uid, mpps_uid, mpps_ds);
    (void)assoc.send_dimse(ctx, n_create);

    auto create_recv = assoc.receive_dimse(default_timeout);
    REQUIRE(create_recv.is_ok());
    REQUIRE(create_recv.value().second.status() == status_success);

    // Discontinue the procedure
    dicom_dataset disc_ds;
    disc_ds.set_string(tags::performed_procedure_step_status, "DISCONTINUED");
    disc_ds.set_string(tags::performed_procedure_step_end_date, "20240201");
    disc_ds.set_string(tags::performed_procedure_step_end_time, "101500");
    disc_ds.set_string(tags::performed_procedure_step_discontinuation_reason_code_sequence, "");

    auto n_set = make_n_set_rq(2, mpps_sop_class_uid, mpps_uid, disc_ds);
    (void)assoc.send_dimse(ctx, n_set);

    auto set_recv = assoc.receive_dimse(default_timeout);
    REQUIRE(set_recv.is_ok());
    REQUIRE(set_recv.value().second.status() == status_success);

    // Verify discontinued
    auto mpps_opt = ris.get_mpps(mpps_uid);
    REQUIRE(mpps_opt.has_value());
    REQUIRE(mpps_opt->status == mpps_status::discontinued);

    (void)assoc.release(default_timeout);
    ris.stop();
}

TEST_CASE("MPPS cannot modify completed procedure", "[worklist][mpps][error]") {
    auto port = find_available_port();
    ris_mock_server ris(port, "RIS_MOCK");

    REQUIRE(ris.initialize());
    REQUIRE(ris.start());

    auto mpps_uid = generate_uid();

    association_config config;
    config.calling_ae_title = "CT_SCANNER";
    config.called_ae_title = ris.ae_title();
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.6";
    config.proposed_contexts.push_back({
        1,
        mpps_sop_class_uid,
        {"1.2.840.10008.1.2.1"}
    });

    auto connect_result = association::connect("localhost", port, config, default_timeout);
    REQUIRE(connect_result.is_ok());

    auto& assoc = connect_result.value();
    auto ctx = *assoc.accepted_context_id(mpps_sop_class_uid);

    // Create and complete MPPS
    dicom_dataset create_ds;
    create_ds.set_string(tags::performed_procedure_step_status, "IN PROGRESS");
    create_ds.set_string(tags::performed_procedure_step_start_date, "20240201");
    create_ds.set_string(tags::performed_procedure_step_start_time, "110000");
    create_ds.set_string(tags::performed_station_ae_title, "CT_SCANNER");
    create_ds.set_string(tags::modality, "CT");

    auto n_create = make_n_create_rq(1, mpps_sop_class_uid, mpps_uid, create_ds);
    (void)assoc.send_dimse(ctx, n_create);
    auto create_recv = assoc.receive_dimse(default_timeout);
    REQUIRE(create_recv.is_ok());

    // Complete the MPPS
    dicom_dataset complete_ds;
    complete_ds.set_string(tags::performed_procedure_step_status, "COMPLETED");
    auto n_set_complete = make_n_set_rq(2, mpps_sop_class_uid, mpps_uid, complete_ds);
    (void)assoc.send_dimse(ctx, n_set_complete);
    auto complete_recv = assoc.receive_dimse(default_timeout);
    REQUIRE(complete_recv.is_ok());

    // Try to modify completed MPPS - should fail
    dicom_dataset modify_ds;
    modify_ds.set_string(tags::performed_procedure_step_description, "Changed");
    auto n_set_modify = make_n_set_rq(3, mpps_sop_class_uid, mpps_uid, modify_ds);
    (void)assoc.send_dimse(ctx, n_set_modify);

    auto modify_recv = assoc.receive_dimse(default_timeout);
    REQUIRE(modify_recv.is_ok());
    // Should return error status
    REQUIRE(modify_recv.value().second.status() != status_success);

    (void)assoc.release(default_timeout);
    ris.stop();
}
