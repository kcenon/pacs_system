/**
 * @file retrieve_scp.hpp
 * @brief DICOM Retrieve SCP service (C-MOVE/C-GET handler)
 *
 * This file provides the retrieve_scp class for handling C-MOVE and C-GET
 * requests. The Retrieve SCP retrieves DICOM images from the PACS archive
 * and either transfers them to a destination (C-MOVE) or returns them
 * directly to the requester (C-GET).
 *
 * @see DICOM PS3.4 Section C - Query/Retrieve Service Class
 * @see DICOM PS3.7 Section 9.1.3 - C-MOVE Service
 * @see DICOM PS3.7 Section 9.1.4 - C-GET Service
 * @see DES-SVC-005 - Retrieve SCP Design Specification
 */

#ifndef PACS_SERVICES_RETRIEVE_SCP_HPP
#define PACS_SERVICES_RETRIEVE_SCP_HPP

#include "scp_service.hpp"
#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_file.hpp"

#include <atomic>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace pacs::services {

// =============================================================================
// SOP Class UIDs
// =============================================================================

/// Patient Root Query/Retrieve Information Model - MOVE
inline constexpr std::string_view patient_root_move_sop_class_uid =
    "1.2.840.10008.5.1.4.1.2.1.2";

/// Study Root Query/Retrieve Information Model - MOVE
inline constexpr std::string_view study_root_move_sop_class_uid =
    "1.2.840.10008.5.1.4.1.2.2.2";

/// Patient Root Query/Retrieve Information Model - GET
inline constexpr std::string_view patient_root_get_sop_class_uid =
    "1.2.840.10008.5.1.4.1.2.1.3";

/// Study Root Query/Retrieve Information Model - GET
inline constexpr std::string_view study_root_get_sop_class_uid =
    "1.2.840.10008.5.1.4.1.2.2.3";

// =============================================================================
// Sub-operation Statistics
// =============================================================================

/**
 * @brief Statistics for C-MOVE/C-GET sub-operations
 *
 * Tracks the progress of sub-operations during a retrieve operation.
 */
struct sub_operation_stats {
    uint16_t remaining{0};   ///< Number of remaining sub-operations
    uint16_t completed{0};   ///< Number of completed sub-operations
    uint16_t failed{0};      ///< Number of failed sub-operations
    uint16_t warning{0};     ///< Number of sub-operations with warnings

    /**
     * @brief Get total number of sub-operations
     * @return Total count
     */
    [[nodiscard]] uint16_t total() const noexcept {
        return remaining + completed + failed + warning;
    }

    /**
     * @brief Check if all operations completed successfully
     * @return true if no failures
     */
    [[nodiscard]] bool all_successful() const noexcept {
        return failed == 0;
    }
};

// =============================================================================
// Handler Types
// =============================================================================

/**
 * @brief Retrieve handler function type
 *
 * Called by retrieve_scp to get matching DICOM files for a retrieve query.
 *
 * @param query_keys The query dataset containing search criteria
 * @return Vector of matching DICOM files (empty if no matches)
 */
using retrieve_handler = std::function<std::vector<core::dicom_file>(
    const core::dicom_dataset& query_keys)>;

/**
 * @brief Destination resolver function type
 *
 * Called by retrieve_scp to resolve a Move Destination AE title
 * to a network address (host and port).
 *
 * @param ae_title The AE title of the destination
 * @return Optional pair of (host, port) if resolved, std::nullopt if unknown
 */
using destination_resolver = std::function<
    std::optional<std::pair<std::string, uint16_t>>(
        const std::string& ae_title)>;

/**
 * @brief Store sub-operation function type
 *
 * Called by retrieve_scp to perform a C-STORE sub-operation.
 * This is used for both C-MOVE (to external destination) and C-GET
 * (back to requesting SCU on the same association).
 *
 * @param assoc The association to use for C-STORE
 * @param context_id The presentation context ID
 * @param file The DICOM file to store
 * @param move_originator_ae The original requester's AE title (for C-MOVE)
 * @param move_originator_msg_id The original message ID (for C-MOVE)
 * @return Status code from the C-STORE operation
 */
using store_sub_operation = std::function<network::dimse::status_code(
    network::association& assoc,
    uint8_t context_id,
    const core::dicom_file& file,
    const std::string& move_originator_ae,
    uint16_t move_originator_msg_id)>;

/**
 * @brief Cancel check function type
 *
 * Called periodically during retrieve processing to check if a C-CANCEL
 * request has been received.
 *
 * @return true if cancel has been requested
 */
using retrieve_cancel_check = std::function<bool()>;

// =============================================================================
// Retrieve SCP Class
// =============================================================================

/**
 * @brief Retrieve SCP service for handling C-MOVE and C-GET requests
 *
 * The Retrieve SCP (Service Class Provider) responds to C-MOVE and C-GET
 * requests from SCU (Service Class User) applications. It supports both
 * Patient Root and Study Root Query/Retrieve Information Models.
 *
 * ## C-MOVE Message Flow
 *
 * ```
 * Viewer (SCU)          PACS (SCP)                  Destination (SCP)
 *     │                     │                            │
 *     │  C-MOVE-RQ          │                            │
 *     │  MoveDestination:   │                            │
 *     │    VIEWER_SCP       │                            │
 *     │────────────────────►│                            │
 *     │                     │                            │
 *     │                     │  Establish sub-association │
 *     │                     │───────────────────────────►│
 *     │                     │                            │
 *     │  C-MOVE-RSP         │  C-STORE-RQ (image 1)     │
 *     │  (Pending: 50)      │───────────────────────────►│
 *     │◄────────────────────│                            │
 *     │                     │◄───────────────────────────│
 *     │                     │  C-STORE-RSP (Success)     │
 *     │                     │                            │
 *     │  ... (repeat)       │  ... (repeat)              │
 *     │                     │                            │
 *     │  C-MOVE-RSP         │                            │
 *     │  (Success)          │                            │
 *     │  Completed: 50      │                            │
 *     │  Failed: 0          │                            │
 *     │◄────────────────────│                            │
 * ```
 *
 * ## C-GET Message Flow
 *
 * ```
 * Viewer (SCU/SCP)                PACS (SCP/SCU)
 *     │                                │
 *     │  C-GET-RQ                      │
 *     │───────────────────────────────►│
 *     │                                │
 *     │  C-GET-RSP (Pending: 50)       │
 *     │◄───────────────────────────────│
 *     │                                │
 *     │  C-STORE-RQ (image 1)          │  (on same association)
 *     │◄───────────────────────────────│
 *     │  C-STORE-RSP (Success)         │
 *     │───────────────────────────────►│
 *     │                                │
 *     │  ... (repeat)                  │
 *     │                                │
 *     │  C-GET-RSP (Success)           │
 *     │  Completed: 50                 │
 *     │◄───────────────────────────────│
 * ```
 *
 * @example Usage
 * @code
 * retrieve_scp scp;
 *
 * // Set up retrieve handler
 * scp.set_retrieve_handler([&storage](const auto& query_keys) {
 *     return storage.find_matching_files(query_keys);
 * });
 *
 * // Set up destination resolver (for C-MOVE)
 * scp.set_destination_resolver([](const std::string& ae) {
 *     if (ae == "VIEWER") return std::make_pair("192.168.1.10", 11112);
 *     return std::nullopt;
 * });
 *
 * // Handle incoming request
 * auto result = scp.handle_message(association, context_id, request);
 * @endcode
 */
class retrieve_scp final : public scp_service {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct a Retrieve SCP
     */
    retrieve_scp();

    ~retrieve_scp() override = default;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set the retrieve handler function
     *
     * The handler is called for each retrieve request to find matching
     * DICOM files in storage.
     *
     * @param handler The retrieve handler function
     */
    void set_retrieve_handler(retrieve_handler handler);

    /**
     * @brief Set the destination resolver function
     *
     * The resolver maps AE titles to network addresses for C-MOVE operations.
     *
     * @param resolver The destination resolver function
     */
    void set_destination_resolver(destination_resolver resolver);

    /**
     * @brief Set the store sub-operation handler
     *
     * The store handler performs C-STORE sub-operations during C-MOVE/C-GET.
     * If not set, a default implementation using the association's send_dimse
     * will be used.
     *
     * @param handler The store sub-operation handler
     */
    void set_store_sub_operation(store_sub_operation handler);

    /**
     * @brief Set the cancel check function
     *
     * The cancel check is called periodically during retrieve processing
     * to check if a C-CANCEL has been received.
     *
     * @param check The cancel check function
     */
    void set_cancel_check(retrieve_cancel_check check);

    // =========================================================================
    // scp_service Interface Implementation
    // =========================================================================

    /**
     * @brief Get supported SOP Class UIDs
     *
     * @return Vector containing Patient Root and Study Root Move/Get SOP Classes
     */
    [[nodiscard]] std::vector<std::string> supported_sop_classes() const override;

    /**
     * @brief Handle an incoming DIMSE message (C-MOVE-RQ or C-GET-RQ)
     *
     * Processes the retrieve request:
     * 1. Validates the message type (C-MOVE-RQ or C-GET-RQ)
     * 2. Extracts query keys from the dataset
     * 3. Retrieves matching files via the retrieve handler
     * 4. For C-MOVE: resolves destination and establishes sub-association
     * 5. Performs C-STORE sub-operations for each file
     * 6. Sends pending responses with progress updates
     * 7. Sends final response with operation statistics
     *
     * @param assoc The association on which the message was received
     * @param context_id The presentation context ID for the message
     * @param request The incoming C-MOVE-RQ or C-GET-RQ message
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> handle_message(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request) override;

    /**
     * @brief Get the service name
     * @return "Retrieve SCP"
     */
    [[nodiscard]] std::string_view service_name() const noexcept override;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total number of C-MOVE operations processed
     * @return Number of C-MOVE requests handled
     */
    [[nodiscard]] size_t move_operations() const noexcept;

    /**
     * @brief Get total number of C-GET operations processed
     * @return Number of C-GET requests handled
     */
    [[nodiscard]] size_t get_operations() const noexcept;

    /**
     * @brief Get total number of images transferred
     * @return Number of images sent via C-STORE sub-operations
     */
    [[nodiscard]] size_t images_transferred() const noexcept;

    /**
     * @brief Reset statistics counters
     */
    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /**
     * @brief Handle a C-MOVE request
     *
     * @param assoc The association
     * @param context_id The presentation context ID
     * @param request The C-MOVE-RQ message
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> handle_c_move(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    /**
     * @brief Handle a C-GET request
     *
     * @param assoc The association
     * @param context_id The presentation context ID
     * @param request The C-GET-RQ message
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> handle_c_get(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    /**
     * @brief Send a pending response with progress information
     *
     * @param assoc The association
     * @param context_id The presentation context ID
     * @param message_id The original request message ID
     * @param sop_class_uid The SOP Class UID
     * @param is_move true for C-MOVE-RSP, false for C-GET-RSP
     * @param stats Current sub-operation statistics
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> send_pending_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        std::string_view sop_class_uid,
        bool is_move,
        const sub_operation_stats& stats);

    /**
     * @brief Send a final response with completion status
     *
     * @param assoc The association
     * @param context_id The presentation context ID
     * @param message_id The original request message ID
     * @param sop_class_uid The SOP Class UID
     * @param is_move true for C-MOVE-RSP, false for C-GET-RSP
     * @param stats Final sub-operation statistics
     * @param was_cancelled true if operation was cancelled
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> send_final_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        std::string_view sop_class_uid,
        bool is_move,
        const sub_operation_stats& stats,
        bool was_cancelled);

    /**
     * @brief Get the Move Destination AE title from the request
     *
     * @param request The C-MOVE-RQ message
     * @return Move Destination AE title, or empty string if not present
     */
    [[nodiscard]] std::string get_move_destination(
        const network::dimse::dimse_message& request) const;

    // =========================================================================
    // Member Variables
    // =========================================================================

    retrieve_handler retrieve_handler_;
    destination_resolver destination_resolver_;
    store_sub_operation store_handler_;
    retrieve_cancel_check cancel_check_;

    std::atomic<size_t> move_operations_{0};
    std::atomic<size_t> get_operations_{0};
    std::atomic<size_t> images_transferred_{0};
};

}  // namespace pacs::services

#endif  // PACS_SERVICES_RETRIEVE_SCP_HPP
