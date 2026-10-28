/**
 * @file association_tcp_test.cpp
 * @brief Tests for the TCP transport of association
 *
 * The SCU connects over TCP to a scripted peer, which sends chosen bytes and
 * records what the SCU sends back, and to dicom_server_v2. They check the
 * A-ASSOCIATE exchange, P-DATA-TF fragmentation and reassembly, release in
 * both directions, and the A-ABORT the SCU sends for malformed or
 * unexpected PDUs.
 *
 * @see Issue #1240 - Real TCP client path in association::connect()
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "kcenon/pacs/core/dicom_dataset.h"
#include "kcenon/pacs/core/dicom_element.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/core/result.h"
#include "kcenon/pacs/network/association.h"
#include "kcenon/pacs/network/dimse/dimse_message.h"
#include "kcenon/pacs/network/pdu_decoder.h"
#include "kcenon/pacs/network/pdu_encoder.h"
#include "kcenon/pacs/network/server_config.h"
#include "kcenon/pacs/network/v2/dicom_server_v2.h"
#include "kcenon/pacs/services/storage_scp.h"
#include "kcenon/pacs/services/storage_scu.h"
#include "kcenon/pacs/services/verification_scp.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace kcenon::pacs;
using namespace kcenon::pacs::network;
using namespace std::chrono_literals;

namespace {

constexpr const char* VERIFICATION_SOP_CLASS = "1.2.840.10008.1.1";
constexpr const char* CT_IMAGE_STORAGE = "1.2.840.10008.5.1.4.1.1.2";
constexpr const char* EXPLICIT_VR_LE = "1.2.840.10008.1.2.1";
constexpr const char* PEER_IMPLEMENTATION_UID = "1.2.826.0.1.3680043.9.9999.1240.2";
constexpr uint8_t CONTEXT_ID = 1;

/// How long the scripted peer waits for the SCU
constexpr auto peer_timeout = 5s;

/// dicom_server_v2 ports of this file; no other test uses them
constexpr uint16_t V2_ECHO_PORT = 11361;
constexpr uint16_t V2_STORE_PORT = 11362;
constexpr uint16_t V2_CONCURRENT_PORT = 11363;
constexpr uint16_t V2_ABORT_PORT = 11364;

/// Associations open at once in the concurrency test, and C-ECHOs on each
constexpr int CONCURRENT_CLIENTS = 4;
constexpr int ECHOES_PER_CLIENT = 5;

// =============================================================================
// Scripted peer (plain sockets, so it does not depend on network_system)
// =============================================================================

#ifdef _WIN32
using native_socket = SOCKET;
const native_socket no_socket = INVALID_SOCKET;
void close_socket(native_socket s) {
    if (s != no_socket) {
        closesocket(s);
    }
}
#else
using native_socket = int;
constexpr native_socket no_socket = -1;
void close_socket(native_socket s) {
    if (s != no_socket) {
        ::close(s);
    }
}
#endif

/// Wait until @p s is readable
bool wait_readable(native_socket s, std::chrono::milliseconds timeout) {
#ifdef _WIN32
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(s, &fds);
    timeval tv{};
    tv.tv_sec = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
    return select(0, &fds, nullptr, nullptr, &tv) > 0;
#else
    pollfd pfd{};
    pfd.fd = s;
    pfd.events = POLLIN;
    return ::poll(&pfd, 1, static_cast<int>(timeout.count())) > 0;
#endif
}

/**
 * @brief A TCP listener on 127.0.0.1 that plays one DICOM peer.
 *
 * It accepts one connection, reads the PDUs the SCU sends, and sends what a
 * test tells it to, including bytes that break PS3.8.
 */
class scripted_peer {
public:
    scripted_peer() {
#ifdef _WIN32
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
#endif
        listener_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        REQUIRE(listener_ != no_socket);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;  // any free port
        REQUIRE(::bind(listener_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
        REQUIRE(::listen(listener_, 4) == 0);

        socklen_t length = sizeof(addr);
        REQUIRE(::getsockname(listener_, reinterpret_cast<sockaddr*>(&addr), &length) == 0);
        port_ = ntohs(addr.sin_port);
    }

    ~scripted_peer() {
        close_socket(connection_);
        close_socket(listener_);
#ifdef _WIN32
        WSACleanup();
#endif
    }

    scripted_peer(const scripted_peer&) = delete;
    scripted_peer& operator=(const scripted_peer&) = delete;

    [[nodiscard]] uint16_t port() const noexcept { return port_; }

    /// Accept the SCU's connection
    bool accept_connection() {
        if (!wait_readable(listener_, peer_timeout)) {
            return false;
        }
        connection_ = ::accept(listener_, nullptr, nullptr);
#ifdef SO_NOSIGPIPE
        int one = 1;
        setsockopt(connection_, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
        return connection_ != no_socket;
    }

    /// The next PDU from the SCU (header included), or nothing when the SCU
    /// closed the connection or sent nothing in time
    std::optional<std::vector<uint8_t>> read_pdu() {
        std::vector<uint8_t> pdu(6);
        if (!read_exact(std::span<uint8_t>(pdu))) {
            return std::nullopt;
        }
        const uint32_t length = (static_cast<uint32_t>(pdu[2]) << 24) |
                                (static_cast<uint32_t>(pdu[3]) << 16) |
                                (static_cast<uint32_t>(pdu[4]) << 8) |
                                static_cast<uint32_t>(pdu[5]);
        // The SCU under test is trusted not to send huge PDUs here
        pdu.resize(6 + length);
        if (!read_exact(std::span<uint8_t>(pdu).subspan(6))) {
            return std::nullopt;
        }
        return pdu;
    }

    /// Whether the SCU closed the connection (EOF) within the timeout
    bool sees_close() {
        uint8_t byte = 0;
        return !read_exact(std::span<uint8_t>(&byte, 1));
    }

    void send(const std::vector<uint8_t>& bytes) {
        std::size_t sent = 0;
        while (sent < bytes.size()) {
#if defined(_WIN32)
            const int n = ::send(connection_, reinterpret_cast<const char*>(bytes.data() + sent),
                                 static_cast<int>(bytes.size() - sent), 0);
#elif defined(MSG_NOSIGNAL)
            const auto n = ::send(connection_, bytes.data() + sent, bytes.size() - sent,
                                  MSG_NOSIGNAL);
#else
            const auto n = ::send(connection_, bytes.data() + sent, bytes.size() - sent, 0);
#endif
            if (n <= 0) {
                return;
            }
            sent += static_cast<std::size_t>(n);
        }
    }

private:
    bool read_exact(std::span<uint8_t> out) {
        std::size_t received = 0;
        while (received < out.size()) {
            if (!wait_readable(connection_, peer_timeout)) {
                return false;
            }
#ifdef _WIN32
            const int n = ::recv(connection_, reinterpret_cast<char*>(out.data() + received),
                                 static_cast<int>(out.size() - received), 0);
#else
            const auto n = ::recv(connection_, out.data() + received, out.size() - received, 0);
#endif
            if (n <= 0) {
                return false;
            }
            received += static_cast<std::size_t>(n);
        }
        return true;
    }

    native_socket listener_{no_socket};
    native_socket connection_{no_socket};
    uint16_t port_{0};
};

// =============================================================================
// Helpers
// =============================================================================

association_config make_scu_config(const std::string& called_ae,
                                   const std::string& abstract_syntax = VERIFICATION_SOP_CLASS) {
    association_config config;
    config.calling_ae_title = "TCP_SCU";
    config.called_ae_title = called_ae;
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.1240.1";
    config.implementation_version_name = "TCP_SCU";
    config.proposed_contexts.push_back({CONTEXT_ID, abstract_syntax, {EXPLICIT_VR_LE}});
    return config;
}

/// A-ASSOCIATE-AC accepting the SCU's context 1 with Explicit VR LE
std::vector<uint8_t> make_ac(uint32_t max_pdu_length = 16384) {
    associate_ac ac;
    ac.calling_ae_title = "TCP_SCU";
    ac.called_ae_title = "SCRIPTED_SCP";
    ac.application_context = DICOM_APPLICATION_CONTEXT;
    ac.presentation_contexts.push_back(
        {CONTEXT_ID, presentation_context_result::acceptance, EXPLICIT_VR_LE});
    ac.user_info.max_pdu_length = max_pdu_length;
    ac.user_info.implementation_class_uid = PEER_IMPLEMENTATION_UID;
    ac.user_info.implementation_version_name = "SCRIPTED";
    return pdu_encoder::encode_associate_ac(ac);
}

/// The peer accepts the connection, reads A-ASSOCIATE-RQ, and accepts it
bool peer_accepts_association(scripted_peer& peer) {
    if (!peer.accept_connection()) {
        return false;
    }
    auto rq = peer.read_pdu();
    if (!rq || (*rq)[0] != static_cast<uint8_t>(pdu_type::associate_rq)) {
        return false;
    }
    peer.send(make_ac());
    return true;
}

/// Whether @p pdu is an A-ABORT from @p source with @p reason
bool is_abort(const std::optional<std::vector<uint8_t>>& pdu,
              abort_source source, abort_reason reason) {
    return pdu && pdu->size() == 10 &&
           (*pdu)[0] == static_cast<uint8_t>(pdu_type::abort) &&
           (*pdu)[8] == static_cast<uint8_t>(source) &&
           (*pdu)[9] == static_cast<uint8_t>(reason);
}

encoding::transfer_syntax explicit_le() {
    return encoding::transfer_syntax{EXPLICIT_VR_LE};
}

/// Encoded command set of @p msg, empty when it does not encode. The
/// scripted peer calls it on its own thread, where Catch2 3.4 assertions
/// are not safe, so it does not assert; an empty command fails the SCU's
/// checks instead.
std::vector<uint8_t> command_bytes(const dimse::dimse_message& msg) {
    auto encoded = dimse::dimse_message::encode(msg, explicit_le());
    if (encoded.is_err()) {
        return {};
    }
    return encoded.value().first;
}

/// A CT image data set with @p pixel_bytes bytes of pixel data
core::dicom_dataset make_ct_dataset(const std::string& instance_uid, std::size_t pixel_bytes) {
    core::dicom_dataset ds;
    ds.set_string(core::tags::patient_name, encoding::vr_type::PN, "TCP^TEST");
    ds.set_string(core::tags::patient_id, encoding::vr_type::LO, "TCP1240");
    ds.set_string(core::tags::study_instance_uid, encoding::vr_type::UI,
                  "1.2.826.0.1.3680043.9.9999.1240.10");
    ds.set_string(core::tags::series_instance_uid, encoding::vr_type::UI,
                  "1.2.826.0.1.3680043.9.9999.1240.11");
    ds.set_string(core::tags::modality, encoding::vr_type::CS, "CT");
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI, CT_IMAGE_STORAGE);
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, instance_uid);
    std::vector<uint8_t> pixels(pixel_bytes);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<uint8_t>(i % 251);
    }
    core::dicom_element pixel_element(core::tags::pixel_data, encoding::vr_type::OB);
    pixel_element.set_value(std::span<const uint8_t>(pixels));
    ds.insert(std::move(pixel_element));
    return ds;
}

/// The error message of @p result, empty on success
template <typename T>
std::string error_text(const kcenon::pacs::Result<T>& result) {
    return result.is_err() ? result.error().message : std::string{};
}

/// Wait until @p condition holds or @p timeout passes
template <typename Condition>
bool eventually(Condition condition, std::chrono::milliseconds timeout = 5s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!condition()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(10ms);
    }
    return true;
}

}  // namespace

// =============================================================================
// A-ASSOCIATE over TCP
// =============================================================================

TEST_CASE("association over TCP: connect fails when nothing listens",
          "[association][tcp]") {
    uint16_t port = 0;
    {
        scripted_peer peer;
        port = peer.port();
    }  // the listener is closed again

    auto result = association::connect("127.0.0.1", port, make_scu_config("NOBODY"), 5s);
    REQUIRE(result.is_err());
    CHECK((result.error().code == error_codes::connection_failed ||
           result.error().code == error_codes::connection_timeout));
}

TEST_CASE("association over TCP: A-ASSOCIATE-AC establishes the association",
          "[association][tcp]") {
    scripted_peer peer;
    auto scp = std::async(std::launch::async, [&peer]() {
        const bool accepted = peer_accepts_association(peer);
        // The SCU releases; answer it
        auto release_rq = peer.read_pdu();
        const bool got_release_rq =
            release_rq && (*release_rq)[0] == static_cast<uint8_t>(pdu_type::release_rq);
        peer.send(pdu_encoder::encode_release_rp());
        return accepted && got_release_rq && peer.sees_close();
    });

    auto result = association::connect("localhost", peer.port(),
                                       make_scu_config("SCRIPTED_SCP"), 5s);
    REQUIRE(result.is_ok());
    auto& assoc = result.value();
    CHECK(assoc.is_established());
    CHECK(assoc.has_accepted_context(VERIFICATION_SOP_CLASS));
    CHECK(assoc.remote_implementation_class() == PEER_IMPLEMENTATION_UID);
    CHECK(assoc.remote_implementation_version() == "SCRIPTED");
    CHECK(assoc.max_pdu_size() == 16384);

    REQUIRE(assoc.release(5s).is_ok());
    CHECK(assoc.state() == association_state::released);
    CHECK(scp.get());
}

TEST_CASE("association over TCP: A-ASSOCIATE-RJ is reported", "[association][tcp]") {
    scripted_peer peer;
    auto scp = std::async(std::launch::async, [&peer]() {
        if (!peer.accept_connection() || !peer.read_pdu()) {
            return false;
        }
        peer.send(pdu_encoder::encode_associate_rj(associate_rj{
            reject_result::rejected_permanent,
            static_cast<uint8_t>(reject_source::service_user),
            static_cast<uint8_t>(reject_reason_user::called_ae_not_recognized)}));
        return peer.sees_close();
    });

    auto result = association::connect("localhost", peer.port(),
                                       make_scu_config("WRONG_AE"), 5s);
    REQUIRE(result.is_err());
    CHECK(result.error().code == error_codes::association_rejected);
    CHECK_THAT(result.error().message,
               Catch::Matchers::ContainsSubstring("called AE title not recognized"));
    CHECK(scp.get());
}

TEST_CASE("association over TCP: no A-ASSOCIATE response aborts at the timeout",
          "[association][tcp]") {
    scripted_peer peer;
    auto scp = std::async(std::launch::async, [&peer]() {
        if (!peer.accept_connection() || !peer.read_pdu()) {
            return false;
        }
        // Stay silent: the SCU's ARTIM expires and it aborts
        return is_abort(peer.read_pdu(), abort_source::service_provider,
                        abort_reason::not_specified);
    });

    const auto started = std::chrono::steady_clock::now();
    auto result = association::connect("localhost", peer.port(),
                                       make_scu_config("SILENT_SCP"), 300ms);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    REQUIRE(result.is_err());
    CHECK(result.error().code == error_codes::connection_timeout);
    CHECK(elapsed < 4s);
    CHECK(scp.get());
}

TEST_CASE("association over TCP: an oversized PDU length is refused with A-ABORT",
          "[association][tcp]") {
    scripted_peer peer;
    auto scp = std::async(std::launch::async, [&peer]() {
        if (!peer.accept_connection() || !peer.read_pdu()) {
            return false;
        }
        // A-ASSOCIATE-AC header claiming 4 GiB; the SCU must not wait for
        // (or allocate) the body
        peer.send({0x02, 0x00, 0xFF, 0xFF, 0xFF, 0xFF});
        return is_abort(peer.read_pdu(), abort_source::service_provider,
                        abort_reason::invalid_pdu_parameter);
    });

    auto result = association::connect("localhost", peer.port(),
                                       make_scu_config("HUGE_SCP"), 5s);
    REQUIRE(result.is_err());
    CHECK(result.error().code == error_codes::pdu_decoding_error);
    CHECK(scp.get());
}

TEST_CASE("association over TCP: an unknown PDU type is refused with A-ABORT",
          "[association][tcp]") {
    scripted_peer peer;
    auto scp = std::async(std::launch::async, [&peer]() {
        if (!peer.accept_connection() || !peer.read_pdu()) {
            return false;
        }
        peer.send({0x09, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00});
        return is_abort(peer.read_pdu(), abort_source::service_provider,
                        abort_reason::unrecognized_pdu);
    });

    auto result = association::connect("localhost", peer.port(),
                                       make_scu_config("ODD_SCP"), 5s);
    REQUIRE(result.is_err());
    CHECK(scp.get());
}

// =============================================================================
// P-DATA-TF over TCP
// =============================================================================

TEST_CASE("association over TCP: a response in several P-DATA-TF PDUs is reassembled",
          "[association][tcp]") {
    scripted_peer peer;
    auto scp = std::async(std::launch::async, [&peer]() -> std::optional<dimse::command_field> {
        if (!peer_accepts_association(peer)) {
            return std::nullopt;
        }

        // The C-ECHO-RQ from the SCU
        auto request = peer.read_pdu();
        if (!request) {
            return std::nullopt;
        }
        auto data = pdu_decoder::decode_p_data_tf(*request);
        if (data.is_err() || data.value().pdvs.empty()) {
            return std::nullopt;
        }
        auto command = dimse::dimse_message::decode(
            data.value().pdvs[0].data, {}, explicit_le());
        if (command.is_err()) {
            return std::nullopt;
        }

        // Answer with the command set cut into two fragments in two PDUs
        auto rsp = command_bytes(dimse::make_c_echo_rsp(command.value().message_id()));
        const auto half = static_cast<std::ptrdiff_t>(rsp.size() / 2);
        peer.send(pdu_encoder::encode_p_data_tf(presentation_data_value(
            CONTEXT_ID, true, false, std::vector<uint8_t>(rsp.begin(), rsp.begin() + half))));
        peer.send(pdu_encoder::encode_p_data_tf(presentation_data_value(
            CONTEXT_ID, true, true, std::vector<uint8_t>(rsp.begin() + half, rsp.end()))));
        return command.value().command();
    });

    auto result = association::connect("localhost", peer.port(),
                                       make_scu_config("SCRIPTED_SCP"), 5s);
    REQUIRE(result.is_ok());
    auto& assoc = result.value();

    REQUIRE(assoc.send_dimse(CONTEXT_ID, dimse::make_c_echo_rq(42)).is_ok());
    auto received = assoc.receive_dimse(5s);
    REQUIRE(received.is_ok());
    CHECK(received.value().first == CONTEXT_ID);
    CHECK(received.value().second.command() == dimse::command_field::c_echo_rsp);
    CHECK(received.value().second.status() == dimse::status_success);

    auto seen = scp.get();
    REQUIRE(seen.has_value());
    CHECK(*seen == dimse::command_field::c_echo_rq);
}

TEST_CASE("association over TCP: malformed P-DATA-TF aborts the association",
          "[association][tcp]") {
    SECTION("a PDV on a presentation context that was not accepted") {
        scripted_peer peer;
        auto scp = std::async(std::launch::async, [&peer]() {
            if (!peer_accepts_association(peer)) {
                return false;
            }
            peer.send(pdu_encoder::encode_p_data_tf(presentation_data_value(
                3, true, true, command_bytes(dimse::make_c_echo_rsp(1)))));
            return is_abort(peer.read_pdu(), abort_source::service_provider,
                            abort_reason::unexpected_pdu_parameter);
        });

        auto result = association::connect("localhost", peer.port(),
                                           make_scu_config("SCRIPTED_SCP"), 5s);
        REQUIRE(result.is_ok());
        auto& assoc = result.value();
        CHECK(assoc.receive_dimse(5s).is_err());
        CHECK(assoc.state() == association_state::aborted);
        CHECK(scp.get());
    }

    SECTION("a P-DATA-TF longer than the maximum PDU length the SCU announced") {
        scripted_peer peer;
        auto scp = std::async(std::launch::async, [&peer]() {
            if (!peer_accepts_association(peer)) {
                return false;
            }
            // 1 MiB claimed; the SCU announced 16384 (the default)
            peer.send({0x04, 0x00, 0x00, 0x10, 0x00, 0x00});
            return is_abort(peer.read_pdu(), abort_source::service_provider,
                            abort_reason::invalid_pdu_parameter);
        });

        auto result = association::connect("localhost", peer.port(),
                                           make_scu_config("SCRIPTED_SCP"), 5s);
        REQUIRE(result.is_ok());
        auto& assoc = result.value();
        CHECK(assoc.receive_dimse(5s).is_err());
        CHECK(assoc.state() == association_state::aborted);
        CHECK(scp.get());
    }
}

// =============================================================================
// Release and Abort from the Peer over TCP
// =============================================================================

TEST_CASE("association over TCP: the peer's A-RELEASE-RQ is answered",
          "[association][tcp]") {
    scripted_peer peer;
    auto scp = std::async(std::launch::async, [&peer]() {
        if (!peer_accepts_association(peer)) {
            return false;
        }
        peer.send(pdu_encoder::encode_release_rq());
        auto reply = peer.read_pdu();
        return reply && (*reply)[0] == static_cast<uint8_t>(pdu_type::release_rp);
    });

    auto result = association::connect("localhost", peer.port(),
                                       make_scu_config("SCRIPTED_SCP"), 5s);
    REQUIRE(result.is_ok());
    auto& assoc = result.value();
    CHECK(assoc.receive_dimse(5s).is_err());
    CHECK(assoc.state() == association_state::released);
    CHECK(scp.get());
}

TEST_CASE("association over TCP: the peer's A-ABORT ends the association",
          "[association][tcp]") {
    scripted_peer peer;
    auto scp = std::async(std::launch::async, [&peer]() {
        if (!peer_accepts_association(peer)) {
            return false;
        }
        peer.send(pdu_encoder::encode_abort(abort_source::service_provider,
                                            abort_reason::not_specified));
        return peer.sees_close();
    });

    auto result = association::connect("localhost", peer.port(),
                                       make_scu_config("SCRIPTED_SCP"), 5s);
    REQUIRE(result.is_ok());
    auto& assoc = result.value();
    auto received = assoc.receive_dimse(5s);
    REQUIRE(received.is_err());
    CHECK(received.error().code == error_codes::association_aborted);
    CHECK(assoc.state() == association_state::aborted);
    CHECK(scp.get());
}

TEST_CASE("association over TCP: abort() sends A-ABORT", "[association][tcp]") {
    scripted_peer peer;
    auto scp = std::async(std::launch::async, [&peer]() {
        if (!peer_accepts_association(peer)) {
            return false;
        }
        return is_abort(peer.read_pdu(), abort_source::service_user,
                        abort_reason::not_specified);
    });

    auto result = association::connect("localhost", peer.port(),
                                       make_scu_config("SCRIPTED_SCP"), 5s);
    REQUIRE(result.is_ok());
    result.value().abort();
    CHECK(result.value().state() == association_state::aborted);
    CHECK(scp.get());
}

// =============================================================================
// dicom_server_v2 over TCP
// =============================================================================

#ifdef PACS_WITH_NETWORK_SYSTEM

namespace {

server_config make_v2_config(const std::string& ae_title, uint16_t port) {
    server_config config;
    config.ae_title = ae_title;
    config.port = port;
    config.max_associations = 20;
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.1240.3";
    config.implementation_version_name = "V2_TCP_TEST";
    return config;
}

}  // namespace

TEST_CASE("association over TCP: C-ECHO and release with dicom_server_v2",
          "[association][tcp][server_v2]") {
    v2::dicom_server_v2 server(make_v2_config("V2_TCP_ECHO", V2_ECHO_PORT));
    server.register_service(std::make_shared<services::verification_scp>());

    std::atomic<int> established{0};
    std::atomic<int> closed{0};
    server.on_association_established(
        [&](const std::string&, const std::string&, const std::string&) { ++established; });
    server.on_association_closed([&](const std::string&, bool) { ++closed; });
    REQUIRE(server.start().is_ok());

    {
        auto result = association::connect("localhost", V2_ECHO_PORT,
                                           make_scu_config("V2_TCP_ECHO"), 5s);
        REQUIRE(result.is_ok());
        auto& assoc = result.value();
        CHECK(assoc.remote_implementation_class() == "1.2.826.0.1.3680043.9.9999.1240.3");

        for (uint16_t id = 1; id <= 3; ++id) {
            REQUIRE(assoc.send_dimse(CONTEXT_ID, dimse::make_c_echo_rq(id)).is_ok());
            auto received = assoc.receive_dimse(5s);
            REQUIRE(received.is_ok());
            CHECK(received.value().second.command() == dimse::command_field::c_echo_rsp);
            CHECK(received.value().second.status() == dimse::status_success);
        }
        CHECK(eventually([&] { return established.load() == 1; }));

        auto released = assoc.release(5s);
        INFO(error_text(released));
        REQUIRE(released.is_ok());
        CHECK(eventually([&] { return closed.load() == 1; }));
    }

    server.stop();
    CHECK(server.get_statistics().total_associations == 1);
}

TEST_CASE("association over TCP: C-STORE is fragmented to the maximum PDU length",
          "[association][tcp][server_v2]") {
    v2::dicom_server_v2 server(make_v2_config("V2_TCP_STORE", V2_STORE_PORT));

    std::atomic<std::size_t> stored_pixel_bytes{0};
    std::atomic<int> stored{0};
    auto storage = std::make_shared<services::storage_scp>();
    storage->set_handler([&](const core::dicom_dataset& dataset, const std::string&,
                             const std::string&, const std::string&) {
        if (const auto* pixels = dataset.get(core::tags::pixel_data)) {
            stored_pixel_bytes = pixels->length();
        }
        ++stored;
        return services::storage_status::success;
    });
    server.register_service(storage);
    REQUIRE(server.start().is_ok());

    {
        auto config = make_scu_config("V2_TCP_STORE", CT_IMAGE_STORAGE);
        // The SCU announces 4096, so the server sends at most that and the
        // SCU fragments its 64 KiB of pixel data to the same limit
        config.max_pdu_length = 4096;
        auto result = association::connect("localhost", V2_STORE_PORT, config, 5s);
        REQUIRE(result.is_ok());
        auto& assoc = result.value();
        CHECK(assoc.max_pdu_size() == 4096);

        services::storage_scu scu;
        auto stored_result = scu.store(
            assoc, make_ct_dataset("1.2.826.0.1.3680043.9.9999.1240.20", 64 * 1024));
        REQUIRE(stored_result.is_ok());
        CHECK(stored_result.value().is_success());
        CHECK(stored.load() == 1);
        CHECK(stored_pixel_bytes.load() == 64 * 1024);

        auto released = assoc.release(5s);
        INFO(error_text(released));
        REQUIRE(released.is_ok());
    }

    server.stop();
}

TEST_CASE("association over TCP: concurrent associations each reach their own handler",
          "[association][tcp][server_v2]") {
    v2::dicom_server_v2 server(make_v2_config("V2_TCP_MULTI", V2_CONCURRENT_PORT));
    server.register_service(std::make_shared<services::verification_scp>());
    REQUIRE(server.start().is_ok());

    // All associations are open at once, so a response routed to the wrong
    // session would leave one SCU waiting
    std::atomic<int> connected{0};
    std::vector<std::future<int>> results;
    for (int i = 0; i < CONCURRENT_CLIENTS; ++i) {
        results.push_back(std::async(std::launch::async, [&connected]() {
            auto result = association::connect("localhost", V2_CONCURRENT_PORT,
                                               make_scu_config("V2_TCP_MULTI"), 5s);
            if (result.is_err()) {
                return 0;
            }
            auto& assoc = result.value();
            ++connected;
            eventually([&connected] { return connected.load() == CONCURRENT_CLIENTS; });

            int succeeded = 0;
            for (uint16_t id = 1; id <= ECHOES_PER_CLIENT; ++id) {
                if (assoc.send_dimse(CONTEXT_ID, dimse::make_c_echo_rq(id)).is_err()) {
                    break;
                }
                auto received = assoc.receive_dimse(5s);
                if (received.is_ok() &&
                    received.value().second.status() == dimse::status_success) {
                    ++succeeded;
                }
            }
            (void)assoc.release(5s);
            return succeeded;
        }));
    }

    int total = 0;
    for (auto& result : results) {
        total += result.get();
    }
    CHECK(total == CONCURRENT_CLIENTS * ECHOES_PER_CLIENT);

    server.stop();
    CHECK(server.get_statistics().total_associations == CONCURRENT_CLIENTS);
}

TEST_CASE("association over TCP: dicom_server_v2 sees an SCU abort",
          "[association][tcp][server_v2]") {
    v2::dicom_server_v2 server(make_v2_config("V2_TCP_ABORT", V2_ABORT_PORT));
    server.register_service(std::make_shared<services::verification_scp>());

    std::atomic<int> closed{0};
    server.on_association_closed([&](const std::string&, bool) { ++closed; });
    REQUIRE(server.start().is_ok());

    auto result = association::connect("localhost", V2_ABORT_PORT,
                                       make_scu_config("V2_TCP_ABORT"), 5s);
    REQUIRE(result.is_ok());
    result.value().abort();

    CHECK(eventually([&] { return closed.load() == 1; }));
    CHECK(eventually([&] { return server.active_associations() == 0; }));
    server.stop();
}

#endif  // PACS_WITH_NETWORK_SYSTEM
