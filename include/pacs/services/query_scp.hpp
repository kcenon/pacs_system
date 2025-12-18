/**
 * @file query_scp.hpp
 * @brief DICOM Query SCP service (C-FIND handler)
 *
 * This file provides the query_scp class for handling C-FIND requests
 * at Patient/Study/Series/Image query levels.
 *
 * @see DICOM PS3.4 Section C - Query/Retrieve Service Class
 * @see DICOM PS3.7 Section 9.1 - C-FIND Service
 * @see DES-SVC-004 - Query SCP Design Specification
 */

#ifndef PACS_SERVICES_QUERY_SCP_HPP
#define PACS_SERVICES_QUERY_SCP_HPP

#include "scp_service.hpp"

#include <atomic>
#include <functional>
#include <optional>

namespace pacs::services {

// =============================================================================
// SOP Class UIDs
// =============================================================================

/// Patient Root Query/Retrieve Information Model - FIND
inline constexpr std::string_view patient_root_find_sop_class_uid =
    "1.2.840.10008.5.1.4.1.2.1.1";

/// Study Root Query/Retrieve Information Model - FIND
inline constexpr std::string_view study_root_find_sop_class_uid =
    "1.2.840.10008.5.1.4.1.2.2.1";

/// Patient/Study Only Query/Retrieve Information Model - FIND (Retired)
inline constexpr std::string_view patient_study_only_find_sop_class_uid =
    "1.2.840.10008.5.1.4.1.2.3.1";

/// Modality Worklist Information Model - FIND
inline constexpr std::string_view modality_worklist_find_sop_class_uid =
    "1.2.840.10008.5.1.4.31";

// =============================================================================
// Query Level
// =============================================================================

/**
 * @brief DICOM Query/Retrieve level enumeration
 *
 * Defines the hierarchical levels at which queries can be performed
 * in the DICOM Query/Retrieve Information Model.
 */
enum class query_level {
    patient,  ///< Patient level - query patient demographics
    study,    ///< Study level - query study information
    series,   ///< Series level - query series information
    image     ///< Image (Instance) level - query instance information
};

/**
 * @brief Convert query_level to string representation
 * @param level The query level to convert
 * @return DICOM string representation (PATIENT, STUDY, SERIES, IMAGE)
 */
[[nodiscard]] constexpr std::string_view to_string(query_level level) noexcept {
    switch (level) {
        case query_level::patient: return "PATIENT";
        case query_level::study: return "STUDY";
        case query_level::series: return "SERIES";
        case query_level::image: return "IMAGE";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Parse query level from DICOM string
 * @param level_str The DICOM string (PATIENT, STUDY, SERIES, IMAGE)
 * @return Parsed query level, or std::nullopt if invalid
 */
[[nodiscard]] inline std::optional<query_level> parse_query_level(
    std::string_view level_str) noexcept {
    if (level_str == "PATIENT") return query_level::patient;
    if (level_str == "STUDY") return query_level::study;
    if (level_str == "SERIES") return query_level::series;
    if (level_str == "IMAGE") return query_level::image;
    return std::nullopt;
}

// =============================================================================
// Query Handler Types
// =============================================================================

/**
 * @brief Query handler function type
 *
 * Called by query_scp to retrieve matching records for a C-FIND query.
 *
 * @param level The query level (Patient/Study/Series/Image)
 * @param query_keys The query dataset containing search criteria
 * @param calling_ae The calling AE title of the requesting SCU
 * @return Vector of matching datasets (empty if no matches)
 */
using query_handler = std::function<std::vector<core::dicom_dataset>(
    query_level level,
    const core::dicom_dataset& query_keys,
    const std::string& calling_ae)>;

/**
 * @brief Cancel check function type
 *
 * Called periodically during query processing to check if a C-CANCEL
 * request has been received.
 *
 * @return true if cancel has been requested
 */
using cancel_check = std::function<bool()>;

// =============================================================================
// Query SCP Class
// =============================================================================

/**
 * @brief Query SCP service for handling C-FIND requests
 *
 * The Query SCP (Service Class Provider) responds to C-FIND requests
 * from SCU (Service Class User) applications. It supports both Patient Root
 * and Study Root Query/Retrieve Information Models.
 *
 * ## C-FIND Message Flow
 *
 * ```
 * SCU                                    SCP (this class)
 *  │                                      │
 *  │  C-FIND-RQ                           │
 *  │  ┌──────────────────────────────────┐│
 *  │  │ QueryLevel: STUDY                ││
 *  │  │ PatientName: "DOE^J*"            ││
 *  │  │ StudyDate: "20250101-"           ││
 *  │  └──────────────────────────────────┘│
 *  │─────────────────────────────────────►│
 *  │                                      │
 *  │                          Query DB    │
 *  │                          (3 results) │
 *  │                                      │
 *  │  C-FIND-RSP (Pending)                │
 *  │  ┌──────────────────────────────────┐│
 *  │  │ Status: 0xFF00 (Pending)         ││
 *  │  │ PatientName: "DOE^JOHN"          ││
 *  │  │ StudyInstanceUID: 1.2.3...       ││
 *  │  └──────────────────────────────────┘│
 *  │◄─────────────────────────────────────│
 *  │                                      │
 *  │  ... (repeat for each result)        │
 *  │                                      │
 *  │  C-FIND-RSP (Success)                │
 *  │  ┌──────────────────────────────────┐│
 *  │  │ Status: 0x0000 (Success)         ││
 *  │  │ (No dataset)                     ││
 *  │  └──────────────────────────────────┘│
 *  │◄─────────────────────────────────────│
 * ```
 *
 * @example Usage
 * @code
 * query_scp scp;
 *
 * // Set up query handler
 * scp.set_handler([&database](query_level level, const auto& keys, const auto& ae) {
 *     return database.query(level, keys);
 * });
 *
 * // Optionally limit results
 * scp.set_max_results(1000);
 *
 * // Handle incoming C-FIND request
 * auto result = scp.handle_message(association, context_id, request);
 * @endcode
 */
class query_scp final : public scp_service {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct Query SCP with optional logger
     *
     * @param logger Logger instance for service logging (nullptr uses null_logger)
     */
    explicit query_scp(std::shared_ptr<di::ILogger> logger = nullptr);

    ~query_scp() override = default;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set the query handler function
     *
     * The handler is called for each C-FIND request to retrieve matching
     * records from the database.
     *
     * @param handler The query handler function
     */
    void set_handler(query_handler handler);

    /**
     * @brief Set maximum number of results to return
     *
     * @param max Maximum results (0 = unlimited)
     */
    void set_max_results(size_t max) noexcept;

    /**
     * @brief Get maximum number of results
     *
     * @return Maximum results (0 = unlimited)
     */
    [[nodiscard]] size_t max_results() const noexcept;

    /**
     * @brief Set the cancel check function
     *
     * The cancel check is called periodically during query processing
     * to check if a C-CANCEL has been received.
     *
     * @param check The cancel check function
     */
    void set_cancel_check(cancel_check check);

    // =========================================================================
    // scp_service Interface Implementation
    // =========================================================================

    /**
     * @brief Get supported SOP Class UIDs
     *
     * @return Vector containing Patient Root and Study Root Find SOP Classes
     */
    [[nodiscard]] std::vector<std::string> supported_sop_classes() const override;

    /**
     * @brief Handle an incoming DIMSE message (C-FIND-RQ)
     *
     * Processes the C-FIND request, queries the database through the handler,
     * and sends pending responses for each match followed by a final success.
     *
     * @param assoc The association on which the message was received
     * @param context_id The presentation context ID for the message
     * @param request The incoming C-FIND-RQ message
     * @return Success or error if the message is not a valid C-FIND-RQ
     */
    [[nodiscard]] network::Result<std::monostate> handle_message(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request) override;

    /**
     * @brief Get the service name
     *
     * @return "Query SCP"
     */
    [[nodiscard]] std::string_view service_name() const noexcept override;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total number of queries processed
     *
     * @return Number of C-FIND requests handled
     */
    [[nodiscard]] size_t queries_processed() const noexcept;

    /**
     * @brief Reset statistics counters
     */
    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /**
     * @brief Extract query level from request dataset
     *
     * @param dataset The query dataset
     * @return Parsed query level, or std::nullopt if invalid
     */
    [[nodiscard]] std::optional<query_level> extract_query_level(
        const core::dicom_dataset& dataset) const;

    /**
     * @brief Send a pending C-FIND response with matching dataset
     *
     * @param assoc The association
     * @param context_id The presentation context ID
     * @param message_id The original request message ID
     * @param sop_class_uid The SOP Class UID
     * @param result The matching dataset
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> send_pending_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        std::string_view sop_class_uid,
        const core::dicom_dataset& result);

    /**
     * @brief Send the final C-FIND response (success or cancel)
     *
     * @param assoc The association
     * @param context_id The presentation context ID
     * @param message_id The original request message ID
     * @param sop_class_uid The SOP Class UID
     * @param status The final status code
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> send_final_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        std::string_view sop_class_uid,
        network::dimse::status_code status);

    // =========================================================================
    // Member Variables
    // =========================================================================

    query_handler handler_;
    cancel_check cancel_check_;
    size_t max_results_{0};  // 0 = unlimited
    std::atomic<size_t> queries_processed_{0};
};

}  // namespace pacs::services

#endif  // PACS_SERVICES_QUERY_SCP_HPP
