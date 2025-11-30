/**
 * @file dimse_message_test.cpp
 * @brief Unit tests for DIMSE message class
 */

#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
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
// Command Field Tests
// ============================================================================

TEST_CASE("command_field request/response detection", "[dimse][command_field]") {
    SECTION("C-STORE commands") {
        CHECK(is_request(command_field::c_store_rq));
        CHECK_FALSE(is_response(command_field::c_store_rq));

        CHECK_FALSE(is_request(command_field::c_store_rsp));
        CHECK(is_response(command_field::c_store_rsp));
    }

    SECTION("C-ECHO commands") {
        CHECK(is_request(command_field::c_echo_rq));
        CHECK(is_response(command_field::c_echo_rsp));
    }

    SECTION("C-FIND commands") {
        CHECK(is_request(command_field::c_find_rq));
        CHECK(is_response(command_field::c_find_rsp));
    }

    SECTION("N-CREATE commands") {
        CHECK(is_request(command_field::n_create_rq));
        CHECK(is_response(command_field::n_create_rsp));
    }
}

TEST_CASE("command_field DIMSE-C/N classification", "[dimse][command_field]") {
    SECTION("DIMSE-C commands") {
        CHECK(is_dimse_c(command_field::c_store_rq));
        CHECK(is_dimse_c(command_field::c_echo_rq));
        CHECK(is_dimse_c(command_field::c_find_rq));
        CHECK(is_dimse_c(command_field::c_move_rq));
        CHECK(is_dimse_c(command_field::c_get_rq));
        CHECK(is_dimse_c(command_field::c_cancel_rq));
    }

    SECTION("DIMSE-N commands") {
        CHECK(is_dimse_n(command_field::n_create_rq));
        CHECK(is_dimse_n(command_field::n_delete_rq));
        CHECK(is_dimse_n(command_field::n_set_rq));
        CHECK(is_dimse_n(command_field::n_get_rq));
        CHECK(is_dimse_n(command_field::n_event_report_rq));
        CHECK(is_dimse_n(command_field::n_action_rq));
    }
}

TEST_CASE("command_field response/request conversion", "[dimse][command_field]") {
    CHECK(get_response_command(command_field::c_store_rq) == command_field::c_store_rsp);
    CHECK(get_response_command(command_field::c_echo_rq) == command_field::c_echo_rsp);
    CHECK(get_response_command(command_field::c_find_rq) == command_field::c_find_rsp);

    CHECK(get_request_command(command_field::c_store_rsp) == command_field::c_store_rq);
    CHECK(get_request_command(command_field::c_echo_rsp) == command_field::c_echo_rq);
}

TEST_CASE("command_field to_string", "[dimse][command_field]") {
    CHECK(to_string(command_field::c_store_rq) == "C-STORE-RQ");
    CHECK(to_string(command_field::c_store_rsp) == "C-STORE-RSP");
    CHECK(to_string(command_field::c_echo_rq) == "C-ECHO-RQ");
    CHECK(to_string(command_field::c_echo_rsp) == "C-ECHO-RSP");
    CHECK(to_string(command_field::n_create_rq) == "N-CREATE-RQ");
}

// ============================================================================
// Status Codes Tests
// ============================================================================

TEST_CASE("status_codes classification", "[dimse][status]") {
    SECTION("Success status") {
        CHECK(is_success(status_success));
        CHECK_FALSE(is_pending(status_success));
        CHECK_FALSE(is_failure(status_success));
    }

    SECTION("Pending status") {
        CHECK(is_pending(status_pending));
        CHECK(is_pending(status_pending_warning));
        CHECK_FALSE(is_success(status_pending));
        CHECK_FALSE(is_failure(status_pending));
    }

    SECTION("Failure status") {
        CHECK(is_failure(status_refused_out_of_resources));
        CHECK(is_failure(status_error_cannot_understand));
        CHECK_FALSE(is_success(status_refused_out_of_resources));
        CHECK_FALSE(is_pending(status_error_cannot_understand));
    }

    SECTION("Cancel status") {
        CHECK(is_cancel(status_cancel));
        CHECK_FALSE(is_success(status_cancel));
    }

    SECTION("Warning status") {
        CHECK(is_warning(status_warning_coercion));
        CHECK_FALSE(is_failure(status_warning_coercion));
    }
}

TEST_CASE("status_codes final check", "[dimse][status]") {
    CHECK(is_final(status_success));
    CHECK(is_final(status_cancel));
    CHECK(is_final(status_refused_out_of_resources));

    CHECK_FALSE(is_final(status_pending));
    CHECK_FALSE(is_final(status_pending_warning));
}

TEST_CASE("status_codes description", "[dimse][status]") {
    CHECK(status_description(status_success) == "Success");
    CHECK(status_description(status_pending) == "Pending");
    CHECK(status_description(status_cancel) == "Canceled");
    CHECK(status_category(status_success) == "Success");
    CHECK(status_category(status_pending) == "Pending");
}

// ============================================================================
// DIMSE Message Construction Tests
// ============================================================================

TEST_CASE("dimse_message basic construction", "[dimse][message]") {
    dimse_message msg(command_field::c_echo_rq, 1);

    CHECK(msg.command() == command_field::c_echo_rq);
    CHECK(msg.message_id() == 1);
    CHECK(msg.is_valid());
    CHECK(msg.is_request());
    CHECK_FALSE(msg.is_response());
    CHECK_FALSE(msg.has_dataset());
}

TEST_CASE("dimse_message C-ECHO request", "[dimse][message][c-echo]") {
    auto msg = make_c_echo_rq(42);

    CHECK(msg.command() == command_field::c_echo_rq);
    CHECK(msg.message_id() == 42);
    CHECK(msg.affected_sop_class_uid() == "1.2.840.10008.1.1");
    CHECK_FALSE(msg.has_dataset());
}

TEST_CASE("dimse_message C-ECHO response", "[dimse][message][c-echo]") {
    auto msg = make_c_echo_rsp(42, status_success);

    CHECK(msg.command() == command_field::c_echo_rsp);
    CHECK(msg.is_response());
    CHECK(msg.message_id_responded_to() == 42);
    CHECK(msg.status() == status_success);
    CHECK(msg.affected_sop_class_uid() == "1.2.840.10008.1.1");
}

TEST_CASE("dimse_message C-STORE request", "[dimse][message][c-store]") {
    const std::string_view sop_class = "1.2.840.10008.5.1.4.1.1.2";  // CT Image
    const std::string_view sop_instance = "1.2.3.4.5.6.7.8.9";

    auto msg = make_c_store_rq(1, sop_class, sop_instance);

    CHECK(msg.command() == command_field::c_store_rq);
    CHECK(msg.message_id() == 1);
    CHECK(msg.affected_sop_class_uid() == sop_class);
    CHECK(msg.affected_sop_instance_uid() == sop_instance);
    CHECK(msg.priority() == priority_medium);

    SECTION("Set high priority") {
        msg.set_priority(priority_high);
        CHECK(msg.priority() == priority_high);
    }
}

TEST_CASE("dimse_message C-STORE response", "[dimse][message][c-store]") {
    const std::string_view sop_class = "1.2.840.10008.5.1.4.1.1.2";
    const std::string_view sop_instance = "1.2.3.4.5.6.7.8.9";

    auto msg = make_c_store_rsp(1, sop_class, sop_instance, status_success);

    CHECK(msg.command() == command_field::c_store_rsp);
    CHECK(msg.is_response());
    CHECK(msg.message_id_responded_to() == 1);
    CHECK(msg.status() == status_success);
}

TEST_CASE("dimse_message C-FIND with dataset", "[dimse][message][c-find]") {
    const std::string_view qr_find_class = "1.2.840.10008.5.1.4.1.2.1.1";

    auto msg = make_c_find_rq(5, qr_find_class);

    // Add query dataset
    dicom_dataset query;
    query.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
    query.set_string(tags::patient_id, vr_type::LO, "12345");
    msg.set_dataset(std::move(query));

    CHECK(msg.has_dataset());
    CHECK(msg.dataset().get_string(tags::patient_name) == "DOE^JOHN");
    CHECK(msg.dataset().get_string(tags::patient_id) == "12345");
}

TEST_CASE("dimse_message dataset management", "[dimse][message]") {
    dimse_message msg(command_field::c_store_rq, 1);

    SECTION("Initially no dataset") {
        CHECK_FALSE(msg.has_dataset());
        CHECK_THROWS(msg.dataset());
    }

    SECTION("Add dataset") {
        dicom_dataset ds;
        ds.set_string(tags::patient_name, vr_type::PN, "TEST^PATIENT");
        msg.set_dataset(std::move(ds));

        CHECK(msg.has_dataset());
        CHECK(msg.dataset().get_string(tags::patient_name) == "TEST^PATIENT");
    }

    SECTION("Clear dataset") {
        dicom_dataset ds;
        msg.set_dataset(std::move(ds));
        CHECK(msg.has_dataset());

        msg.clear_dataset();
        CHECK_FALSE(msg.has_dataset());
    }
}

// ============================================================================
// DIMSE Message Encoding/Decoding Tests
// ============================================================================

TEST_CASE("dimse_message C-ECHO encode/decode", "[dimse][message][codec]") {
    auto original = make_c_echo_rq(42);

    auto encoded = dimse_message::encode(
        original, transfer_syntax::implicit_vr_little_endian);
    REQUIRE(encoded.has_value());

    auto& [command_bytes, dataset_bytes] = *encoded;
    CHECK_FALSE(command_bytes.empty());
    CHECK(dataset_bytes.empty());  // C-ECHO has no dataset

    auto decoded = dimse_message::decode(
        command_bytes, dataset_bytes, transfer_syntax::implicit_vr_little_endian);
    REQUIRE(decoded.has_value());

    CHECK(decoded->command() == command_field::c_echo_rq);
    CHECK(decoded->message_id() == 42);
    CHECK(decoded->affected_sop_class_uid() == "1.2.840.10008.1.1");
}

TEST_CASE("dimse_message C-STORE with dataset encode/decode",
          "[dimse][message][codec]") {
    const std::string_view sop_class = "1.2.840.10008.5.1.4.1.1.2";
    const std::string_view sop_instance = "1.2.3.4.5.6.7.8.9";

    auto original = make_c_store_rq(1, sop_class, sop_instance);

    // Add a dataset
    dicom_dataset ds;
    ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
    ds.set_string(tags::patient_id, vr_type::LO, "12345");
    original.set_dataset(std::move(ds));

    // Encode with Explicit VR LE
    auto encoded = dimse_message::encode(
        original, transfer_syntax::explicit_vr_little_endian);
    REQUIRE(encoded.has_value());

    auto& [command_bytes, dataset_bytes] = *encoded;
    CHECK_FALSE(command_bytes.empty());
    CHECK_FALSE(dataset_bytes.empty());

    // Decode
    auto decoded = dimse_message::decode(
        command_bytes, dataset_bytes, transfer_syntax::explicit_vr_little_endian);
    REQUIRE(decoded.has_value());

    CHECK(decoded->command() == command_field::c_store_rq);
    CHECK(decoded->message_id() == 1);
    CHECK(decoded->affected_sop_class_uid() == sop_class);
    CHECK(decoded->affected_sop_instance_uid() == sop_instance);
    CHECK(decoded->has_dataset());
    CHECK(decoded->dataset().get_string(tags::patient_name) == "DOE^JOHN");
}

TEST_CASE("dimse_message response with status encode/decode",
          "[dimse][message][codec]") {
    auto original = make_c_echo_rsp(42, status_success);

    auto encoded = dimse_message::encode(
        original, transfer_syntax::implicit_vr_little_endian);
    REQUIRE(encoded.has_value());

    auto& [command_bytes, dataset_bytes] = *encoded;

    auto decoded = dimse_message::decode(
        command_bytes, dataset_bytes, transfer_syntax::implicit_vr_little_endian);
    REQUIRE(decoded.has_value());

    CHECK(decoded->command() == command_field::c_echo_rsp);
    CHECK(decoded->is_response());
    CHECK(decoded->message_id_responded_to() == 42);
    CHECK(decoded->status() == status_success);
}

// ============================================================================
// Sub-operation Counts Tests
// ============================================================================

TEST_CASE("dimse_message sub-operation counts", "[dimse][message]") {
    dimse_message msg(command_field::c_move_rsp, 0);
    msg.set_message_id_responded_to(1);
    msg.set_status(status_pending);

    SECTION("Set and get remaining") {
        CHECK_FALSE(msg.remaining_subops().has_value());
        msg.set_remaining_subops(10);
        CHECK(msg.remaining_subops().value() == 10);
    }

    SECTION("Set and get completed") {
        msg.set_completed_subops(5);
        CHECK(msg.completed_subops().value() == 5);
    }

    SECTION("Set and get failed") {
        msg.set_failed_subops(2);
        CHECK(msg.failed_subops().value() == 2);
    }

    SECTION("Set and get warning") {
        msg.set_warning_subops(1);
        CHECK(msg.warning_subops().value() == 1);
    }
}
