/**
 * @file n_service_test.cpp
 * @brief Unit tests for DIMSE-N services (N-CREATE, N-SET, N-GET, N-EVENT-REPORT,
 *        N-ACTION, N-DELETE)
 */

#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/n_action.hpp>
#include <pacs/network/dimse/n_create.hpp>
#include <pacs/network/dimse/n_delete.hpp>
#include <pacs/network/dimse/n_event_report.hpp>
#include <pacs/network/dimse/n_get.hpp>
#include <pacs/network/dimse/n_set.hpp>
#include <pacs/network/dimse/status_codes.hpp>

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/transfer_syntax.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::network::dimse;
using namespace pacs::core;
using namespace pacs::encoding;

// ============================================================================
// DIMSE-N Status Codes Tests
// ============================================================================

TEST_CASE("DIMSE-N specific status codes", "[dimse][status][n-service]") {
    SECTION("Attribute errors") {
        CHECK(is_failure(status_error_attribute_list_error));
        CHECK(is_failure(status_error_attribute_value_out_of_range));
        CHECK(status_description(status_error_attribute_list_error) ==
              "Error: Attribute list error");
        CHECK(status_description(status_error_attribute_value_out_of_range) ==
              "Error: Attribute value out of range");
    }

    SECTION("Object instance errors") {
        CHECK(is_failure(status_error_invalid_object_instance));
        CHECK(is_failure(status_error_no_such_sop_class));
        CHECK(is_failure(status_error_class_instance_conflict));
        CHECK(status_description(status_error_invalid_object_instance) ==
              "Error: Invalid object instance");
    }

    SECTION("Authorization and operation errors") {
        CHECK(is_failure(status_error_not_authorized));
        CHECK(is_failure(status_error_duplicate_invocation));
        CHECK(is_failure(status_error_unrecognized_operation));
        CHECK(is_failure(status_error_mistyped_argument));
        CHECK(is_failure(status_error_resource_limitation));
    }

    SECTION("Type ID errors") {
        CHECK(is_failure(status_error_no_such_action_type));
        CHECK(is_failure(status_error_no_such_event_type));
        CHECK(is_failure(status_error_processing_failure));
    }
}

// ============================================================================
// N-CREATE Tests
// ============================================================================

TEST_CASE("N-CREATE request message", "[dimse][message][n-create]") {
    const std::string_view mpps_class = "1.2.840.10008.3.1.2.3.3";  // MPPS SOP Class
    const std::string_view instance_uid = "1.2.3.4.5.6.7.8.9";

    SECTION("Basic N-CREATE request") {
        auto msg = make_n_create_rq(1, mpps_class, instance_uid);

        CHECK(msg.command() == command_field::n_create_rq);
        CHECK(msg.message_id() == 1);
        CHECK(msg.is_request());
        CHECK(is_dimse_n(msg.command()));
        CHECK(msg.affected_sop_class_uid() == mpps_class);
        CHECK(msg.affected_sop_instance_uid() == instance_uid);
    }

    SECTION("N-CREATE request without instance UID") {
        auto msg = make_n_create_rq(2, mpps_class);

        CHECK(msg.command() == command_field::n_create_rq);
        CHECK(msg.message_id() == 2);
        CHECK(msg.affected_sop_class_uid() == mpps_class);
        CHECK(msg.affected_sop_instance_uid().empty());
    }

    SECTION("N-CREATE request with dataset") {
        auto msg = make_n_create_rq(3, mpps_class, instance_uid);

        dicom_dataset attributes;
        attributes.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
        attributes.set_string(tags::patient_id, vr_type::LO, "12345");
        msg.set_dataset(std::move(attributes));

        CHECK(msg.has_dataset());
        CHECK(msg.dataset().get_string(tags::patient_name) == "DOE^JOHN");
    }
}

TEST_CASE("N-CREATE response message", "[dimse][message][n-create]") {
    const std::string_view mpps_class = "1.2.840.10008.3.1.2.3.3";
    const std::string_view instance_uid = "1.2.3.4.5.6.7.8.9";

    SECTION("Successful N-CREATE response") {
        auto msg = make_n_create_rsp(1, mpps_class, instance_uid, status_success);

        CHECK(msg.command() == command_field::n_create_rsp);
        CHECK(msg.is_response());
        CHECK(msg.message_id_responded_to() == 1);
        CHECK(msg.status() == status_success);
        CHECK(msg.affected_sop_class_uid() == mpps_class);
        CHECK(msg.affected_sop_instance_uid() == instance_uid);
    }

    SECTION("Failed N-CREATE response") {
        auto msg = make_n_create_rsp(2, mpps_class, instance_uid,
                                     status_error_attribute_list_error);

        CHECK(msg.status() == status_error_attribute_list_error);
        CHECK(is_failure(msg.status()));
    }
}

TEST_CASE("N-CREATE encode/decode", "[dimse][message][n-create][codec]") {
    const std::string_view mpps_class = "1.2.840.10008.3.1.2.3.3";
    const std::string_view instance_uid = "1.2.3.4.5.6.7.8.9";

    auto original = make_n_create_rq(42, mpps_class, instance_uid);

    dicom_dataset ds;
    ds.set_string(tags::patient_name, vr_type::PN, "TEST^PATIENT");
    original.set_dataset(std::move(ds));

    auto encoded =
        dimse_message::encode(original, transfer_syntax::implicit_vr_little_endian);
    REQUIRE(encoded.is_ok());

    auto& [cmd_bytes, ds_bytes] = encoded.value();
    CHECK_FALSE(cmd_bytes.empty());
    CHECK_FALSE(ds_bytes.empty());

    auto decoded = dimse_message::decode(cmd_bytes, ds_bytes,
                                         transfer_syntax::implicit_vr_little_endian);
    REQUIRE(decoded.is_ok());

    CHECK(decoded.value().command() == command_field::n_create_rq);
    CHECK(decoded.value().message_id() == 42);
    CHECK(decoded.value().affected_sop_class_uid() == mpps_class);
    CHECK(decoded.value().has_dataset());
    CHECK(decoded.value().dataset().get_string(tags::patient_name) == "TEST^PATIENT");
}

// ============================================================================
// N-SET Tests
// ============================================================================

TEST_CASE("N-SET request message", "[dimse][message][n-set]") {
    const std::string_view mpps_class = "1.2.840.10008.3.1.2.3.3";
    const std::string_view instance_uid = "1.2.3.4.5.6.7.8.9";

    SECTION("Basic N-SET request") {
        auto msg = make_n_set_rq(1, mpps_class, instance_uid);

        CHECK(msg.command() == command_field::n_set_rq);
        CHECK(msg.message_id() == 1);
        CHECK(msg.is_request());
        CHECK(is_dimse_n(msg.command()));
        CHECK(msg.requested_sop_class_uid() == mpps_class);
        CHECK(msg.requested_sop_instance_uid() == instance_uid);
    }

    SECTION("N-SET request with modification list") {
        auto msg = make_n_set_rq(2, mpps_class, instance_uid);

        dicom_dataset modifications;
        modifications.set_string(tags::patient_name, vr_type::PN, "DOE^JANE");
        msg.set_dataset(std::move(modifications));

        CHECK(msg.has_dataset());
        CHECK(msg.dataset().get_string(tags::patient_name) == "DOE^JANE");
    }
}

TEST_CASE("N-SET response message", "[dimse][message][n-set]") {
    const std::string_view mpps_class = "1.2.840.10008.3.1.2.3.3";
    const std::string_view instance_uid = "1.2.3.4.5.6.7.8.9";

    SECTION("Successful N-SET response") {
        auto msg = make_n_set_rsp(1, mpps_class, instance_uid, status_success);

        CHECK(msg.command() == command_field::n_set_rsp);
        CHECK(msg.is_response());
        CHECK(msg.message_id_responded_to() == 1);
        CHECK(msg.status() == status_success);
        // Response uses Affected (not Requested) UIDs
        CHECK(msg.affected_sop_class_uid() == mpps_class);
        CHECK(msg.affected_sop_instance_uid() == instance_uid);
    }

    SECTION("Failed N-SET response - attribute value out of range") {
        auto msg = make_n_set_rsp(2, mpps_class, instance_uid,
                                  status_error_attribute_value_out_of_range);

        CHECK(msg.status() == status_error_attribute_value_out_of_range);
    }
}

TEST_CASE("N-SET encode/decode", "[dimse][message][n-set][codec]") {
    const std::string_view mpps_class = "1.2.840.10008.3.1.2.3.3";
    const std::string_view instance_uid = "1.2.3.4.5.6.7.8.9";

    auto original = make_n_set_rq(10, mpps_class, instance_uid);

    auto encoded =
        dimse_message::encode(original, transfer_syntax::explicit_vr_little_endian);
    REQUIRE(encoded.is_ok());

    auto& [cmd_bytes, ds_bytes] = encoded.value();

    auto decoded = dimse_message::decode(cmd_bytes, ds_bytes,
                                         transfer_syntax::explicit_vr_little_endian);
    REQUIRE(decoded.is_ok());

    CHECK(decoded.value().command() == command_field::n_set_rq);
    // Note: UI VR may have trailing space padding for even length per DICOM
    CHECK(decoded.value().requested_sop_class_uid().starts_with(mpps_class));
    CHECK(decoded.value().requested_sop_instance_uid().starts_with(instance_uid));
}

// ============================================================================
// N-GET Tests
// ============================================================================

TEST_CASE("N-GET request message", "[dimse][message][n-get]") {
    const std::string_view sop_class = "1.2.840.10008.5.1.4.33";  // Instance Availability
    const std::string_view instance_uid = "1.2.3.4.5.6.7.8.9";

    SECTION("N-GET request without attribute list (get all)") {
        auto msg = make_n_get_rq(1, sop_class, instance_uid);

        CHECK(msg.command() == command_field::n_get_rq);
        CHECK(msg.message_id() == 1);
        CHECK(is_dimse_n(msg.command()));
        CHECK(msg.requested_sop_class_uid() == sop_class);
        CHECK(msg.requested_sop_instance_uid() == instance_uid);
        CHECK(msg.attribute_identifier_list().empty());
    }

    SECTION("N-GET request with attribute list") {
        std::vector<dicom_tag> tags_to_get = {
            tags::patient_name, tags::patient_id, tags::study_instance_uid};

        auto msg = make_n_get_rq(2, sop_class, instance_uid, tags_to_get);

        CHECK(msg.command() == command_field::n_get_rq);
        auto retrieved_tags = msg.attribute_identifier_list();
        REQUIRE(retrieved_tags.size() == 3);
        CHECK(retrieved_tags[0] == tags::patient_name);
        CHECK(retrieved_tags[1] == tags::patient_id);
        CHECK(retrieved_tags[2] == tags::study_instance_uid);
    }
}

TEST_CASE("N-GET response message", "[dimse][message][n-get]") {
    const std::string_view sop_class = "1.2.840.10008.5.1.4.33";
    const std::string_view instance_uid = "1.2.3.4.5.6.7.8.9";

    SECTION("Successful N-GET response with attributes") {
        auto msg = make_n_get_rsp(1, sop_class, instance_uid, status_success);

        dicom_dataset attributes;
        attributes.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
        attributes.set_string(tags::patient_id, vr_type::LO, "12345");
        msg.set_dataset(std::move(attributes));

        CHECK(msg.command() == command_field::n_get_rsp);
        CHECK(msg.is_response());
        CHECK(msg.status() == status_success);
        CHECK(msg.has_dataset());
        CHECK(msg.dataset().get_string(tags::patient_name) == "DOE^JOHN");
    }

    SECTION("Failed N-GET response - invalid instance") {
        auto msg = make_n_get_rsp(2, sop_class, instance_uid,
                                  status_error_invalid_object_instance);

        CHECK(msg.status() == status_error_invalid_object_instance);
        CHECK_FALSE(msg.has_dataset());
    }
}

TEST_CASE("N-GET attribute identifier list encode/decode",
          "[dimse][message][n-get][codec]") {
    const std::string_view sop_class = "1.2.840.10008.5.1.4.33";
    const std::string_view instance_uid = "1.2.3.4.5.6.7.8.9";

    std::vector<dicom_tag> tags_to_get = {
        {0x0010, 0x0010},  // Patient Name
        {0x0010, 0x0020},  // Patient ID
        {0x0020, 0x000D}   // Study Instance UID
    };

    auto original = make_n_get_rq(100, sop_class, instance_uid, tags_to_get);

    auto encoded =
        dimse_message::encode(original, transfer_syntax::implicit_vr_little_endian);
    REQUIRE(encoded.is_ok());

    auto& [cmd_bytes, ds_bytes] = encoded.value();

    auto decoded = dimse_message::decode(cmd_bytes, ds_bytes,
                                         transfer_syntax::implicit_vr_little_endian);
    REQUIRE(decoded.is_ok());

    CHECK(decoded.value().command() == command_field::n_get_rq);
    auto decoded_tags = decoded.value().attribute_identifier_list();
    REQUIRE(decoded_tags.size() == 3);
    CHECK(decoded_tags[0] == dicom_tag{0x0010, 0x0010});
    CHECK(decoded_tags[1] == dicom_tag{0x0010, 0x0020});
    CHECK(decoded_tags[2] == dicom_tag{0x0020, 0x000D});
}

// ============================================================================
// N-EVENT-REPORT Tests
// ============================================================================

TEST_CASE("N-EVENT-REPORT request message", "[dimse][message][n-event-report]") {
    // Storage Commitment Push Model SOP Class
    const std::string_view sc_class = "1.2.840.10008.1.20.1";
    const std::string_view transaction_uid = "1.2.3.4.5.6.7.8.9";
    constexpr uint16_t event_storage_commitment_success = 1;

    SECTION("Basic N-EVENT-REPORT request") {
        auto msg = make_n_event_report_rq(1, sc_class, transaction_uid,
                                          event_storage_commitment_success);

        CHECK(msg.command() == command_field::n_event_report_rq);
        CHECK(msg.message_id() == 1);
        CHECK(is_dimse_n(msg.command()));
        CHECK(msg.affected_sop_class_uid() == sc_class);
        CHECK(msg.affected_sop_instance_uid() == transaction_uid);
        REQUIRE(msg.event_type_id().has_value());
        CHECK(msg.event_type_id().value() == event_storage_commitment_success);
    }

    SECTION("N-EVENT-REPORT request with event information") {
        auto msg = make_n_event_report_rq(2, sc_class, transaction_uid, 2);

        dicom_dataset event_info;
        // Transaction UID (0008,1195)
        event_info.set_string(dicom_tag{0x0008, 0x1195}, vr_type::UI, transaction_uid);
        msg.set_dataset(std::move(event_info));

        CHECK(msg.has_dataset());
        CHECK(msg.event_type_id().value() == 2);
    }
}

TEST_CASE("N-EVENT-REPORT response message", "[dimse][message][n-event-report]") {
    const std::string_view sc_class = "1.2.840.10008.1.20.1";
    const std::string_view transaction_uid = "1.2.3.4.5.6.7.8.9";

    SECTION("Successful N-EVENT-REPORT response") {
        auto msg = make_n_event_report_rsp(1, sc_class, transaction_uid, 1, status_success);

        CHECK(msg.command() == command_field::n_event_report_rsp);
        CHECK(msg.is_response());
        CHECK(msg.message_id_responded_to() == 1);
        CHECK(msg.status() == status_success);
        CHECK(msg.event_type_id().value() == 1);
    }
}

TEST_CASE("N-EVENT-REPORT encode/decode", "[dimse][message][n-event-report][codec]") {
    const std::string_view sc_class = "1.2.840.10008.1.20.1";
    const std::string_view transaction_uid = "1.2.3.4.5.6.7.8.9";

    auto original = make_n_event_report_rq(50, sc_class, transaction_uid, 1);

    auto encoded =
        dimse_message::encode(original, transfer_syntax::implicit_vr_little_endian);
    REQUIRE(encoded.is_ok());

    auto& [cmd_bytes, ds_bytes] = encoded.value();

    auto decoded = dimse_message::decode(cmd_bytes, ds_bytes,
                                         transfer_syntax::implicit_vr_little_endian);
    REQUIRE(decoded.is_ok());

    CHECK(decoded.value().command() == command_field::n_event_report_rq);
    CHECK(decoded.value().message_id() == 50);
    CHECK(decoded.value().affected_sop_class_uid() == sc_class);
    CHECK(decoded.value().affected_sop_instance_uid() == transaction_uid);
    REQUIRE(decoded.value().event_type_id().has_value());
    CHECK(decoded.value().event_type_id().value() == 1);
}

// ============================================================================
// N-ACTION Tests
// ============================================================================

TEST_CASE("N-ACTION request message", "[dimse][message][n-action]") {
    // Storage Commitment Push Model SOP Class
    const std::string_view sc_class = "1.2.840.10008.1.20.1";
    const std::string_view sc_instance = "1.2.840.10008.1.20.1.1";
    constexpr uint16_t action_request_storage_commitment = 1;

    SECTION("Basic N-ACTION request") {
        auto msg = make_n_action_rq(1, sc_class, sc_instance,
                                    action_request_storage_commitment);

        CHECK(msg.command() == command_field::n_action_rq);
        CHECK(msg.message_id() == 1);
        CHECK(is_dimse_n(msg.command()));
        CHECK(msg.requested_sop_class_uid() == sc_class);
        CHECK(msg.requested_sop_instance_uid() == sc_instance);
        REQUIRE(msg.action_type_id().has_value());
        CHECK(msg.action_type_id().value() == action_request_storage_commitment);
    }

    SECTION("N-ACTION request with action information") {
        auto msg = make_n_action_rq(2, sc_class, sc_instance, 1);

        dicom_dataset action_info;
        // Transaction UID (0008,1195)
        action_info.set_string(dicom_tag{0x0008, 0x1195}, vr_type::UI, "1.2.3.4.5");
        msg.set_dataset(std::move(action_info));

        CHECK(msg.has_dataset());
    }
}

TEST_CASE("N-ACTION response message", "[dimse][message][n-action]") {
    const std::string_view sc_class = "1.2.840.10008.1.20.1";
    const std::string_view transaction_uid = "1.2.3.4.5.6.7.8.9";

    SECTION("Successful N-ACTION response") {
        auto msg = make_n_action_rsp(1, sc_class, transaction_uid, 1, status_success);

        CHECK(msg.command() == command_field::n_action_rsp);
        CHECK(msg.is_response());
        CHECK(msg.message_id_responded_to() == 1);
        CHECK(msg.status() == status_success);
        // Response uses Affected UIDs
        CHECK(msg.affected_sop_class_uid() == sc_class);
        CHECK(msg.affected_sop_instance_uid() == transaction_uid);
        CHECK(msg.action_type_id().value() == 1);
    }

    SECTION("Failed N-ACTION response - no such action type") {
        auto msg =
            make_n_action_rsp(2, sc_class, transaction_uid, 99, status_error_no_such_action_type);

        CHECK(msg.status() == status_error_no_such_action_type);
    }
}

TEST_CASE("N-ACTION encode/decode", "[dimse][message][n-action][codec]") {
    const std::string_view sc_class = "1.2.840.10008.1.20.1";
    const std::string_view sc_instance = "1.2.840.10008.1.20.1.1";

    auto original = make_n_action_rq(75, sc_class, sc_instance, 1);

    auto encoded =
        dimse_message::encode(original, transfer_syntax::explicit_vr_little_endian);
    REQUIRE(encoded.is_ok());

    auto& [cmd_bytes, ds_bytes] = encoded.value();

    auto decoded = dimse_message::decode(cmd_bytes, ds_bytes,
                                         transfer_syntax::explicit_vr_little_endian);
    REQUIRE(decoded.is_ok());

    CHECK(decoded.value().command() == command_field::n_action_rq);
    CHECK(decoded.value().message_id() == 75);
    CHECK(decoded.value().requested_sop_class_uid() == sc_class);
    CHECK(decoded.value().requested_sop_instance_uid() == sc_instance);
    CHECK(decoded.value().action_type_id().value() == 1);
}

// ============================================================================
// N-DELETE Tests
// ============================================================================

TEST_CASE("N-DELETE request message", "[dimse][message][n-delete]") {
    // Basic Film Session SOP Class (Print Management)
    const std::string_view print_class = "1.2.840.10008.5.1.1.1";
    const std::string_view session_uid = "1.2.3.4.5.6.7.8.9";

    SECTION("Basic N-DELETE request") {
        auto msg = make_n_delete_rq(1, print_class, session_uid);

        CHECK(msg.command() == command_field::n_delete_rq);
        CHECK(msg.message_id() == 1);
        CHECK(is_dimse_n(msg.command()));
        CHECK(msg.requested_sop_class_uid() == print_class);
        CHECK(msg.requested_sop_instance_uid() == session_uid);
        // N-DELETE has no data set
        CHECK_FALSE(msg.has_dataset());
    }
}

TEST_CASE("N-DELETE response message", "[dimse][message][n-delete]") {
    const std::string_view print_class = "1.2.840.10008.5.1.1.1";
    const std::string_view session_uid = "1.2.3.4.5.6.7.8.9";

    SECTION("Successful N-DELETE response") {
        auto msg = make_n_delete_rsp(1, print_class, session_uid, status_success);

        CHECK(msg.command() == command_field::n_delete_rsp);
        CHECK(msg.is_response());
        CHECK(msg.message_id_responded_to() == 1);
        CHECK(msg.status() == status_success);
        CHECK(msg.affected_sop_class_uid() == print_class);
        CHECK(msg.affected_sop_instance_uid() == session_uid);
    }

    SECTION("Failed N-DELETE response - invalid object instance") {
        auto msg =
            make_n_delete_rsp(2, print_class, session_uid, status_error_invalid_object_instance);

        CHECK(msg.status() == status_error_invalid_object_instance);
    }
}

TEST_CASE("N-DELETE encode/decode", "[dimse][message][n-delete][codec]") {
    const std::string_view print_class = "1.2.840.10008.5.1.1.1";
    const std::string_view session_uid = "1.2.3.4.5.6.7.8.9";

    auto original = make_n_delete_rq(99, print_class, session_uid);

    auto encoded =
        dimse_message::encode(original, transfer_syntax::implicit_vr_little_endian);
    REQUIRE(encoded.is_ok());

    auto& [cmd_bytes, ds_bytes] = encoded.value();
    CHECK_FALSE(cmd_bytes.empty());
    CHECK(ds_bytes.empty());  // N-DELETE has no data set

    auto decoded = dimse_message::decode(cmd_bytes, ds_bytes,
                                         transfer_syntax::implicit_vr_little_endian);
    REQUIRE(decoded.is_ok());

    CHECK(decoded.value().command() == command_field::n_delete_rq);
    CHECK(decoded.value().message_id() == 99);
    // Note: UI VR may have trailing space padding for even length per DICOM
    CHECK(decoded.value().requested_sop_class_uid().starts_with(print_class));
    CHECK(decoded.value().requested_sop_instance_uid().starts_with(session_uid));
}

// ============================================================================
// Cross-service Tests
// ============================================================================

TEST_CASE("All DIMSE-N commands are correctly classified", "[dimse][command_field]") {
    CHECK(is_dimse_n(command_field::n_create_rq));
    CHECK(is_dimse_n(command_field::n_create_rsp));
    CHECK(is_dimse_n(command_field::n_set_rq));
    CHECK(is_dimse_n(command_field::n_set_rsp));
    CHECK(is_dimse_n(command_field::n_get_rq));
    CHECK(is_dimse_n(command_field::n_get_rsp));
    CHECK(is_dimse_n(command_field::n_event_report_rq));
    CHECK(is_dimse_n(command_field::n_event_report_rsp));
    CHECK(is_dimse_n(command_field::n_action_rq));
    CHECK(is_dimse_n(command_field::n_action_rsp));
    CHECK(is_dimse_n(command_field::n_delete_rq));
    CHECK(is_dimse_n(command_field::n_delete_rsp));

    // Verify they are not DIMSE-C
    CHECK_FALSE(is_dimse_c(command_field::n_create_rq));
    CHECK_FALSE(is_dimse_c(command_field::n_set_rq));
    CHECK_FALSE(is_dimse_c(command_field::n_get_rq));
}

TEST_CASE("DIMSE-N request/response conversion", "[dimse][command_field]") {
    CHECK(get_response_command(command_field::n_create_rq) == command_field::n_create_rsp);
    CHECK(get_response_command(command_field::n_set_rq) == command_field::n_set_rsp);
    CHECK(get_response_command(command_field::n_get_rq) == command_field::n_get_rsp);
    CHECK(get_response_command(command_field::n_event_report_rq) ==
          command_field::n_event_report_rsp);
    CHECK(get_response_command(command_field::n_action_rq) == command_field::n_action_rsp);
    CHECK(get_response_command(command_field::n_delete_rq) == command_field::n_delete_rsp);

    CHECK(get_request_command(command_field::n_create_rsp) == command_field::n_create_rq);
    CHECK(get_request_command(command_field::n_set_rsp) == command_field::n_set_rq);
    CHECK(get_request_command(command_field::n_get_rsp) == command_field::n_get_rq);
}

TEST_CASE("DIMSE-N command to_string", "[dimse][command_field]") {
    CHECK(to_string(command_field::n_create_rq) == "N-CREATE-RQ");
    CHECK(to_string(command_field::n_create_rsp) == "N-CREATE-RSP");
    CHECK(to_string(command_field::n_set_rq) == "N-SET-RQ");
    CHECK(to_string(command_field::n_set_rsp) == "N-SET-RSP");
    CHECK(to_string(command_field::n_get_rq) == "N-GET-RQ");
    CHECK(to_string(command_field::n_get_rsp) == "N-GET-RSP");
    CHECK(to_string(command_field::n_event_report_rq) == "N-EVENT-REPORT-RQ");
    CHECK(to_string(command_field::n_event_report_rsp) == "N-EVENT-REPORT-RSP");
    CHECK(to_string(command_field::n_action_rq) == "N-ACTION-RQ");
    CHECK(to_string(command_field::n_action_rsp) == "N-ACTION-RSP");
    CHECK(to_string(command_field::n_delete_rq) == "N-DELETE-RQ");
    CHECK(to_string(command_field::n_delete_rsp) == "N-DELETE-RSP");
}
