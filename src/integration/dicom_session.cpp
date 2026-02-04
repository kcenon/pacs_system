/**
 * @file dicom_session.cpp
 * @brief Implementation of dicom_session for DICOM PDU communication
 */

#include <pacs/integration/dicom_session.hpp>

#include <kcenon/network/session/session.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace pacs::integration {

// =============================================================================
// Construction / Destruction
// =============================================================================

dicom_session::dicom_session(
    std::shared_ptr<network_system::session::messaging_session> session)
    : session_(std::move(session)) {
    if (auto* s = std::get_if<
            std::shared_ptr<network_system::session::messaging_session>>(&session_)) {
        // Set up callbacks for the underlying session
        (*s)->set_receive_callback(
            [this](const std::vector<uint8_t>& data) {
                on_data_received(data);
            });

        (*s)->set_error_callback(
            [this](std::error_code ec) {
                on_error(ec);
            });
    }
}

dicom_session::dicom_session(
    std::shared_ptr<network_system::session::secure_session> session)
    : session_(std::move(session)) {
    if (auto* s = std::get_if<
            std::shared_ptr<network_system::session::secure_session>>(&session_)) {
        // Set up callbacks for the underlying secure session
        (*s)->set_receive_callback(
            [this](const std::vector<uint8_t>& data) {
                on_data_received(data);
            });

        (*s)->set_error_callback(
            [this](std::error_code ec) {
                on_error(ec);
            });
    }
}

dicom_session::~dicom_session() {
    close();
}

dicom_session::dicom_session(dicom_session&& other) noexcept
    : session_(std::move(other.session_))
    , receive_buffer_(std::move(other.receive_buffer_))
    , received_pdus_(std::move(other.received_pdus_))
    , pdu_callback_(std::move(other.pdu_callback_))
    , error_callback_(std::move(other.error_callback_))
    , last_error_(other.last_error_)
    , closed_(other.closed_) {
    other.closed_ = true;
}

dicom_session& dicom_session::operator=(dicom_session&& other) noexcept {
    if (this != &other) {
        close();

        std::lock_guard<std::mutex> lock(mutex_);
        session_ = std::move(other.session_);
        receive_buffer_ = std::move(other.receive_buffer_);
        received_pdus_ = std::move(other.received_pdus_);
        pdu_callback_ = std::move(other.pdu_callback_);
        error_callback_ = std::move(other.error_callback_);
        last_error_ = other.last_error_;
        closed_ = other.closed_;

        other.closed_ = true;
    }
    return *this;
}

// =============================================================================
// PDU Operations
// =============================================================================

Result<std::monostate>
dicom_session::send_pdu(network::pdu_type type,
                        const std::vector<uint8_t>& payload) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (closed_) {
        return Result<std::monostate>(error_info("Session is closed"));
    }

    if (payload.size() > max_pdu_payload_size) {
        return Result<std::monostate>(error_info("PDU payload exceeds maximum size"));
    }

    // Encode PDU header
    auto header = encode_pdu_header(type, static_cast<uint32_t>(payload.size()));

    // Combine header and payload
    std::vector<uint8_t> pdu_data;
    pdu_data.reserve(header.size() + payload.size());
    pdu_data.insert(pdu_data.end(), header.begin(), header.end());
    pdu_data.insert(pdu_data.end(), payload.begin(), payload.end());

    // Send data
    send_data(std::move(pdu_data));

    return Result<std::monostate>(std::monostate{});
}

Result<std::monostate>
dicom_session::send_raw(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (closed_) {
        return Result<std::monostate>(error_info("Session is closed"));
    }

    if (data.size() < pdu_header_size) {
        return Result<std::monostate>(error_info("Raw PDU data too small"));
    }

    // Send data as-is
    std::vector<uint8_t> copy = data;
    send_data(std::move(copy));

    return Result<std::monostate>(std::monostate{});
}

Result<pdu_data>
dicom_session::receive_pdu(duration timeout) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (closed_) {
        return Result<pdu_data>(error_info("Session is closed"));
    }

    // Wait for a complete PDU with timeout
    auto deadline = clock::now() + timeout;

    while (received_pdus_.empty()) {
        if (closed_) {
            return Result<pdu_data>(error_info("Session closed while waiting"));
        }

        auto status = receive_cv_.wait_until(lock, deadline);
        if (status == std::cv_status::timeout) {
            return Result<pdu_data>(error_info("Receive timeout"));
        }

        if (last_error_) {
            return Result<pdu_data>(error_info("Receive error: " + last_error_.message()));
        }
    }

    // Get the first PDU from the queue
    pdu_data pdu = std::move(received_pdus_.front());
    received_pdus_.erase(received_pdus_.begin());

    return Result<pdu_data>(std::move(pdu));
}

void dicom_session::set_pdu_callback(
    std::function<void(const pdu_data&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    pdu_callback_ = std::move(callback);
}

void dicom_session::clear_pdu_callback() {
    std::lock_guard<std::mutex> lock(mutex_);
    pdu_callback_ = nullptr;
}

// =============================================================================
// Connection State
// =============================================================================

void dicom_session::close() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (closed_) {
        return;
    }

    closed_ = true;

    // Stop the underlying session
    std::visit([](auto&& session) {
        if (session) {
            session->stop_session();
        }
    }, session_);

    // Notify waiting receivers
    receive_cv_.notify_all();
}

bool dicom_session::is_open() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (closed_) {
        return false;
    }

    return std::visit([](auto&& session) -> bool {
        if (session) {
            return !session->is_stopped();
        }
        return false;
    }, session_);
}

std::string dicom_session::remote_address() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // For now, return a placeholder
    // Real implementation would query the underlying socket
    return "unknown";
}

std::string dicom_session::session_id() const {
    std::lock_guard<std::mutex> lock(mutex_);

    return std::visit([](auto&& session) -> std::string {
        if (session) {
            return session->server_id();
        }
        return "invalid";
    }, session_);
}

bool dicom_session::is_secure() const noexcept {
    return std::holds_alternative<
        std::shared_ptr<network_system::session::secure_session>>(session_);
}

// =============================================================================
// Error Handling
// =============================================================================

void dicom_session::set_error_callback(
    std::function<void(std::error_code)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_callback_ = std::move(callback);
}

std::error_code dicom_session::last_error() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

// =============================================================================
// Private Methods
// =============================================================================

void dicom_session::on_data_received(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (closed_) {
        return;
    }

    // Append to receive buffer
    receive_buffer_.insert(receive_buffer_.end(), data.begin(), data.end());

    // Process complete PDUs
    process_buffer();
}

void dicom_session::on_error(std::error_code ec) {
    std::function<void(std::error_code)> callback;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_error_ = ec;
        callback = error_callback_;
    }

    if (callback) {
        callback(ec);
    }

    // Notify waiting receivers
    receive_cv_.notify_all();
}

void dicom_session::process_buffer() {
    // Process all complete PDUs in the buffer
    while (receive_buffer_.size() >= pdu_header_size) {
        // Parse header
        network::pdu_type type;
        uint32_t length;

        if (!parse_pdu_header(receive_buffer_, type, length)) {
            // Invalid header - this is a protocol error
            last_error_ = std::make_error_code(std::errc::protocol_error);
            receive_cv_.notify_all();
            return;
        }

        // Sanity check on length
        if (length > max_pdu_payload_size) {
            last_error_ = std::make_error_code(std::errc::message_size);
            receive_cv_.notify_all();
            return;
        }

        // Check if we have the complete PDU
        size_t total_size = pdu_header_size + length;
        if (receive_buffer_.size() < total_size) {
            // Need more data
            return;
        }

        // Extract payload (skip 6-byte header)
        std::vector<uint8_t> payload(
            receive_buffer_.begin() + pdu_header_size,
            receive_buffer_.begin() + total_size);

        // Remove processed data from buffer
        receive_buffer_.erase(
            receive_buffer_.begin(),
            receive_buffer_.begin() + total_size);

        // Create PDU data
        pdu_data pdu(type, std::move(payload));

        // Deliver through callback or queue
        if (pdu_callback_) {
            pdu_callback_(pdu);
        } else {
            received_pdus_.push_back(std::move(pdu));
            receive_cv_.notify_one();
        }
    }
}

std::vector<uint8_t>
dicom_session::encode_pdu_header(network::pdu_type type, uint32_t length) {
    std::vector<uint8_t> header(pdu_header_size);

    // Byte 0: PDU Type
    header[0] = static_cast<uint8_t>(type);

    // Byte 1: Reserved
    header[1] = 0x00;

    // Bytes 2-5: Length (Big Endian)
    header[2] = static_cast<uint8_t>((length >> 24) & 0xFF);
    header[3] = static_cast<uint8_t>((length >> 16) & 0xFF);
    header[4] = static_cast<uint8_t>((length >> 8) & 0xFF);
    header[5] = static_cast<uint8_t>(length & 0xFF);

    return header;
}

bool dicom_session::parse_pdu_header(const std::vector<uint8_t>& buffer,
                                     network::pdu_type& type,
                                     uint32_t& length) {
    if (buffer.size() < pdu_header_size) {
        return false;
    }

    // Byte 0: PDU Type
    uint8_t type_byte = buffer[0];

    // Validate PDU type
    if (type_byte < 0x01 || type_byte > 0x07) {
        return false;
    }

    type = static_cast<network::pdu_type>(type_byte);

    // Byte 1: Reserved (should be 0, but we don't validate)

    // Bytes 2-5: Length (Big Endian)
    length = (static_cast<uint32_t>(buffer[2]) << 24) |
             (static_cast<uint32_t>(buffer[3]) << 16) |
             (static_cast<uint32_t>(buffer[4]) << 8) |
             static_cast<uint32_t>(buffer[5]);

    return true;
}

void dicom_session::send_data(std::vector<uint8_t>&& data) {
    std::visit([&data](auto&& session) {
        if (session) {
            session->send_packet(std::move(data));
        }
    }, session_);
}

}  // namespace pacs::integration
