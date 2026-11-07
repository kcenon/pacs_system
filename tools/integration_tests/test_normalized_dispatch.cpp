// BSD 3-Clause License. See LICENSE.
#include "test_fixtures.h"
#include "kcenon/pacs/services/mpps_scp.h"
#include "kcenon/pacs/services/mpps_scu.h"
#include "kcenon/pacs/storage/index_database.h"
#include <catch2/catch_test_macros.hpp>

using namespace kcenon::pacs;
using namespace kcenon::pacs::integration_test;
using namespace kcenon::pacs::network;
using namespace kcenon::pacs::network::dimse;

namespace {
constexpr auto get_class = "1.2.826.0.1.3680043.9.9999.1316";

// A test-only normalized service exercises actual N-GET dispatch. This does
// not add N-GET support to the MPPS SOP Class.
class get_probe final : public services::scp_service {
public:
    std::vector<std::string> supported_sop_classes() const override { return {get_class}; }
    std::string_view service_name() const noexcept override { return "N-GET dispatch probe"; }
    network::Result<std::monostate> handle_message(association& assoc, uint8_t context,
                                         const dimse_message& request) override {
        auto response = make_n_get_rsp(request.message_id(), get_class,
                                      request.requested_sop_instance_uid());
        core::dicom_dataset data;
        data.set_string(core::tags::patient_id, encoding::vr_type::LO, "DISPATCHED");
        response.set_dataset(std::move(data));
        return assoc.send_dimse(context, response);
    }
};

template<class Server>
void durable_mpps_workflow() {
    test_directory directory("normalized_dispatch_");
    const auto path = (directory.path() / "mpps.db").string();
    const auto port = find_available_port();
    // Exact fixed identifiers exercise both UI padding parities and terminal states.
    const std::vector<std::pair<std::string, std::string>> cases{
        {"1.2.3.1316.1", "COMPLETED"}, {"1.2.3.1316.10", "COMPLETED"},
        {"1.2.3.1316.2", "DISCONTINUED"}, {"1.2.3.1316.20", "DISCONTINUED"}};

    for (bool restart : {false, true}) {
        auto opened = storage::index_database::open(path);
        REQUIRE(opened.is_ok());
        auto database = std::move(opened.value());
        server_config config;
        config.ae_title = "NORMALIZED";
        config.port = port;
        Server server(config);
        auto mpps = std::make_shared<services::mpps_scp>();
        mpps->set_create_handler([&](const services::mpps_instance& instance) -> network::Result<std::monostate> {
            auto created = database->create_mpps(instance.sop_instance_uid, instance.station_ae);
            if (created.is_err()) { return created.error(); }
            return std::monostate{};
        });
        mpps->set_set_handler([&](const std::string& uid, const core::dicom_dataset&,
                                  services::mpps_status status) -> network::Result<std::monostate> {
            return database->update_mpps(uid, services::to_string(status));
        });
        server.register_service(mpps);
        server.register_service(std::make_shared<services::verification_scp>());
        server.register_service(std::make_shared<get_probe>());
        REQUIRE(server.start().is_ok());

        // A separately opened connection observes committed database state.
        auto observer = storage::index_database::open(path);
        REQUIRE(observer.is_ok());
        auto config_scu = association_config{};
        config_scu.calling_ae_title = "MODALITY";
        config_scu.called_ae_title = "NORMALIZED";
        config_scu.proposed_contexts = {
            {1, std::string(services::mpps_sop_class_uid), {"1.2.840.10008.1.2.1"}},
            {3, "1.2.840.10008.1.1", {"1.2.840.10008.1.2.1"}},
            {5, get_class, {"1.2.840.10008.1.2.1"}}};
        auto connected = association::connect("localhost", port, config_scu, default_timeout());
        REQUIRE(connected.is_ok());
        auto& assoc = connected.value();
        uint16_t message_id = 0;
        auto exchange = [&](uint8_t context, dimse_message request, command_field command,
                            status_code status, const std::string& uid) {
            const auto id = request.message_id();
            REQUIRE(assoc.send_dimse(context, request).is_ok());
            auto received = assoc.receive_dimse(default_timeout());
            REQUIRE(received.is_ok());
            const auto& [actual_context, response] = received.value();
            REQUIRE(actual_context == context);
            REQUIRE(response.command() == command);
            REQUIRE(response.message_id_responded_to() == id);
            REQUIRE(response.status() == status);
            REQUIRE(response.affected_sop_instance_uid() == uid);
        };

        for (const auto& [uid, terminal] : cases) {
            CAPTURE(restart, uid, terminal);
            if (!restart) {
                auto create = make_n_create_rq(++message_id, services::mpps_sop_class_uid, uid);
                core::dicom_dataset data;
                data.set_string(core::tags::performed_procedure_step_status, encoding::vr_type::CS, "IN PROGRESS");
                create.set_dataset(std::move(data));
                exchange(1, std::move(create), command_field::n_create_rsp, status_success, uid);
                auto row = observer.value()->find_mpps(uid);
                REQUIRE(row.has_value());
                REQUIRE(row->mpps_uid == uid);
                REQUIRE(row->status == "IN PROGRESS");

                auto update = make_n_set_rq(++message_id, services::mpps_sop_class_uid, uid);
                core::dicom_dataset modifications;
                modifications.set_string(core::tags::performed_procedure_step_status, encoding::vr_type::CS, terminal);
                update.set_dataset(std::move(modifications));
                if (terminal == "DISCONTINUED") {
                    // Exercise the public service builder too, not just DIMSE factories.
                    services::mpps_scu scu;
                    auto result = scu.discontinue(assoc, uid, "Synthetic test");
                    REQUIRE(result.is_ok());
                    REQUIRE(result.value().is_success());
                } else {
                    exchange(1, std::move(update), command_field::n_set_rsp, status_success, uid);
                }
            }
            auto row = observer.value()->find_mpps(uid);
            REQUIRE(row.has_value());
            REQUIRE(row->mpps_uid == uid);
            REQUIRE(row->status == terminal);
            auto invalid = make_n_set_rq(++message_id, services::mpps_sop_class_uid, uid);
            core::dicom_dataset modifications;
            modifications.set_string(core::tags::performed_procedure_step_status, encoding::vr_type::CS,
                                     terminal == "COMPLETED" ? "DISCONTINUED" : "COMPLETED");
            invalid.set_dataset(std::move(modifications));
            exchange(1, std::move(invalid), command_field::n_set_rsp, status_error_unable_to_process, uid);
            REQUIRE(observer.value()->find_mpps(uid)->status == terminal);
        }
        auto get = make_n_get_rq(++message_id, get_class, cases.front().first);
        REQUIRE(assoc.send_dimse(5, get).is_ok());
        auto response = assoc.receive_dimse(default_timeout());
        REQUIRE(response.is_ok());
        REQUIRE(response.value().first == 5);
        REQUIRE(response.value().second.command() == command_field::n_get_rsp);
        REQUIRE(response.value().second.status() == status_success);
        REQUIRE(response.value().second.has_dataset());
        REQUIRE(response.value().second.dataset().value().get().get_string(core::tags::patient_id) == "DISPATCHED");
        const std::string active_uid = "1.2.3.1316.30";
        if (!restart) {
            auto create = make_n_create_rq(++message_id, services::mpps_sop_class_uid, active_uid);
            core::dicom_dataset data;
            data.set_string(core::tags::performed_procedure_step_status, encoding::vr_type::CS, "IN PROGRESS");
            create.set_dataset(std::move(data));
            exchange(1, std::move(create), command_field::n_create_rsp, status_success, active_uid);
        }
        REQUIRE(assoc.release(default_timeout()).is_ok());

        for (bool wrong_context : {false, true}) {
            // An active record would accept the update if validation were bypassed.
            auto malformed = association::connect("localhost", port, config_scu, default_timeout());
            REQUIRE(malformed.is_ok());
            auto wrong = make_n_set_rq(1, wrong_context ? services::mpps_sop_class_uid :
                                      std::string_view("1.2.840.10008.1.1"), active_uid);
            core::dicom_dataset data;
            data.set_string(core::tags::performed_procedure_step_status, encoding::vr_type::CS, "COMPLETED");
            wrong.set_dataset(std::move(data));
            REQUIRE(malformed.value().send_dimse(wrong_context ? 3 : 1, wrong).is_ok());
            auto rejected = malformed.value().receive_dimse(default_timeout());
            REQUIRE(rejected.is_err());
            REQUIRE(malformed.value().state() == association_state::aborted);
            REQUIRE(observer.value()->find_mpps(active_uid).has_value());
            REQUIRE(observer.value()->find_mpps(active_uid)->status == "IN PROGRESS");
        }
        server.stop();
    }
}
} // namespace

TEST_CASE("Normalized dispatch preserves durable MPPS state on retained v1 API", "[worklist][mpps][dispatch][v1]") {
    // Retained v1 is an in-process API. This is not an external TCP claim.
    durable_mpps_workflow<dicom_server>();
}

TEST_CASE("Normalized dispatch preserves durable MPPS state over v2 TCP", "[worklist][mpps][dispatch][v2]") {
    durable_mpps_workflow<v2::dicom_server_v2>();
}
