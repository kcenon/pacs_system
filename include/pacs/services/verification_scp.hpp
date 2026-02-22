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
 * @file verification_scp.hpp
 * @brief DICOM Verification SCP service (C-ECHO handler)
 *
 * This file provides the verification_scp class for handling C-ECHO requests.
 * C-ECHO is the simplest DICOM service, used to verify network connectivity
 * between DICOM applications (similar to ping).
 *
 * @see DICOM PS3.4 Section A.4 - Verification Service Class
 * @see DICOM PS3.7 Section 9.1 - C-ECHO Service
 * @see DES-SVC-001 - Verification SCP Design Specification
 */

#ifndef PACS_SERVICES_VERIFICATION_SCP_HPP
#define PACS_SERVICES_VERIFICATION_SCP_HPP

#include "scp_service.hpp"

namespace pacs::services {

/// Verification SOP Class UID (1.2.840.10008.1.1)
inline constexpr std::string_view verification_sop_class_uid = "1.2.840.10008.1.1";

/**
 * @brief Verification SCP service for handling C-ECHO requests
 *
 * The Verification SCP (Service Class Provider) responds to C-ECHO requests
 * from SCU (Service Class User) applications. This is the most basic DICOM
 * service, equivalent to a "ping" to verify connectivity.
 *
 * ## C-ECHO Message Flow
 *
 * ```
 * SCU                                    SCP (this class)
 *  │                                      │
 *  │  C-ECHO-RQ                           │
 *  │  ┌──────────────────────────────────┐│
 *  │  │ CommandField: 0x0030             ││
 *  │  │ MessageID: N                     ││
 *  │  │ AffectedSOPClassUID: 1.2...1.1   ││
 *  │  └──────────────────────────────────┘│
 *  │─────────────────────────────────────►│
 *  │                                      │ handle_message()
 *  │  C-ECHO-RSP                          │
 *  │  ┌──────────────────────────────────┐│
 *  │  │ CommandField: 0x8030             ││
 *  │  │ MessageIDBeingRespondedTo: N     ││
 *  │  │ Status: 0x0000 (Success)         ││
 *  │  └──────────────────────────────────┘│
 *  │◄─────────────────────────────────────│
 *  │                                      │
 * ```
 *
 * @example Usage
 * @code
 * verification_scp scp;
 *
 * // Check supported SOP classes
 * auto classes = scp.supported_sop_classes();
 * assert(classes[0] == "1.2.840.10008.1.1");
 *
 * // Handle incoming C-ECHO request
 * dimse_message request{command_field::c_echo_rq, 1};
 * auto result = scp.handle_message(association, context_id, request);
 * assert(result.is_ok());
 * @endcode
 */
class verification_scp final : public scp_service {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct Verification SCP with optional logger
     *
     * @param logger Logger instance for service logging (nullptr uses null_logger)
     */
    explicit verification_scp(std::shared_ptr<di::ILogger> logger = nullptr)
        : scp_service(std::move(logger)) {}

    ~verification_scp() override = default;

    // =========================================================================
    // scp_service Interface Implementation
    // =========================================================================

    /**
     * @brief Get supported SOP Class UIDs
     *
     * @return Vector containing only the Verification SOP Class UID
     */
    [[nodiscard]] std::vector<std::string> supported_sop_classes() const override;

    /**
     * @brief Handle an incoming DIMSE message (C-ECHO-RQ)
     *
     * Processes the C-ECHO request and sends a C-ECHO response with success status.
     *
     * @param assoc The association on which the message was received
     * @param context_id The presentation context ID for the message
     * @param request The incoming C-ECHO-RQ message
     * @return Success or error if the message is not a valid C-ECHO-RQ
     *
     * @note This method rejects any message that is not a C-ECHO-RQ
     */
    [[nodiscard]] network::Result<std::monostate> handle_message(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request) override;

    /**
     * @brief Get the service name
     *
     * @return "Verification SCP"
     */
    [[nodiscard]] std::string_view service_name() const noexcept override;
};

}  // namespace pacs::services

#endif  // PACS_SERVICES_VERIFICATION_SCP_HPP
