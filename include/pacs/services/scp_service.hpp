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
 * @file scp_service.hpp
 * @brief Base class for DICOM SCP (Service Class Provider) services
 *
 * This file provides the abstract base class for implementing DICOM SCP services.
 * Each service handles specific SOP Classes and processes DIMSE messages.
 *
 * @see DICOM PS3.4 - Service Class Specifications
 * @see DICOM PS3.7 - Message Exchange
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_SCP_SERVICE_HPP
#define PACS_SERVICES_SCP_SERVICE_HPP

#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/di/ilogger.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace kcenon::pacs::services {

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

    /**
     * @brief Construct SCP service with optional logger
     *
     * @param logger Logger instance for service logging (nullptr uses null_logger)
     */
    explicit scp_service(std::shared_ptr<di::ILogger> logger = nullptr)
        : logger_(logger ? std::move(logger) : di::null_logger()) {}

    virtual ~scp_service() = default;

    // Non-copyable but movable
    scp_service(const scp_service&) = delete;
    scp_service& operator=(const scp_service&) = delete;
    scp_service(scp_service&&) = default;
    scp_service& operator=(scp_service&&) = default;

    // =========================================================================
    // Logger Access
    // =========================================================================

    /**
     * @brief Set the logger instance
     *
     * @param logger New logger instance (nullptr uses null_logger)
     */
    void set_logger(std::shared_ptr<di::ILogger> logger) {
        logger_ = logger ? std::move(logger) : di::null_logger();
    }

    /**
     * @brief Get the current logger instance
     *
     * @return Shared pointer to the logger
     */
    [[nodiscard]] const std::shared_ptr<di::ILogger>& logger() const noexcept {
        return logger_;
    }

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

protected:
    // =========================================================================
    // Protected Members
    // =========================================================================

    /// Logger instance for service logging
    std::shared_ptr<di::ILogger> logger_;
};

/**
 * @brief Shared pointer type for SCP services
 */
using scp_service_ptr = std::shared_ptr<scp_service>;

}  // namespace kcenon::pacs::services

#endif  // PACS_SERVICES_SCP_SERVICE_HPP
