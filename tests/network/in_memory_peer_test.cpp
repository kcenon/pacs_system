/**
 * @file in_memory_peer_test.cpp
 * @brief Lifetime tests for the in-memory association pairing behind the
 *        dicom_server test hook
 *
 * association::connect() reaches an in-process dicom_server and pairs the
 * client association with a server association. These tests destroy,
 * release, abort, and move either end and check that the other end fails
 * cleanly instead of touching freed memory. The ends live on the heap, so
 * AddressSanitizer reports any access to an end after it is destroyed.
 *
 * @see Issue #1245 - Lifetime hazards in the in-memory dicom_server test hook
 */

#include <catch2/catch_test_macros.hpp>

#include "kcenon/pacs/core/result.h"
#include "kcenon/pacs/network/association.h"
#include "kcenon/pacs/network/dicom_server.h"
#include "kcenon/pacs/network/dimse/dimse_message.h"
#include "kcenon/pacs/network/server_config.h"
#include "kcenon/pacs/services/scp_service.h"

#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <thread>

using namespace kcenon::pacs::network;
using kcenon::pacs::error_codes::association_aborted;

// =============================================================================
// Test Helpers
// =============================================================================

namespace {

constexpr const char* VERIFICATION_SOP_CLASS = "1.2.840.10008.1.1";
constexpr const char* EXPLICIT_VR_LE = "1.2.840.10008.1.2.1";
constexpr uint8_t CONTEXT_ID = 1;

/// No dicom_server listens here, so connect() returns an unpaired SCU
constexpr uint16_t UNSERVED_PORT = 104;

/// Port for the dicom_server end-to-end test
constexpr uint16_t SERVER_PORT = 11145;

association_config make_scu_config() {
    association_config config;
    config.calling_ae_title = "PEER_SCU";
    config.called_ae_title = "PEER_SCP";
    config.proposed_contexts.push_back(
        {CONTEXT_ID, VERIFICATION_SOP_CLASS, {EXPLICIT_VR_LE}});
    return config;
}

/// An SCU and an SCP association paired the way dicom_server pairs them
struct paired_associations {
    std::unique_ptr<association> scu;
    std::unique_ptr<association> scp;
};

paired_associations make_paired_associations() {
    auto connect_result =
        association::connect("localhost", UNSERVED_PORT, make_scu_config());
    REQUIRE(connect_result.is_ok());
    auto scu = std::make_unique<association>(std::move(connect_result.value()));

    scp_config config;
    config.ae_title = "PEER_SCP";
    config.supported_abstract_syntaxes = {VERIFICATION_SOP_CLASS};
    config.supported_transfer_syntaxes = {EXPLICIT_VR_LE};
    auto scp = std::make_unique<association>(
        association::accept(scu->build_associate_rq(), config));
    REQUIRE(scp->is_established());
    REQUIRE(scu->process_associate_ac(scp->build_associate_ac()));

    scp->set_peer(scu.get());
    return {std::move(scu), std::move(scp)};
}

/**
 * @brief Verification SCP that holds its C-ECHO response until the test
 *        lets it go, then reports what sending the response returned.
 */
class held_echo_scp final : public kcenon::pacs::services::scp_service {
public:
    [[nodiscard]] std::vector<std::string> supported_sop_classes() const override {
        return {VERIFICATION_SOP_CLASS};
    }

    [[nodiscard]] Result<std::monostate> handle_message(
        association& assoc,
        uint8_t context_id,
        const dimse::dimse_message& request) override {
        request_received_.set_value();
        if (respond_.wait_for(std::chrono::seconds{30}) != std::future_status::ready) {
            return std::monostate{};  // The test failed before letting go
        }

        auto result = assoc.send_dimse(
            context_id, dimse::make_c_echo_rsp(request.message_id()));
        send_error_.set_value(result.is_err()
            ? std::optional<int>{result.error().code}
            : std::nullopt);
        return result;
    }

    [[nodiscard]] std::string_view service_name() const noexcept override {
        return "Held Echo SCP";
    }

    std::future<void> request_received() { return request_received_.get_future(); }
    void respond() { respond_promise_.set_value(); }

    /// Error code the response send returned, or std::nullopt on success
    std::future<std::optional<int>> send_error() { return send_error_.get_future(); }

private:
    std::promise<void> request_received_;
    std::promise<void> respond_promise_;
    std::shared_future<void> respond_{respond_promise_.get_future()};
    std::promise<std::optional<int>> send_error_;
};

}  // namespace

// =============================================================================
// Destroying Either End
// =============================================================================

TEST_CASE("in-memory peer: SCP send after the SCU is destroyed fails cleanly",
          "[association][in_memory][lifetime]") {
    auto pair = make_paired_associations();
    pair.scu.reset();

    auto result = pair.scp->send_dimse(CONTEXT_ID, dimse::make_c_echo_rsp(1));
    REQUIRE(result.is_err());
    CHECK(result.error().code == association_aborted);
    CHECK(pair.scp->state() == association_state::aborted);
}

TEST_CASE("in-memory peer: SCU send after the SCP is destroyed fails cleanly",
          "[association][in_memory][lifetime]") {
    auto pair = make_paired_associations();
    pair.scp.reset();

    auto result = pair.scu->send_dimse(CONTEXT_ID, dimse::make_c_echo_rq(1));
    REQUIRE(result.is_err());
    CHECK(result.error().code == association_aborted);
    CHECK(pair.scu->state() == association_state::aborted);
}

// =============================================================================
// Release and Abort
// =============================================================================

TEST_CASE("in-memory peer: release and abort reach the other end",
          "[association][in_memory][lifetime]") {
    auto pair = make_paired_associations();

    SECTION("SCU release ends the SCP without waiting for its timeout") {
        REQUIRE(pair.scu->release().is_ok());

        auto start = std::chrono::steady_clock::now();
        auto received = pair.scp->receive_dimse(std::chrono::seconds{10});
        auto waited = std::chrono::steady_clock::now() - start;

        REQUIRE(received.is_err());
        CHECK(received.error().code == association_aborted);
        CHECK(waited < std::chrono::seconds{5});
        CHECK(pair.scp->state() == association_state::released);
        CHECK(pair.scp->send_dimse(CONTEXT_ID, dimse::make_c_echo_rsp(1)).is_err());
    }

    SECTION("SCU abort aborts the SCP") {
        pair.scu->abort();

        auto received = pair.scp->receive_dimse(std::chrono::seconds{10});
        REQUIRE(received.is_err());
        CHECK(pair.scp->state() == association_state::aborted);
    }

    SECTION("SCP release ends the SCU") {
        REQUIRE(pair.scp->release().is_ok());

        auto result = pair.scu->send_dimse(CONTEXT_ID, dimse::make_c_echo_rq(1));
        REQUIRE(result.is_err());
        CHECK(result.error().code == association_aborted);
        CHECK(pair.scu->state() == association_state::released);
    }
}

TEST_CASE("in-memory peer: messages queued before a release are delivered",
          "[association][in_memory][lifetime]") {
    auto pair = make_paired_associations();
    REQUIRE(pair.scu->send_dimse(CONTEXT_ID, dimse::make_c_echo_rq(7)).is_ok());
    REQUIRE(pair.scu->release().is_ok());

    auto first = pair.scp->receive_dimse(std::chrono::seconds{1});
    REQUIRE(first.is_ok());
    CHECK(first.value().second.message_id() == 7);

    auto second = pair.scp->receive_dimse(std::chrono::seconds{1});
    REQUIRE(second.is_err());
    CHECK(pair.scp->state() == association_state::released);
}

// =============================================================================
// Moves and Unpaired Associations
// =============================================================================

TEST_CASE("in-memory peer: moving an end keeps the pairing",
          "[association][in_memory][lifetime]") {
    auto pair = make_paired_associations();

    // Move construction; destroying the moved-from object must not unpair
    auto moved_scu = std::make_unique<association>(std::move(*pair.scu));
    pair.scu.reset();
    REQUIRE(pair.scp->send_dimse(CONTEXT_ID, dimse::make_c_echo_rsp(3)).is_ok());
    auto at_scu = moved_scu->receive_dimse(std::chrono::seconds{1});
    REQUIRE(at_scu.is_ok());
    CHECK(at_scu.value().second.message_id_responded_to() == 3);

    // Move assignment
    auto assigned_scu = std::make_unique<association>();
    *assigned_scu = std::move(*moved_scu);
    moved_scu.reset();
    REQUIRE(assigned_scu->send_dimse(CONTEXT_ID, dimse::make_c_echo_rq(4)).is_ok());
    auto at_scp = pair.scp->receive_dimse(std::chrono::seconds{1});
    REQUIRE(at_scp.is_ok());
    CHECK(at_scp.value().second.message_id() == 4);
}

TEST_CASE("in-memory peer: an association that was never paired still sends",
          "[association][in_memory]") {
    // Without a peer, send_dimse() keeps its placeholder behavior (no
    // transport yet), so only a peer that left makes it fail
    auto connect_result =
        association::connect("localhost", UNSERVED_PORT, make_scu_config());
    REQUIRE(connect_result.is_ok());
    auto& scu = connect_result.value();

    associate_ac ac;
    ac.calling_ae_title = "PEER_SCU";
    ac.called_ae_title = "PEER_SCP";
    ac.application_context = DICOM_APPLICATION_CONTEXT;
    ac.presentation_contexts.push_back(
        {CONTEXT_ID, presentation_context_result::acceptance, EXPLICIT_VR_LE});
    REQUIRE(scu.process_associate_ac(ac));

    CHECK(scu.send_dimse(CONTEXT_ID, dimse::make_c_echo_rq(1)).is_ok());
}

// =============================================================================
// dicom_server End-to-End
// =============================================================================

TEST_CASE("dicom_server: a response to a destroyed SCU fails cleanly",
          "[dicom_server][in_memory][lifetime]") {
    server_config config;
    config.ae_title = "PEER_SCP";
    config.port = SERVER_PORT;

    auto service = std::make_shared<held_echo_scp>();
    auto request_received = service->request_received();
    auto send_error = service->send_error();

    dicom_server server{config};
    server.register_service(service);
    REQUIRE(server.start().is_ok());

    auto connect_result =
        association::connect("localhost", SERVER_PORT, make_scu_config());
    REQUIRE(connect_result.is_ok());
    auto scu = std::make_unique<association>(std::move(connect_result.value()));

    // Let the server's message loop block in its receive first: the queue
    // then hands the request over through its condition variable, which
    // ThreadSanitizer can follow (kcenon/thread_system#761)
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    REQUIRE(scu->send_dimse(CONTEXT_ID, dimse::make_c_echo_rq(1)).is_ok());
    REQUIRE(request_received.wait_for(std::chrono::seconds{10}) ==
            std::future_status::ready);

    // Destroy the SCU while the server still owes it the response
    scu.reset();
    service->respond();

    REQUIRE(send_error.wait_for(std::chrono::seconds{10}) ==
            std::future_status::ready);
    auto error_code = send_error.get();
    REQUIRE(error_code.has_value());
    CHECK(*error_code == association_aborted);

    server.stop();
}
