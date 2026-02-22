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
 * @file storage_commitment_scu.hpp
 * @brief DICOM Storage Commitment Push Model SCU service
 *
 * Sends N-ACTION commitment requests to an SCP and processes
 * N-EVENT-REPORT responses with per-instance commitment status.
 *
 * @see DICOM PS3.4 Annex J - Storage Commitment Push Model Service Class
 * @see DICOM PS3.7 Section 10.1.4 - N-ACTION Service
 * @see DICOM PS3.7 Section 10.1.1 - N-EVENT-REPORT Service
 */

#ifndef PACS_SERVICES_STORAGE_COMMITMENT_SCU_HPP
#define PACS_SERVICES_STORAGE_COMMITMENT_SCU_HPP

#include "storage_commitment_types.hpp"

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/di/ilogger.hpp"
#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace pacs::services {

/**
 * @brief Storage Commitment Push Model SCU
 *
 * Sends N-ACTION requests to an SCP to request storage commitment
 * for specified SOP Instances, and handles N-EVENT-REPORT responses
 * with per-instance verification results.
 *
 * ## Usage
 *
 * ```cpp
 * storage_commitment_scu scu;
 *
 * // Set callback for commitment results
 * scu.set_commitment_callback([](const auto& uid, const auto& result) {
 *     // Handle commitment result
 * });
 *
 * // Request commitment
 * auto result = scu.request_commitment(assoc, transaction_uid, references);
 * ```
 */
class storage_commitment_scu {
public:
    explicit storage_commitment_scu(
        std::shared_ptr<di::ILogger> logger = nullptr);

    ~storage_commitment_scu() = default;

    // =========================================================================
    // Callback Configuration
    // =========================================================================

    /**
     * @brief Callback type for commitment results
     *
     * Invoked when an N-EVENT-REPORT is received with the commitment result.
     */
    using commitment_callback = std::function<void(
        const std::string& transaction_uid,
        const commitment_result& result)>;

    /**
     * @brief Set callback for commitment result notifications
     * @param cb The callback function
     */
    void set_commitment_callback(commitment_callback cb);

    // =========================================================================
    // N-ACTION Request
    // =========================================================================

    /**
     * @brief Send N-ACTION request to commit stored instances
     *
     * Builds and sends an N-ACTION-RQ with the specified Transaction UID
     * and Referenced SOP Sequence to the connected SCP.
     *
     * @param assoc The established association with the SCP
     * @param context_id The presentation context ID for Storage Commitment
     * @param transaction_uid Unique identifier for this commitment request
     * @param references SOP Instance references to commit
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> request_commitment(
        network::association& assoc,
        uint8_t context_id,
        const std::string& transaction_uid,
        const std::vector<sop_reference>& references);

    // =========================================================================
    // N-EVENT-REPORT Handler
    // =========================================================================

    /**
     * @brief Handle an N-EVENT-REPORT-RQ received from the SCP
     *
     * Parses the event dataset to extract per-instance commitment status
     * and invokes the registered callback.
     *
     * @param event_rq The N-EVENT-REPORT-RQ message
     * @return The parsed commitment result
     */
    [[nodiscard]] network::Result<commitment_result> handle_event_report(
        const network::dimse::dimse_message& event_rq);

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] size_t requests_sent() const noexcept;
    [[nodiscard]] size_t event_reports_received() const noexcept;
    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Dataset Builders
    // =========================================================================

    [[nodiscard]] static core::dicom_dataset build_action_dataset(
        const std::string& transaction_uid,
        const std::vector<sop_reference>& references);

    [[nodiscard]] static commitment_result parse_event_report_dataset(
        const core::dicom_dataset& dataset);

    // =========================================================================
    // Member Variables
    // =========================================================================

    std::shared_ptr<di::ILogger> logger_;
    commitment_callback callback_;

    std::atomic<size_t> requests_sent_{0};
    std::atomic<size_t> event_reports_received_{0};
};

}  // namespace pacs::services

#endif  // PACS_SERVICES_STORAGE_COMMITMENT_SCU_HPP
