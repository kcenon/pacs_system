// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file tcp_stream.h
 * @brief Blocking TCP client connection with per-call timeouts
 *
 * association::connect() opens one of these to reach a peer that is not an
 * in-process dicom_server. Every call takes a timeout, so a silent or slow
 * peer cannot block an SCU forever, and nothing runs on a background thread.
 *
 * @see Issue #1240 - Real TCP client path in association::connect()
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_NETWORK_DETAIL_TCP_STREAM_HPP
#define PACS_NETWORK_DETAIL_TCP_STREAM_HPP

#include "kcenon/pacs/core/result.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace kcenon::pacs::network::detail {

/**
 * @brief A connected TCP socket that reads and writes with timeouts.
 *
 * Reads and writes may run on different threads at the same time; callers
 * serialize reads among themselves and writes among themselves. shutdown()
 * may be called from any thread to make a blocked read or write return.
 */
class tcp_stream {
public:
    using duration = std::chrono::milliseconds;

    /**
     * @brief Connect to @p host : @p port.
     *
     * IPv4 addresses are tried before IPv6 ones, since dicom_server_v2 and
     * most DICOM peers listen on IPv4 only.
     *
     * @param host Host name or address
     * @param port TCP port
     * @param timeout Time allowed for name resolution and the TCP handshake
     * @return The connection, or connection_failed / connection_timeout
     */
    [[nodiscard]] static kcenon::pacs::Result<std::unique_ptr<tcp_stream>> connect(
        const std::string& host,
        uint16_t port,
        duration timeout);

    /// Closes the socket
    ~tcp_stream();

    tcp_stream(const tcp_stream&) = delete;
    tcp_stream& operator=(const tcp_stream&) = delete;
    tcp_stream(tcp_stream&&) = delete;
    tcp_stream& operator=(tcp_stream&&) = delete;

    /**
     * @brief Write all of @p data.
     * @return Success, or send_failed when the connection broke or the
     *         peer did not take the data within @p timeout
     */
    [[nodiscard]] kcenon::pacs::VoidResult write_all(
        std::span<const uint8_t> data,
        duration timeout);

    /**
     * @brief Read at least one byte into @p buffer.
     * @return The number of bytes read; receive_timeout when nothing arrived
     *         within @p timeout; receive_failed when the peer closed the
     *         connection or it broke
     */
    [[nodiscard]] kcenon::pacs::Result<std::size_t> read_some(
        std::span<uint8_t> buffer,
        duration timeout);

    /**
     * @brief Shut down both directions.
     *
     * A read or write blocked on another thread returns with an error. The
     * socket itself is closed by the destructor.
     */
    void shutdown() noexcept;

private:
    explicit tcp_stream(std::intptr_t handle) noexcept;

    /// Native socket (int on POSIX, SOCKET on Windows)
    std::intptr_t handle_;
};

}  // namespace kcenon::pacs::network::detail

#endif  // PACS_NETWORK_DETAIL_TCP_STREAM_HPP
