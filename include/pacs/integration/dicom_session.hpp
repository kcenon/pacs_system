/**
 * @file dicom_session.hpp
 * @brief DICOM session wrapper for network_system sessions
 *
 * This file provides the dicom_session class for wrapping network_system
 * sessions to handle DICOM PDU-level communication, including PDU framing,
 * encoding, and decoding.
 *
 * @see IR-2 (network_system Integration)
 * @see DES-INT-002 - Network Adapter Design Specification
 * @see DICOM PS3.8 - Network Communication Support for Message Exchange
 */

#pragma once

#include "pacs/network/pdu_types.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <variant>
#include <vector>

#ifdef PACS_WITH_COMMON_SYSTEM
#include <kcenon/common/patterns/result.h>
#endif

// Forward declarations for network_system types (no ASIO dependency)
#include <network_system/forward.h>

namespace pacs::integration {

// =============================================================================
// Result Type and Error Info
// =============================================================================

#ifdef PACS_WITH_COMMON_SYSTEM
template <typename T>
using Result = kcenon::common::Result<T>;

using error_info = kcenon::common::error_info;
#else
/**
 * @brief Simple error info for fallback when common_system is unavailable
 */
struct error_info {
    int code = -1;
    std::string message;
    std::string module;

    error_info() = default;
    error_info(const std::string& msg) : message(msg) {}
    error_info(int c, const std::string& msg, const std::string& mod = "")
        : code(c), message(msg), module(mod) {}
};

/**
 * @brief Simple result type for error handling when common_system is unavailable
 */
template <typename T>
class Result {
public:
    Result(T value) : data_(std::move(value)), has_value_(true) {}
    Result(const error_info& err) : error_(err), has_value_(false) {}

    [[nodiscard]] bool is_ok() const noexcept { return has_value_; }
    [[nodiscard]] bool is_err() const noexcept { return !has_value_; }
    [[nodiscard]] T& value() & { return data_; }
    [[nodiscard]] const T& value() const& { return data_; }
    [[nodiscard]] T&& value() && { return std::move(data_); }
    [[nodiscard]] const error_info& error() const { return error_; }

private:
    T data_{};
    error_info error_;
    bool has_value_;
};
#endif

// =============================================================================
// PDU Receive Result
// =============================================================================

/**
 * @struct pdu_data
 * @brief Container for received PDU data
 */
struct pdu_data {
    network::pdu_type type;        ///< PDU type from header
    std::vector<uint8_t> payload;  ///< PDU payload (excluding 6-byte header)

    pdu_data() : type(network::pdu_type::abort) {}
    pdu_data(network::pdu_type t, std::vector<uint8_t> p)
        : type(t), payload(std::move(p)) {}
};

// =============================================================================
// DICOM Session Class
// =============================================================================

/**
 * @class dicom_session
 * @brief DICOM session wrapper for network_system sessions
 *
 * This class wraps a network_system messaging_session (or secure_session)
 * to provide DICOM PDU-level communication. Key features include:
 *
 * - **PDU Framing**: Handles DICOM PDU header parsing (6-byte header)
 * - **PDU Send/Receive**: Send and receive complete PDUs
 * - **Timeout Support**: Configurable timeouts for receive operations
 * - **Connection Management**: Track connection state and remote address
 *
 * ### PDU Header Format
 * The DICOM PDU header is 6 bytes:
 * - Byte 0: PDU Type (1 byte)
 * - Byte 1: Reserved (0x00)
 * - Bytes 2-5: PDU Length (Big Endian, 4 bytes)
 *
 * Thread Safety: All public methods are thread-safe. The underlying
 * network operations are serialized through network_system's I/O context.
 *
 * @example
 * @code
 * // Sending an A-ASSOCIATE-RQ
 * std::vector<uint8_t> associate_rq_data = encode_associate_rq(rq);
 * auto send_result = session->send_pdu(
 *     network::pdu_type::associate_rq, associate_rq_data);
 *
 * if (send_result.is_err()) {
 *     std::cerr << "Send failed: " << send_result.error() << '\n';
 *     return;
 * }
 *
 * // Receiving the response
 * auto recv_result = session->receive_pdu(std::chrono::seconds{30});
 * if (recv_result.is_ok()) {
 *     auto& pdu = recv_result.value();
 *     if (pdu.type == network::pdu_type::associate_ac) {
 *         // Process A-ASSOCIATE-AC
 *     }
 * }
 * @endcode
 */
class dicom_session : public std::enable_shared_from_this<dicom_session> {
public:
    // ─────────────────────────────────────────────────────
    // Type Aliases
    // ─────────────────────────────────────────────────────

    using clock = std::chrono::steady_clock;
    using duration = std::chrono::milliseconds;
    using time_point = clock::time_point;

    /// Default receive timeout
    static constexpr duration default_timeout{30000};  // 30 seconds

    /// PDU header size per DICOM PS3.8
    static constexpr size_t pdu_header_size = 6;

    /// Maximum PDU payload size (sanity check)
    static constexpr size_t max_pdu_payload_size = 64 * 1024 * 1024;  // 64MB

    // ─────────────────────────────────────────────────────
    // Construction / Destruction
    // ─────────────────────────────────────────────────────

    /**
     * @brief Construct a DICOM session wrapping a messaging_session
     * @param session The network_system session to wrap
     */
    explicit dicom_session(
        std::shared_ptr<network_system::session::messaging_session> session);

    /**
     * @brief Construct a DICOM session wrapping a secure_session
     * @param session The secure network_system session to wrap
     */
    explicit dicom_session(
        std::shared_ptr<network_system::session::secure_session> session);

    /**
     * @brief Destructor (closes session if open)
     */
    ~dicom_session();

    // Non-copyable
    dicom_session(const dicom_session&) = delete;
    dicom_session& operator=(const dicom_session&) = delete;

    // Movable
    dicom_session(dicom_session&& other) noexcept;
    dicom_session& operator=(dicom_session&& other) noexcept;

    // ─────────────────────────────────────────────────────
    // PDU Operations
    // ─────────────────────────────────────────────────────

    /**
     * @brief Send a complete DICOM PDU
     *
     * Constructs a properly framed PDU with header and sends it
     * over the network connection.
     *
     * @param type The PDU type
     * @param payload The PDU payload (without header)
     * @return Result indicating success or error
     *
     * @note The payload should not include the 6-byte PDU header;
     *       the header is constructed automatically.
     */
    [[nodiscard]] Result<std::monostate> send_pdu(
        network::pdu_type type,
        const std::vector<uint8_t>& payload);

    /**
     * @brief Send raw PDU data (with header already included)
     *
     * Sends pre-encoded PDU data directly without modification.
     * Use this when the PDU has already been fully encoded.
     *
     * @param data Complete PDU data including header
     * @return Result indicating success or error
     */
    [[nodiscard]] Result<std::monostate> send_raw(
        const std::vector<uint8_t>& data);

    /**
     * @brief Receive a complete DICOM PDU
     *
     * Waits for and receives a complete PDU, handling the PDU framing
     * protocol (reading header, then payload based on length).
     *
     * @param timeout Maximum time to wait for PDU
     * @return Result containing received PDU data or error
     *
     * @note This method blocks until a complete PDU is received or timeout
     */
    [[nodiscard]] Result<pdu_data> receive_pdu(
        duration timeout = default_timeout);

    /**
     * @brief Set callback for asynchronous PDU reception
     *
     * When set, incoming PDUs are delivered through this callback
     * instead of requiring explicit receive_pdu() calls.
     *
     * @param callback Function called when PDU is received
     */
    void set_pdu_callback(std::function<void(const pdu_data&)> callback);

    /**
     * @brief Clear the PDU callback
     */
    void clear_pdu_callback();

    // ─────────────────────────────────────────────────────
    // Connection State
    // ─────────────────────────────────────────────────────

    /**
     * @brief Close the session
     *
     * Closes the underlying network connection. Any pending
     * send or receive operations will be cancelled.
     */
    void close();

    /**
     * @brief Check if the session is open
     * @return true if the connection is active
     */
    [[nodiscard]] bool is_open() const noexcept;

    /**
     * @brief Get the remote peer address
     * @return Remote address as "host:port" string
     */
    [[nodiscard]] std::string remote_address() const;

    /**
     * @brief Get session identifier
     * @return Unique session identifier string
     */
    [[nodiscard]] std::string session_id() const;

    /**
     * @brief Check if this is a secure (TLS) session
     * @return true if using TLS
     */
    [[nodiscard]] bool is_secure() const noexcept;

    // ─────────────────────────────────────────────────────
    // Error Handling
    // ─────────────────────────────────────────────────────

    /**
     * @brief Set callback for error events
     * @param callback Function called on errors
     */
    void set_error_callback(std::function<void(std::error_code)> callback);

    /**
     * @brief Get the last error code
     * @return Last error or success code
     */
    [[nodiscard]] std::error_code last_error() const noexcept;

private:
    // ─────────────────────────────────────────────────────
    // Private Types
    // ─────────────────────────────────────────────────────

    /// Variant for holding either session type
    using session_variant = std::variant<
        std::shared_ptr<network_system::session::messaging_session>,
        std::shared_ptr<network_system::session::secure_session>>;

    // ─────────────────────────────────────────────────────
    // Private Methods
    // ─────────────────────────────────────────────────────

    /// Handle incoming data from network
    void on_data_received(const std::vector<uint8_t>& data);

    /// Handle connection errors
    void on_error(std::error_code ec);

    /// Process buffered data for complete PDUs
    void process_buffer();

    /// Encode PDU header
    static std::vector<uint8_t> encode_pdu_header(
        network::pdu_type type, uint32_t length);

    /// Parse PDU header from buffer
    static bool parse_pdu_header(
        const std::vector<uint8_t>& buffer,
        network::pdu_type& type,
        uint32_t& length);

    /// Send data through the underlying session
    void send_data(std::vector<uint8_t>&& data);

    // ─────────────────────────────────────────────────────
    // Member Variables
    // ─────────────────────────────────────────────────────

    /// Underlying network session
    session_variant session_;

    /// Receive buffer for PDU framing
    std::vector<uint8_t> receive_buffer_;

    /// Queue of received complete PDUs
    std::vector<pdu_data> received_pdus_;

    /// PDU callback for async reception
    std::function<void(const pdu_data&)> pdu_callback_;

    /// Error callback
    std::function<void(std::error_code)> error_callback_;

    /// Last error code
    std::error_code last_error_;

    /// Session closed flag
    bool closed_ = false;

    /// Mutex for thread safety
    mutable std::mutex mutex_;

    /// Condition variable for synchronous receive
    std::condition_variable receive_cv_;
};

}  // namespace pacs::integration
