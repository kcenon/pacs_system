/**
 * @file storage_scp.hpp
 * @brief DICOM Storage SCP service (C-STORE handler)
 *
 * This file provides the storage_scp class for handling C-STORE requests.
 * The Storage SCP receives DICOM images from SCU applications (modalities,
 * workstations) and stores them in the PACS archive.
 *
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.7 Section 9.1.1 - C-STORE Service
 * @see DES-SVC-002 - Storage SCP Design Specification
 */

#ifndef PACS_SERVICES_STORAGE_SCP_HPP
#define PACS_SERVICES_STORAGE_SCP_HPP

#include "scp_service.hpp"
#include "storage_status.hpp"
#include "pacs/core/dicom_dataset.hpp"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace pacs::services {

/**
 * @brief Policy for handling duplicate SOP instances
 */
enum class duplicate_policy {
    reject,     ///< Reject duplicates with error status
    replace,    ///< Replace existing instance with new one
    ignore      ///< Silently accept duplicate (return success)
};

/**
 * @brief Configuration for Storage SCP service
 */
struct storage_scp_config {
    /// List of accepted SOP Class UIDs (empty = accept all standard storage classes)
    std::vector<std::string> accepted_sop_classes;

    /// Policy for handling duplicate SOP instances
    duplicate_policy duplicate_policy = duplicate_policy::reject;
};

/**
 * @brief Callback type for handling received DICOM images
 *
 * @param dataset The received DICOM dataset
 * @param calling_ae The AE title of the sending application
 * @param sop_class_uid The SOP Class UID of the instance
 * @param sop_instance_uid The unique identifier of the instance
 * @return Status indicating success/failure of storage operation
 */
using storage_handler = std::function<storage_status(
    const core::dicom_dataset& dataset,
    const std::string& calling_ae,
    const std::string& sop_class_uid,
    const std::string& sop_instance_uid)>;

/**
 * @brief Callback type for pre-store validation
 *
 * Called before the storage handler to validate incoming datasets.
 * Return false to reject the storage request.
 *
 * @param dataset The received DICOM dataset
 * @return true to proceed with storage, false to reject
 */
using pre_store_handler = std::function<bool(const core::dicom_dataset& dataset)>;

/**
 * @brief Storage SCP service for handling C-STORE requests
 *
 * The Storage SCP (Service Class Provider) receives DICOM images via C-STORE
 * operations from modalities, workstations, or other PACS systems. It validates
 * incoming data, handles duplicates according to policy, and delegates actual
 * storage to a registered handler.
 *
 * ## C-STORE Message Flow
 *
 * ```
 * Modality (SCU)                          PACS (SCP - this class)
 *  │                                      │
 *  │  C-STORE-RQ                          │
 *  │  ┌──────────────────────────────────┐│
 *  │  │ CommandField: 0x0001             ││
 *  │  │ AffectedSOPClassUID: CT Image    ││
 *  │  │ AffectedSOPInstanceUID: 1.2.3... ││
 *  │  │ Priority: MEDIUM                  ││
 *  │  └──────────────────────────────────┘│
 *  │─────────────────────────────────────►│
 *  │                                      │
 *  │  Dataset (pixel data)                │
 *  │─────────────────────────────────────►│
 *  │                                      │
 *  │                          Pre-validate│
 *  │                          Store file  │
 *  │                          Update index│
 *  │                                      │
 *  │  C-STORE-RSP                         │
 *  │  ┌──────────────────────────────────┐│
 *  │  │ Status: 0x0000 (Success)         ││
 *  │  └──────────────────────────────────┘│
 *  │◄─────────────────────────────────────│
 *  │                                      │
 * ```
 *
 * @example Usage
 * @code
 * storage_scp_config config;
 * config.accepted_sop_classes = {"1.2.840.10008.5.1.4.1.1.2"};  // CT
 * config.duplicate_policy = duplicate_policy::reject;
 *
 * storage_scp scp{config};
 *
 * // Register storage handler
 * scp.set_handler([](const auto& dataset, const auto& ae,
 *                    const auto& sop_class, const auto& sop_uid) {
 *     // Store the dataset to disk/database
 *     return storage_status::success;
 * });
 *
 * // Optional: register pre-validation handler
 * scp.set_pre_store_handler([](const auto& dataset) {
 *     // Validate required attributes exist
 *     return dataset.contains(core::tags::patient_name);
 * });
 * @endcode
 */
class storage_scp final : public scp_service {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct a Storage SCP with default configuration
     */
    storage_scp();

    /**
     * @brief Construct a Storage SCP with custom configuration
     * @param config Configuration options
     */
    explicit storage_scp(const storage_scp_config& config);

    ~storage_scp() override = default;

    // =========================================================================
    // Handler Registration
    // =========================================================================

    /**
     * @brief Set the storage handler callback
     *
     * This handler is called for each received C-STORE request after
     * pre-validation passes. It should perform the actual storage operation.
     *
     * @param handler The storage callback function
     */
    void set_handler(storage_handler handler);

    /**
     * @brief Set the pre-store validation handler
     *
     * This handler is called before the main storage handler to validate
     * incoming datasets. Return false to reject the storage request.
     *
     * @param handler The pre-validation callback function
     */
    void set_pre_store_handler(pre_store_handler handler);

    // =========================================================================
    // scp_service Interface Implementation
    // =========================================================================

    /**
     * @brief Get supported SOP Class UIDs
     *
     * Returns the list of Storage SOP Classes this service accepts.
     * If no specific classes are configured, returns all standard storage classes.
     *
     * @return Vector of accepted SOP Class UIDs
     */
    [[nodiscard]] std::vector<std::string> supported_sop_classes() const override;

    /**
     * @brief Handle an incoming DIMSE message (C-STORE-RQ)
     *
     * Processes the C-STORE request:
     * 1. Validates the message is a C-STORE-RQ
     * 2. Extracts SOP Class/Instance UIDs
     * 3. Calls pre-store handler (if registered)
     * 4. Calls storage handler
     * 5. Sends C-STORE-RSP with appropriate status
     *
     * @param assoc The association on which the message was received
     * @param context_id The presentation context ID for the message
     * @param request The incoming C-STORE-RQ message
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> handle_message(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request) override;

    /**
     * @brief Get the service name
     * @return "Storage SCP"
     */
    [[nodiscard]] std::string_view service_name() const noexcept override;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get the number of images received since construction
     * @return Count of successfully received images
     */
    [[nodiscard]] size_t images_received() const noexcept;

    /**
     * @brief Get the total bytes received since construction
     * @return Total bytes of dataset data received
     */
    [[nodiscard]] size_t bytes_received() const noexcept;

    /**
     * @brief Reset statistics counters to zero
     */
    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Members
    // =========================================================================

    /// Configuration
    storage_scp_config config_;

    /// Main storage handler
    storage_handler handler_;

    /// Pre-store validation handler
    pre_store_handler pre_store_handler_;

    /// Statistics: number of images received
    std::atomic<size_t> images_received_{0};

    /// Statistics: total bytes received
    std::atomic<size_t> bytes_received_{0};
};

// =============================================================================
// Standard Storage SOP Class UIDs
// =============================================================================

/// @name Common Storage SOP Class UIDs
/// @{

/// CT Image Storage
inline constexpr std::string_view ct_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.2";

/// Enhanced CT Image Storage
inline constexpr std::string_view enhanced_ct_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.2.1";

/// MR Image Storage
inline constexpr std::string_view mr_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.4";

/// Enhanced MR Image Storage
inline constexpr std::string_view enhanced_mr_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.4.1";

/// CR Image Storage
inline constexpr std::string_view cr_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.1";

/// Digital X-Ray Image Storage - For Presentation
inline constexpr std::string_view dx_image_storage_presentation_uid =
    "1.2.840.10008.5.1.4.1.1.1.1";

/// Digital X-Ray Image Storage - For Processing
inline constexpr std::string_view dx_image_storage_processing_uid =
    "1.2.840.10008.5.1.4.1.1.1.1.1";

/// US Image Storage
inline constexpr std::string_view us_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.6.1";

/// Secondary Capture Image Storage
inline constexpr std::string_view secondary_capture_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.7";

/// RT Image Storage
inline constexpr std::string_view rt_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.481.1";

/// RT Dose Storage
inline constexpr std::string_view rt_dose_storage_uid =
    "1.2.840.10008.5.1.4.1.1.481.2";

/// RT Structure Set Storage
inline constexpr std::string_view rt_structure_set_storage_uid =
    "1.2.840.10008.5.1.4.1.1.481.3";

/// RT Plan Storage
inline constexpr std::string_view rt_plan_storage_uid =
    "1.2.840.10008.5.1.4.1.1.481.5";

/// @}

/**
 * @brief Get a list of all standard Storage SOP Class UIDs
 *
 * This function returns a comprehensive list of commonly supported
 * storage SOP classes for a typical PACS implementation.
 *
 * @return Vector of standard Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string> get_standard_storage_sop_classes();

}  // namespace pacs::services

#endif  // PACS_SERVICES_STORAGE_SCP_HPP
