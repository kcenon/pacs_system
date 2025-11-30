/**
 * @file scp_service.hpp
 * @brief Base class for DICOM SCP (Service Class Provider) services
 *
 * This file provides the abstract base class for implementing DICOM SCP services.
 * Each service handles specific SOP Classes and processes DIMSE messages.
 *
 * @see DICOM PS3.4 - Service Class Specifications
 * @see DICOM PS3.7 - Message Exchange
 */

#ifndef PACS_SERVICES_SCP_SERVICE_HPP
#define PACS_SERVICES_SCP_SERVICE_HPP

#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::services {

/**
 * @brief Abstract base class for DICOM SCP services
 *
 * Provides a common interface for all SCP service implementations.
 * Each derived class handles specific SOP Classes and their corresponding
 * DIMSE operations.
 *
 * @example Usage
 * @code
 * class verification_scp : public scp_service {
 * public:
 *     std::vector<std::string> supported_sop_classes() const override {
 *         return {"1.2.840.10008.1.1"};  // Verification SOP Class
 *     }
 *
 *     network::Result<std::monostate> handle_message(
 *         network::association& assoc,
 *         const network::dimse::dimse_message& request) override;
 * };
 * @endcode
 */
class scp_service {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    scp_service() = default;
    virtual ~scp_service() = default;

    // Non-copyable but movable
    scp_service(const scp_service&) = delete;
    scp_service& operator=(const scp_service&) = delete;
    scp_service(scp_service&&) = default;
    scp_service& operator=(scp_service&&) = default;

    // =========================================================================
    // Service Interface
    // =========================================================================

    /**
     * @brief Get the list of SOP Class UIDs supported by this service
     *
     * @return Vector of SOP Class UIDs that this service can handle
     */
    [[nodiscard]] virtual std::vector<std::string> supported_sop_classes() const = 0;

    /**
     * @brief Handle an incoming DIMSE message
     *
     * Processes the request and sends appropriate response(s) via the association.
     *
     * @param assoc The association on which the message was received
     * @param context_id The presentation context ID for the message
     * @param request The incoming DIMSE request message
     * @return Success or error result
     */
    [[nodiscard]] virtual network::Result<std::monostate> handle_message(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request) = 0;

    // =========================================================================
    // Service Information
    // =========================================================================

    /**
     * @brief Get the service name for logging/debugging
     *
     * @return Human-readable service name
     */
    [[nodiscard]] virtual std::string_view service_name() const noexcept = 0;

    /**
     * @brief Check if this service supports a specific SOP Class
     *
     * @param sop_class_uid The SOP Class UID to check
     * @return true if the SOP Class is supported
     */
    [[nodiscard]] bool supports_sop_class(std::string_view sop_class_uid) const {
        const auto classes = supported_sop_classes();
        for (const auto& uid : classes) {
            if (uid == sop_class_uid) {
                return true;
            }
        }
        return false;
    }
};

/**
 * @brief Shared pointer type for SCP services
 */
using scp_service_ptr = std::shared_ptr<scp_service>;

}  // namespace pacs::services

#endif  // PACS_SERVICES_SCP_SERVICE_HPP
