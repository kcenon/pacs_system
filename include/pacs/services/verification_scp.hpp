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

    verification_scp() = default;
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
