// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file server_config.hpp
 * @brief DICOM Server configuration structures
 *
 * This file provides configuration structures for the dicom_server class.
 *
 * @see DICOM PS3.8 - Network Communication Support for Message Exchange
 * @see DES-NET-005 - DICOM Server Class Design Specification
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_NETWORK_SERVER_CONFIG_HPP
#define PACS_NETWORK_SERVER_CONFIG_HPP

#include "pacs/network/pdu_types.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace kcenon::pacs::network {

/**
 * @brief Configuration for DICOM server
 *
 * Defines all configurable parameters for a DICOM server instance.
 *
 * @example Usage
 * @code
 * server_config config;
 * config.ae_title = "MY_PACS";
 * config.port = 11112;
 * config.max_associations = 10;
 * config.ae_whitelist = {"MODALITY1", "MODALITY2"};
 *
 * dicom_server server{config};
 * @endcode
 */
struct server_config {
    /// Application Entity Title for this server (16 chars max)
    std::string ae_title{"PACS_SCP"};

    /// Port to listen on (default: 11112, standard alternate DICOM port)
    uint16_t port{11112};

    /// Maximum concurrent associations (0 = unlimited)
    size_t max_associations{20};

    /// Maximum PDU size for data transfer
    uint32_t max_pdu_size{DEFAULT_MAX_PDU_LENGTH};

    /// Idle timeout for associations (0 = no timeout)
    std::chrono::seconds idle_timeout{300};

    /// Timeout for association negotiation
    std::chrono::seconds association_timeout{30};

    /// AE Title whitelist (empty = accept all)
    std::vector<std::string> ae_whitelist;

    /// Implementation Class UID
    std::string implementation_class_uid{"1.2.826.0.1.3680043.2.1545.1"};

    /// Implementation Version Name
    std::string implementation_version_name{"PACS_SYSTEM_001"};

    /// Accept unknown calling AE titles (when whitelist is non-empty)
    bool accept_unknown_calling_ae{false};

    /**
     * @brief Default constructor with sensible defaults
     */
    server_config() = default;

    /**
     * @brief Construct with minimal required parameters
     * @param ae AE Title for the server
     * @param listen_port Port to listen on
     */
    server_config(std::string ae, uint16_t listen_port)
        : ae_title(std::move(ae))
        , port(listen_port) {}
};

/**
 * @brief Statistics for server monitoring
 */
struct server_statistics {
    /// Total associations since server start
    uint64_t total_associations{0};

    /// Currently active associations
    size_t active_associations{0};

    /// Total associations rejected due to limit
    uint64_t rejected_associations{0};

    /// Total DIMSE messages processed
    uint64_t messages_processed{0};

    /// Total bytes received
    uint64_t bytes_received{0};

    /// Total bytes sent
    uint64_t bytes_sent{0};

    /// Server start time
    std::chrono::steady_clock::time_point start_time{};

    /// Time of last activity
    std::chrono::steady_clock::time_point last_activity{};

    /**
     * @brief Get server uptime
     * @return Duration since server start
     */
    [[nodiscard]] std::chrono::seconds uptime() const noexcept {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    }
};

}  // namespace kcenon::pacs::network

#endif  // PACS_NETWORK_SERVER_CONFIG_HPP
