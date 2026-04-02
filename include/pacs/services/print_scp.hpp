// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file print_scp.hpp
 * @brief DICOM Print Management SCP service (PS3.4 Annex H)
 *
 * This file provides the print_scp class for handling DICOM print requests
 * including Film Session, Film Box, Image Box, and Printer management.
 *
 * @see DICOM PS3.4 Annex H - Print Management Service Class
 * @see DICOM PS3.7 Section 10 - DIMSE-N Services
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_PRINT_SCP_HPP
#define PACS_SERVICES_PRINT_SCP_HPP

#include "scp_service.hpp"

#include <pacs/core/dicom_dataset.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace kcenon::pacs::services {

// =============================================================================
// Print Management SOP Class UIDs (PS3.4 Annex H)
// =============================================================================

/// Basic Film Session SOP Class UID
inline constexpr std::string_view basic_film_session_sop_class_uid =
    "1.2.840.10008.5.1.1.1";

/// Basic Film Box SOP Class UID
inline constexpr std::string_view basic_film_box_sop_class_uid =
    "1.2.840.10008.5.1.1.2";

/// Basic Grayscale Image Box SOP Class UID
inline constexpr std::string_view basic_grayscale_image_box_sop_class_uid =
    "1.2.840.10008.5.1.1.4";

/// Basic Color Image Box SOP Class UID
inline constexpr std::string_view basic_color_image_box_sop_class_uid =
    "1.2.840.10008.5.1.1.4.1";

/// Printer SOP Class UID
inline constexpr std::string_view printer_sop_class_uid =
    "1.2.840.10008.5.1.1.16";

/// Basic Grayscale Print Management Meta SOP Class UID
inline constexpr std::string_view basic_grayscale_print_meta_sop_class_uid =
    "1.2.840.10008.5.1.1.9";

/// Basic Color Print Management Meta SOP Class UID
inline constexpr std::string_view basic_color_print_meta_sop_class_uid =
    "1.2.840.10008.5.1.1.18";

// =============================================================================
// Print Management Types
// =============================================================================

/**
 * @brief Printer status enumeration (PS3.4 H.4.17)
 */
enum class printer_status {
    normal,    ///< Printer is operating normally
    warning,   ///< Printer has a non-critical issue
    failure    ///< Printer has a critical error
};

/**
 * @brief Convert printer_status to DICOM string representation
 */
[[nodiscard]] inline auto to_string(printer_status status) -> std::string_view {
    switch (status) {
        case printer_status::normal:
            return "NORMAL";
        case printer_status::warning:
            return "WARNING";
        case printer_status::failure:
            return "FAILURE";
        default:
            return "NORMAL";
    }
}

/**
 * @brief Print job status
 */
enum class print_job_status {
    pending,     ///< Job is queued
    printing,    ///< Job is being printed
    done,        ///< Job completed successfully
    failure      ///< Job failed
};

/**
 * @brief Film session data created by N-CREATE
 */
struct film_session {
    /// SOP Instance UID
    std::string sop_instance_uid;

    /// Number of copies
    uint32_t number_of_copies{1};

    /// Print priority (HIGH, MED, LOW)
    std::string print_priority{"MED"};

    /// Medium type (PAPER, CLEAR FILM, BLUE FILM)
    std::string medium_type{"BLUE FILM"};

    /// Film destination (MAGAZINE, PROCESSOR)
    std::string film_destination{"MAGAZINE"};

    /// Associated film box UIDs
    std::vector<std::string> film_box_uids;

    /// Complete dataset from request
    core::dicom_dataset data;
};

/**
 * @brief Film box data created by N-CREATE
 */
struct film_box {
    /// SOP Instance UID
    std::string sop_instance_uid;

    /// Parent film session UID
    std::string film_session_uid;

    /// Image display format (STANDARD\1,1 etc.)
    std::string image_display_format{"STANDARD\\1,1"};

    /// Film orientation (PORTRAIT, LANDSCAPE)
    std::string film_orientation{"PORTRAIT"};

    /// Film size ID (8INX10IN, 14INX17IN, etc.)
    std::string film_size_id{"8INX10IN"};

    /// Associated image box UIDs
    std::vector<std::string> image_box_uids;

    /// Complete dataset from request
    core::dicom_dataset data;
};

/**
 * @brief Image box data set by N-SET
 */
struct image_box {
    /// SOP Instance UID
    std::string sop_instance_uid;

    /// Parent film box UID
    std::string film_box_uid;

    /// Image position (1-based)
    uint16_t image_position{1};

    /// Whether pixel data has been set
    bool has_pixel_data{false};

    /// Complete dataset from request
    core::dicom_dataset data;
};

// =============================================================================
// Handler Types
// =============================================================================

/**
 * @brief Handler for film session creation
 *
 * @param session The film session data
 * @return Success or error
 */
using print_session_handler = std::function<network::Result<std::monostate>(
    const film_session& session)>;

/**
 * @brief Handler for print action (film box print)
 *
 * @param film_box_uid The film box UID to print
 * @return Success or error
 */
using print_action_handler = std::function<network::Result<std::monostate>(
    const std::string& film_box_uid)>;

/**
 * @brief Handler for printer status query
 *
 * @return Printer status and info dataset
 */
using printer_status_handler = std::function<
    network::Result<std::pair<printer_status, core::dicom_dataset>>()>;

// =============================================================================
// Print SCP Class
// =============================================================================

/**
 * @brief Print Management SCP service
 *
 * Handles DICOM print requests for Film Session, Film Box, Image Box,
 * and Printer SOP Classes.
 *
 * ## Print Workflow
 *
 * ```
 * SCU (Workstation)                         SCP (Print Server)
 *  |                                        |
 *  |  N-CREATE Film Session                 |
 *  |--------------------------------------->|
 *  |                                        |
 *  |  N-CREATE Film Box                     |
 *  |--------------------------------------->|
 *  |                                        |
 *  |  N-SET Image Box (pixel data)          |
 *  |--------------------------------------->|
 *  |                                        |
 *  |  N-ACTION Film Box (Print)             |
 *  |--------------------------------------->|
 *  |                                        |
 *  |  N-DELETE Film Session                 |
 *  |--------------------------------------->|
 * ```
 */
class print_scp final : public scp_service {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct Print SCP with optional logger
     *
     * @param logger Logger instance for service logging
     */
    explicit print_scp(std::shared_ptr<di::ILogger> logger = nullptr);

    ~print_scp() override = default;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set handler for film session creation
     */
    void set_session_handler(print_session_handler handler);

    /**
     * @brief Set handler for print action (film box print)
     */
    void set_print_handler(print_action_handler handler);

    /**
     * @brief Set handler for printer status query
     */
    void set_printer_status_handler(printer_status_handler handler);

    // =========================================================================
    // scp_service Interface Implementation
    // =========================================================================

    /**
     * @brief Get supported SOP Class UIDs
     *
     * @return Vector of Print Management SOP Class UIDs
     */
    [[nodiscard]] std::vector<std::string> supported_sop_classes() const override;

    /**
     * @brief Handle an incoming DIMSE-N message
     *
     * Routes N-CREATE, N-SET, N-GET, N-ACTION, N-DELETE to appropriate handlers
     * based on the affected SOP class.
     */
    [[nodiscard]] network::Result<std::monostate> handle_message(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request) override;

    /**
     * @brief Get the service name
     *
     * @return "Print SCP"
     */
    [[nodiscard]] std::string_view service_name() const noexcept override;

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] size_t sessions_created() const noexcept;
    [[nodiscard]] size_t film_boxes_created() const noexcept;
    [[nodiscard]] size_t images_set() const noexcept;
    [[nodiscard]] size_t prints_executed() const noexcept;
    [[nodiscard]] size_t printer_queries() const noexcept;
    void reset_statistics() noexcept;

private:
    // =========================================================================
    // DIMSE-N Command Handlers
    // =========================================================================

    [[nodiscard]] network::Result<std::monostate> handle_n_create(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    [[nodiscard]] network::Result<std::monostate> handle_n_set(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    [[nodiscard]] network::Result<std::monostate> handle_n_get(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    [[nodiscard]] network::Result<std::monostate> handle_n_action(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    [[nodiscard]] network::Result<std::monostate> handle_n_delete(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    // =========================================================================
    // SOP-specific Create Handlers
    // =========================================================================

    [[nodiscard]] network::Result<std::monostate> create_film_session(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    [[nodiscard]] network::Result<std::monostate> create_film_box(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    // =========================================================================
    // Response Helpers
    // =========================================================================

    [[nodiscard]] network::Result<std::monostate> send_response(
        network::association& assoc,
        uint8_t context_id,
        network::dimse::command_field response_type,
        uint16_t message_id,
        std::string_view sop_class_uid,
        const std::string& sop_instance_uid,
        network::dimse::status_code status);

    // =========================================================================
    // UID Generation
    // =========================================================================

    [[nodiscard]] auto generate_uid() -> std::string;

    // =========================================================================
    // Member Variables
    // =========================================================================

    print_session_handler session_handler_;
    print_action_handler print_handler_;
    printer_status_handler printer_status_handler_;

    /// Active film sessions indexed by SOP Instance UID
    std::unordered_map<std::string, film_session> sessions_;

    /// Active film boxes indexed by SOP Instance UID
    std::unordered_map<std::string, film_box> film_boxes_;

    /// Active image boxes indexed by SOP Instance UID
    std::unordered_map<std::string, image_box> image_boxes_;

    /// Mutex for state management
    mutable std::mutex mutex_;

    /// UID generation counter
    std::atomic<uint32_t> uid_counter_{0};

    /// Statistics
    std::atomic<size_t> sessions_created_{0};
    std::atomic<size_t> film_boxes_created_{0};
    std::atomic<size_t> images_set_{0};
    std::atomic<size_t> prints_executed_{0};
    std::atomic<size_t> printer_queries_{0};
};

// =============================================================================
// Print-specific DICOM Tags (PS3.4 Annex H)
// =============================================================================

namespace print_tags {

/// Number of Copies (2000,0010)
inline constexpr core::dicom_tag number_of_copies{0x2000, 0x0010};

/// Print Priority (2000,0020)
inline constexpr core::dicom_tag print_priority{0x2000, 0x0020};

/// Medium Type (2000,0030)
inline constexpr core::dicom_tag medium_type{0x2000, 0x0030};

/// Film Destination (2000,0040)
inline constexpr core::dicom_tag film_destination{0x2000, 0x0040};

/// Film Session Label (2000,0050)
inline constexpr core::dicom_tag film_session_label{0x2000, 0x0050};

/// Image Display Format (2010,0010)
inline constexpr core::dicom_tag image_display_format{0x2010, 0x0010};

/// Film Orientation (2010,0040)
inline constexpr core::dicom_tag film_orientation{0x2010, 0x0040};

/// Film Size ID (2010,0050)
inline constexpr core::dicom_tag film_size_id{0x2010, 0x0050};

/// Magnification Type (2010,0060)
inline constexpr core::dicom_tag magnification_type{0x2010, 0x0060};

/// Referenced Film Session Sequence (2010,0500)
inline constexpr core::dicom_tag referenced_film_session_sequence{0x2010, 0x0500};

/// Referenced Image Box Sequence (2010,0510)
inline constexpr core::dicom_tag referenced_image_box_sequence{0x2010, 0x0510};

/// Image Position (2020,0010)
inline constexpr core::dicom_tag image_position{0x2020, 0x0010};

/// Basic Grayscale Image Sequence (2020,0110)
inline constexpr core::dicom_tag basic_grayscale_image_sequence{0x2020, 0x0110};

/// Basic Color Image Sequence (2020,0111)
inline constexpr core::dicom_tag basic_color_image_sequence{0x2020, 0x0111};

/// Printer Status (2110,0010)
inline constexpr core::dicom_tag printer_status_tag{0x2110, 0x0010};

/// Printer Status Info (2110,0020)
inline constexpr core::dicom_tag printer_status_info{0x2110, 0x0020};

/// Printer Name (2110,0030)
inline constexpr core::dicom_tag printer_name{0x2110, 0x0030};

}  // namespace print_tags

}  // namespace kcenon::pacs::services

#endif  // PACS_SERVICES_PRINT_SCP_HPP
