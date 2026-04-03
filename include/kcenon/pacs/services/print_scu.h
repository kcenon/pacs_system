// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file print_scu.h
 * @brief DICOM Print Management SCU service (PS3.4 Annex H)
 *
 * This file provides the print_scu class for sending DICOM print requests
 * to remote Print SCP systems, including Film Session, Film Box, Image Box,
 * and Printer management operations.
 *
 * @see DICOM PS3.4 Annex H - Print Management Service Class
 * @see DICOM PS3.7 Section 10 - DIMSE-N Services
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_PRINT_SCU_HPP
#define PACS_SERVICES_PRINT_SCU_HPP

#include "print_scp.h"

#include "kcenon/pacs/core/dicom_dataset.h"
#include "kcenon/pacs/di/ilogger.h"
#include "kcenon/pacs/network/association.h"
#include "kcenon/pacs/network/dimse/dimse_message.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace kcenon::pacs::services {

// =============================================================================
// Print SCU Data Structures
// =============================================================================

/**
 * @brief Result of a Print DIMSE-N operation
 *
 * Contains outcome information from N-CREATE, N-SET, N-GET, N-ACTION,
 * or N-DELETE operations.
 */
struct print_result {
    /// SOP Instance UID (session, film box, image box, or printer)
    std::string sop_instance_uid;

    /// DIMSE status code (0x0000 = success)
    uint16_t status{0};

    /// Error comment from the SCP (if any)
    std::string error_comment;

    /// Time taken for the operation
    std::chrono::milliseconds elapsed{0};

    /// Response dataset (e.g., printer status, referenced image box UIDs)
    core::dicom_dataset response_data;

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
 * @brief Data for creating a Film Session via N-CREATE
 */
struct print_session_data {
    /// SOP Instance UID (generated if empty)
    std::string sop_instance_uid;

    /// Number of copies to print
    uint32_t number_of_copies{1};

    /// Print priority (HIGH, MED, LOW)
    std::string print_priority{"MED"};

    /// Medium type (PAPER, CLEAR FILM, BLUE FILM)
    std::string medium_type{"BLUE FILM"};

    /// Film destination (MAGAZINE, PROCESSOR)
    std::string film_destination{"MAGAZINE"};

    /// Film session label
    std::string film_session_label;
};

/**
 * @brief Data for creating a Film Box via N-CREATE
 */
struct print_film_box_data {
    /// Image display format (e.g., "STANDARD\\1,1")
    std::string image_display_format{"STANDARD\\1,1"};

    /// Film orientation (PORTRAIT, LANDSCAPE)
    std::string film_orientation{"PORTRAIT"};

    /// Film size ID (8INX10IN, 14INX17IN, etc.)
    std::string film_size_id{"8INX10IN"};

    /// Magnification type (REPLICATE, BILINEAR, CUBIC, NONE)
    std::string magnification_type;

    /// Parent film session SOP Instance UID
    std::string film_session_uid;
};

/**
 * @brief Data for setting an Image Box via N-SET
 */
struct print_image_data {
    /// Pixel data to set on the image box
    core::dicom_dataset pixel_data;

    /// Image position within the film box (1-based)
    uint16_t image_position{0};
};

/**
 * @brief Configuration for Print SCU service
 */
struct print_scu_config {
    /// Timeout for receiving DIMSE response
    std::chrono::milliseconds timeout{30000};

    /// Auto-generate SOP Instance UIDs if not provided
    bool auto_generate_uid{true};
};

// =============================================================================
// Print SCU Class
// =============================================================================

/**
 * @brief Print Management SCU service
 *
 * The Print SCU (Service Class User) sends DIMSE-N requests to remote
 * Print SCP systems to manage print sessions, film boxes, image boxes,
 * and printer status.
 *
 * ## Print Workflow
 *
 * ```
 * Workstation (Print SCU)                      Printer (Print SCP)
 *  |                                           |
 *  |  N-CREATE Film Session                    |
 *  |------------------------------------------>|
 *  |  N-CREATE-RSP (session UID)               |
 *  |<------------------------------------------|
 *  |                                           |
 *  |  N-CREATE Film Box                        |
 *  |------------------------------------------>|
 *  |  N-CREATE-RSP (box UID + image box UIDs)  |
 *  |<------------------------------------------|
 *  |                                           |
 *  |  N-SET Image Box (pixel data)             |
 *  |------------------------------------------>|
 *  |  N-SET-RSP (success)                      |
 *  |<------------------------------------------|
 *  |                                           |
 *  |  N-ACTION Film Box (Print)                |
 *  |------------------------------------------>|
 *  |  N-ACTION-RSP (success)                   |
 *  |<------------------------------------------|
 *  |                                           |
 *  |  N-DELETE Film Session                    |
 *  |------------------------------------------>|
 *  |  N-DELETE-RSP (success)                   |
 *  |<------------------------------------------|
 * ```
 *
 * @example Basic Usage
 * @code
 * // Establish association with Print SCP
 * association_config config;
 * config.calling_ae_title = "WORKSTATION";
 * config.called_ae_title = "PRINTER_SCP";
 * config.proposed_contexts.push_back({
 *     1,
 *     std::string(basic_grayscale_print_meta_sop_class_uid),
 *     {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
 * });
 *
 * auto assoc_result = association::connect("192.168.1.200", 10400, config);
 * auto& assoc = assoc_result.value();
 *
 * print_scu scu;
 *
 * // Create film session
 * print_session_data session_data;
 * session_data.number_of_copies = 1;
 * session_data.print_priority = "HIGH";
 *
 * auto session_result = scu.create_film_session(assoc, session_data);
 * auto session_uid = session_result.value().sop_instance_uid;
 *
 * // Create film box
 * print_film_box_data box_data;
 * box_data.image_display_format = "STANDARD\\1,1";
 * box_data.film_session_uid = session_uid;
 *
 * auto box_result = scu.create_film_box(assoc, box_data);
 *
 * // Set image box (pixel data)
 * print_image_data image_data;
 * image_data.pixel_data = my_image_dataset;
 * scu.set_image_box(assoc, image_box_uid, image_data);
 *
 * // Print and cleanup
 * auto film_box_uid = box_result.value().sop_instance_uid;
 * scu.print_film_box(assoc, film_box_uid);
 * scu.delete_film_session(assoc, session_uid);
 *
 * assoc.release();
 * @endcode
 */
class print_scu {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct Print SCU with default configuration
     *
     * @param logger Logger instance for service logging (nullptr uses null_logger)
     */
    explicit print_scu(std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Construct Print SCU with custom configuration
     *
     * @param config Configuration options
     * @param logger Logger instance for service logging (nullptr uses null_logger)
     */
    explicit print_scu(const print_scu_config& config,
                       std::shared_ptr<di::ILogger> logger = nullptr);

    ~print_scu() = default;

    // Non-copyable, non-movable (due to atomic members)
    print_scu(const print_scu&) = delete;
    print_scu& operator=(const print_scu&) = delete;
    print_scu(print_scu&&) = delete;
    print_scu& operator=(print_scu&&) = delete;

    // =========================================================================
    // Film Session Operations
    // =========================================================================

    /**
     * @brief Create a new Film Session (N-CREATE)
     *
     * @param assoc The established association to use
     * @param data Film session creation data
     * @return Result containing print_result on success
     */
    [[nodiscard]] network::Result<print_result> create_film_session(
        network::association& assoc,
        const print_session_data& data);

    /**
     * @brief Delete a Film Session (N-DELETE)
     *
     * Also deletes all associated Film Boxes and Image Boxes on the SCP side.
     *
     * @param assoc The established association to use
     * @param session_uid Film Session SOP Instance UID
     * @return Result containing print_result on success
     */
    [[nodiscard]] network::Result<print_result> delete_film_session(
        network::association& assoc,
        std::string_view session_uid);

    // =========================================================================
    // Film Box Operations
    // =========================================================================

    /**
     * @brief Create a new Film Box (N-CREATE)
     *
     * The SCP will auto-create Image Boxes based on the image display format.
     * Referenced Image Box UIDs are returned in the response dataset.
     *
     * @param assoc The established association to use
     * @param data Film box creation data
     * @return Result containing print_result (response_data has image box UIDs)
     */
    [[nodiscard]] network::Result<print_result> create_film_box(
        network::association& assoc,
        const print_film_box_data& data);

    /**
     * @brief Print a Film Box (N-ACTION)
     *
     * Sends N-ACTION to the SCP to initiate printing of the film box.
     *
     * @param assoc The established association to use
     * @param film_box_uid Film Box SOP Instance UID
     * @return Result containing print_result on success
     */
    [[nodiscard]] network::Result<print_result> print_film_box(
        network::association& assoc,
        std::string_view film_box_uid);

    /**
     * @brief Delete a Film Box (N-DELETE)
     *
     * @param assoc The established association to use
     * @param film_box_uid Film Box SOP Instance UID
     * @return Result containing print_result on success
     */
    [[nodiscard]] network::Result<print_result> delete_film_box(
        network::association& assoc,
        std::string_view film_box_uid);

    // =========================================================================
    // Image Box Operations
    // =========================================================================

    /**
     * @brief Set Image Box pixel data (N-SET)
     *
     * Sets the pixel data for an Image Box in the film box.
     * Use Basic Grayscale Image Box SOP Class by default.
     *
     * @param assoc The established association to use
     * @param image_box_uid Image Box SOP Instance UID
     * @param data Image data to set
     * @param use_color Use Basic Color Image Box instead of Grayscale
     * @return Result containing print_result on success
     */
    [[nodiscard]] network::Result<print_result> set_image_box(
        network::association& assoc,
        std::string_view image_box_uid,
        const print_image_data& data,
        bool use_color = false);

    // =========================================================================
    // Printer Status
    // =========================================================================

    /**
     * @brief Query printer status (N-GET)
     *
     * Sends N-GET to the Printer SOP Class to retrieve printer status.
     * The response_data in print_result contains the printer status dataset.
     *
     * @param assoc The established association to use
     * @return Result containing print_result with printer status in response_data
     */
    [[nodiscard]] network::Result<print_result> query_printer_status(
        network::association& assoc);

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
    // Private Implementation
    // =========================================================================

    /**
     * @brief Find an accepted presentation context for a print SOP class
     *
     * Tries the specific SOP class first, then falls back to Meta SOP classes.
     */
    [[nodiscard]] std::optional<uint8_t> find_print_context(
        network::association& assoc,
        std::string_view sop_class_uid) const;

    /**
     * @brief Generate a unique SOP Instance UID
     */
    [[nodiscard]] std::string generate_uid() const;

    /**
     * @brief Get the next message ID for DIMSE operations
     */
    [[nodiscard]] uint16_t next_message_id() noexcept;

    // =========================================================================
    // Private Members
    // =========================================================================

    /// Logger instance
    std::shared_ptr<di::ILogger> logger_;

    /// Configuration
    print_scu_config config_;

    /// Message ID counter
    std::atomic<uint16_t> message_id_counter_{1};

    /// Statistics
    std::atomic<size_t> sessions_created_{0};
    std::atomic<size_t> film_boxes_created_{0};
    std::atomic<size_t> images_set_{0};
    std::atomic<size_t> prints_executed_{0};
    std::atomic<size_t> printer_queries_{0};
};

}  // namespace kcenon::pacs::services

#endif  // PACS_SERVICES_PRINT_SCU_HPP
