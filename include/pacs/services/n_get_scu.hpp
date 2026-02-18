/**
 * @file n_get_scu.hpp
 * @brief DICOM N-GET SCU service for attribute retrieval
 *
 * This file provides the n_get_scu class for querying remote SCPs
 * to retrieve attributes from managed SOP Instances.
 *
 * @see DICOM PS3.7 Section 10.1.2 - N-GET Service
 * @see Issue #720 - Implement N-GET SCP/SCU service
 */

#ifndef PACS_SERVICES_N_GET_SCU_HPP
#define PACS_SERVICES_N_GET_SCU_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/di/ilogger.hpp"
#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::services {

// =============================================================================
// N-GET SCU Data Structures
// =============================================================================

/**
 * @brief Result of an N-GET operation
 *
 * Contains the retrieved attributes and status information.
 */
struct n_get_result {
    /// DIMSE status code (0x0000 = success)
    uint16_t status{0};

    /// Retrieved attributes from the SOP Instance
    core::dicom_dataset attributes;

    /// Error comment from the SCP (if any)
    std::string error_comment;

    /// Time taken for the operation
    std::chrono::milliseconds elapsed{0};

    /// Check if the operation was successful
    [[nodiscard]] bool is_success() const noexcept {
        return status == 0x0000;
    }

    /// Check if this was a warning status
    [[nodiscard]] bool is_warning() const noexcept {
        return (status & 0xF000) == 0xB000;
    }

    /// Check if this was an error status
    [[nodiscard]] bool is_error() const noexcept {
        return !is_success() && !is_warning();
    }
};

/**
 * @brief Configuration for N-GET SCU service
 */
struct n_get_scu_config {
    /// Timeout for receiving DIMSE response
    std::chrono::milliseconds timeout{30000};
};

// =============================================================================
// N-GET SCU Class
// =============================================================================

/**
 * @brief N-GET SCU service for querying SOP Instance attributes
 *
 * The N-GET SCU (Service Class User) sends N-GET-RQ messages to remote
 * SCPs to retrieve attributes from managed SOP Instances.
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
 *  |  N-GET-RSP + Dataset                |
 *  |<-----------------------------------|
 * ```
 *
 * @example Usage
 * @code
 * n_get_scu scu;
 *
 * // Get all attributes
 * auto result = scu.get(assoc, mpps_sop_class_uid, instance_uid);
 *
 * // Get specific attributes
 * std::vector<dicom_tag> tags = {tags::patient_name, tags::patient_id};
 * auto result = scu.get(assoc, mpps_sop_class_uid, instance_uid, tags);
 *
 * if (result.is_ok() && result.value().is_success()) {
 *     auto& attrs = result.value().attributes;
 *     auto name = attrs.get_string(tags::patient_name);
 * }
 * @endcode
 */
class n_get_scu {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct N-GET SCU with default configuration
     *
     * @param logger Logger instance (nullptr uses null_logger)
     */
    explicit n_get_scu(std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Construct N-GET SCU with custom configuration
     *
     * @param config Configuration options
     * @param logger Logger instance (nullptr uses null_logger)
     */
    explicit n_get_scu(const n_get_scu_config& config,
                       std::shared_ptr<di::ILogger> logger = nullptr);

    ~n_get_scu() = default;

    // Non-copyable, non-movable (due to atomic members)
    n_get_scu(const n_get_scu&) = delete;
    n_get_scu& operator=(const n_get_scu&) = delete;
    n_get_scu(n_get_scu&&) = delete;
    n_get_scu& operator=(n_get_scu&&) = delete;

    // =========================================================================
    // N-GET Operation
    // =========================================================================

    /**
     * @brief Retrieve attributes from a managed SOP Instance
     *
     * Sends an N-GET-RQ and returns the retrieved attributes.
     *
     * @param assoc The established association to use
     * @param sop_class_uid The SOP Class UID of the instance
     * @param sop_instance_uid The SOP Instance UID to query
     * @param attribute_tags Tags to retrieve (empty = all attributes)
     * @return Result containing n_get_result, or error
     */
    [[nodiscard]] network::Result<n_get_result> get(
        network::association& assoc,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        const std::vector<core::dicom_tag>& attribute_tags = {});

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get the number of N-GET operations performed
     */
    [[nodiscard]] size_t gets_performed() const noexcept;

    /**
     * @brief Reset statistics counters to zero
     */
    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /**
     * @brief Get the next message ID for DIMSE operations
     */
    [[nodiscard]] uint16_t next_message_id() noexcept;

    // =========================================================================
    // Private Members
    // =========================================================================

    std::shared_ptr<di::ILogger> logger_;
    n_get_scu_config config_;
    std::atomic<uint16_t> message_id_counter_{1};
    std::atomic<size_t> gets_performed_{0};
};

}  // namespace pacs::services

#endif  // PACS_SERVICES_N_GET_SCU_HPP
