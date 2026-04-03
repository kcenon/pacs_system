// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file n_get_scp.h
 * @brief DICOM N-GET SCP service for attribute retrieval
 *
 * This file provides the n_get_scp class for handling N-GET-RQ messages
 * to retrieve attributes from managed SOP Instances (e.g., MPPS status).
 *
 * @see DICOM PS3.7 Section 10.1.2 - N-GET Service
 * @see Issue #720 - Implement N-GET SCP/SCU service
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_N_GET_SCP_HPP
#define PACS_SERVICES_N_GET_SCP_HPP

#include "scp_service.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace kcenon::pacs::services {

// =============================================================================
// Handler Types
// =============================================================================

/**
 * @brief N-GET handler function type
 *
 * Called when an N-GET request is received to retrieve attributes from
 * a managed SOP Instance.
 *
 * The handler should:
 * 1. Look up the SOP Instance by its UID
 * 2. If attribute_tags is empty, return all attributes
 * 3. If attribute_tags is not empty, return only the requested attributes
 * 4. Return the dataset containing the requested attributes
 *
 * @param sop_class_uid The SOP Class UID of the instance
 * @param sop_instance_uid The SOP Instance UID to look up
 * @param attribute_tags Tags to retrieve (empty = all attributes)
 * @return Dataset containing the requested attributes, or error
 */
using n_get_handler = std::function<network::Result<core::dicom_dataset>(
    const std::string& sop_class_uid,
    const std::string& sop_instance_uid,
    const std::vector<core::dicom_tag>& attribute_tags)>;

// =============================================================================
// N-GET SCP Class
// =============================================================================

/**
 * @brief N-GET SCP service for attribute retrieval from managed SOP Instances
 *
 * The N-GET SCP (Service Class Provider) responds to N-GET-RQ messages
 * by looking up the requested SOP Instance and returning its attributes.
 *
 * ## N-GET Message Flow
 *
 * ```
 * SCU (Client)                          SCP (Server)
 *  |                                     |
 *  |  N-GET-RQ                           |
 *  |  +-----------------------------+    |
 *  |  | RequestedSOPClass: MPPS     |    |
 *  |  | RequestedSOPInstance: UID   |    |
 *  |  | AttributeIdentifierList    |    |
 *  |  +-----------------------------+    |
 *  |----------------------------------->|
 *  |                                     |
 *  |                            Look up  |
 *  |                            instance |
 *  |                                     |
 *  |  N-GET-RSP + Dataset                |
 *  |<-----------------------------------|
 * ```
 *
 * @example Usage
 * @code
 * n_get_scp scp;
 *
 * scp.set_handler([&store](const auto& sop_class, const auto& uid,
 *                          const auto& tags) {
 *     auto instance = store.lookup(uid);
 *     if (!instance) {
 *         return pacs_error<dicom_dataset>(...);
 *     }
 *     if (tags.empty()) {
 *         return instance->dataset();
 *     }
 *     return instance->filter_attributes(tags);
 * });
 *
 * auto result = scp.handle_message(association, context_id, request);
 * @endcode
 */
class n_get_scp final : public scp_service {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct N-GET SCP with optional logger
     *
     * @param logger Logger instance (nullptr uses null_logger)
     */
    explicit n_get_scp(std::shared_ptr<di::ILogger> logger = nullptr);

    ~n_get_scp() override = default;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set the N-GET handler function
     *
     * The handler is called for each N-GET request to retrieve attributes
     * from a managed SOP Instance.
     *
     * @param handler The N-GET handler function
     */
    void set_handler(n_get_handler handler);

    /**
     * @brief Add a SOP Class UID that this SCP supports
     *
     * N-GET can be used with multiple SOP Classes (e.g., MPPS, Printer).
     * By default, no SOP Classes are registered. Call this method to add
     * supported SOP Classes.
     *
     * @param sop_class_uid The SOP Class UID to support
     */
    void add_supported_sop_class(std::string sop_class_uid);

    // =========================================================================
    // scp_service Interface Implementation
    // =========================================================================

    /**
     * @brief Get supported SOP Class UIDs
     *
     * @return Vector of registered SOP Class UIDs
     */
    [[nodiscard]] std::vector<std::string> supported_sop_classes() const override;

    /**
     * @brief Handle an incoming N-GET-RQ message
     *
     * @param assoc The association on which the message was received
     * @param context_id The presentation context ID
     * @param request The incoming N-GET-RQ message
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> handle_message(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request) override;

    /**
     * @brief Get the service name
     *
     * @return "N-GET SCP"
     */
    [[nodiscard]] std::string_view service_name() const noexcept override;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total number of N-GET requests processed
     */
    [[nodiscard]] size_t gets_processed() const noexcept;

    /**
     * @brief Reset statistics counters
     */
    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /**
     * @brief Send N-GET response
     */
    [[nodiscard]] network::Result<std::monostate> send_n_get_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        const std::string& sop_class_uid,
        const std::string& sop_instance_uid,
        network::dimse::status_code status,
        core::dicom_dataset* dataset = nullptr);

    // =========================================================================
    // Member Variables
    // =========================================================================

    n_get_handler handler_;
    std::vector<std::string> supported_sop_classes_;

    std::atomic<size_t> gets_processed_{0};
};

}  // namespace kcenon::pacs::services

#endif  // PACS_SERVICES_N_GET_SCP_HPP
