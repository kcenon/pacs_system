/**
 * @file retrieve_scu.hpp
 * @brief DICOM Retrieve SCU service (C-MOVE/C-GET sender)
 *
 * This file provides the retrieve_scu class for performing DICOM C-MOVE and C-GET
 * operations to retrieve images from remote PACS servers. It supports both Patient
 * Root and Study Root Query/Retrieve Information Models with progress tracking.
 *
 * @see DICOM PS3.4 Section C.4.2 - C-MOVE Operation
 * @see DICOM PS3.4 Section C.4.3 - C-GET Operation
 * @see DICOM PS3.7 Section 9.1.3 - C-MOVE Service
 * @see DICOM PS3.7 Section 9.1.4 - C-GET Service
 * @see Issue #532 - Implement retrieve_scu Library (C-MOVE/C-GET SCU)
 */

#ifndef PACS_SERVICES_RETRIEVE_SCU_HPP
#define PACS_SERVICES_RETRIEVE_SCU_HPP

#include "query_scu.hpp"
#include "retrieve_scp.hpp"
#include "pacs/core/dicom_dataset.hpp"
#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/di/ilogger.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace pacs::services {

// =============================================================================
// Retrieve Mode Enumeration
// =============================================================================

/**
 * @brief DICOM Retrieve Mode (C-MOVE vs C-GET)
 *
 * Defines which retrieve operation to use.
 */
enum class retrieve_mode {
    c_move,  ///< Request SCP to send to third party (requires move destination)
    c_get    ///< Receive directly from SCP on same association
};

/**
 * @brief Convert retrieve_mode to string representation
 * @param mode The retrieve mode to convert
 * @return String representation
 */
[[nodiscard]] constexpr std::string_view to_string(retrieve_mode mode) noexcept {
    switch (mode) {
        case retrieve_mode::c_move: return "C-MOVE";
        case retrieve_mode::c_get: return "C-GET";
        default: return "Unknown";
    }
}

// =============================================================================
// Retrieve Progress Structure
// =============================================================================

/**
 * @brief Progress information for a retrieve operation
 *
 * Tracks the progress of sub-operations during C-MOVE or C-GET.
 */
struct retrieve_progress {
    uint16_t remaining{0};   ///< Number of remaining sub-operations
    uint16_t completed{0};   ///< Number of completed sub-operations
    uint16_t failed{0};      ///< Number of failed sub-operations
    uint16_t warning{0};     ///< Number of sub-operations with warnings
    std::chrono::steady_clock::time_point start_time;

    /**
     * @brief Get total number of sub-operations
     * @return Total count (remaining + completed + failed + warning)
     */
    [[nodiscard]] uint16_t total() const noexcept {
        return remaining + completed + failed + warning;
    }

    /**
     * @brief Get completion percentage
     * @return Percentage complete (0.0 to 100.0)
     */
    [[nodiscard]] float percent() const noexcept {
        uint16_t t = total();
        if (t == 0) return 0.0f;
        return (static_cast<float>(completed + failed + warning) / t) * 100.0f;
    }

    /**
     * @brief Get elapsed time since start
     * @return Elapsed time in milliseconds
     */
    [[nodiscard]] std::chrono::milliseconds elapsed() const noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
    }
};

// =============================================================================
// Retrieve Result Structure
// =============================================================================

/**
 * @brief Result of a retrieve operation (C-MOVE or C-GET)
 *
 * Contains completion statistics and metadata about the retrieve execution.
 */
struct retrieve_result {
    /// Number of successfully completed sub-operations
    uint16_t completed{0};

    /// Number of failed sub-operations
    uint16_t failed{0};

    /// Number of sub-operations with warnings
    uint16_t warning{0};

    /// Final DIMSE status code
    uint16_t final_status{0};

    /// Retrieve execution time
    std::chrono::milliseconds elapsed{0};

    /// Received instances (for C-GET mode only)
    std::vector<core::dicom_dataset> received_instances;

    /**
     * @brief Check if the retrieve was fully successful
     * @return true if all sub-operations succeeded
     */
    [[nodiscard]] bool is_success() const noexcept {
        return final_status == 0x0000 && failed == 0;
    }

    /**
     * @brief Check if the retrieve was cancelled
     * @return true if the operation was cancelled
     */
    [[nodiscard]] bool is_cancelled() const noexcept {
        return final_status == 0xFE00;
    }

    /**
     * @brief Check if any sub-operations failed
     * @return true if at least one sub-operation failed
     */
    [[nodiscard]] bool has_failures() const noexcept {
        return failed > 0;
    }

    /**
     * @brief Check if any sub-operations had warnings
     * @return true if at least one sub-operation had warnings
     */
    [[nodiscard]] bool has_warnings() const noexcept {
        return warning > 0;
    }
};

// =============================================================================
// Progress Callback Type
// =============================================================================

/**
 * @brief Callback type for retrieve progress updates
 *
 * Called periodically during retrieve operations with current progress.
 *
 * @param progress Current progress information
 */
using retrieve_progress_callback = std::function<void(const retrieve_progress&)>;

// =============================================================================
// Instance Receive Callback Type (for C-GET)
// =============================================================================

/**
 * @brief Callback type for receiving instances during C-GET
 *
 * Called for each C-STORE sub-operation received during C-GET.
 *
 * @param dataset The received DICOM dataset
 * @return true to continue receiving, false to cancel
 */
using instance_receive_callback = std::function<bool(const core::dicom_dataset&)>;

// =============================================================================
// Retrieve SCU Configuration
// =============================================================================

/**
 * @brief Configuration for Retrieve SCU service
 */
struct retrieve_scu_config {
    /// Retrieve mode (C-MOVE or C-GET)
    retrieve_mode mode{retrieve_mode::c_move};

    /// Query information model (Patient Root or Study Root)
    query_model model{query_model::study_root};

    /// Query level (Study, Series, or Image)
    query_level level{query_level::study};

    /// Move destination AE title (required for C-MOVE mode)
    std::string move_destination;

    /// Timeout for receiving responses (milliseconds)
    std::chrono::milliseconds timeout{120000};

    /// Priority for DIMSE operations (0=medium, 1=high, 2=low)
    uint16_t priority{0};
};

// =============================================================================
// Retrieve SCU Class
// =============================================================================

/**
 * @brief Retrieve SCU service for C-MOVE and C-GET operations
 *
 * The Retrieve SCU (Service Class User) sends C-MOVE or C-GET requests to
 * remote PACS servers to retrieve DICOM images. It supports both Patient Root
 * and Study Root Query/Retrieve Information Models with progress tracking.
 *
 * ## C-MOVE vs C-GET
 *
 * **C-MOVE**:
 * - Requests the SCP to send images to a specified destination AE
 * - The destination can be self or a third party
 * - More commonly supported by PACS servers
 * - Requires separate storage SCP at the destination
 *
 * **C-GET**:
 * - Receives images directly on the same association
 * - No need for separate storage SCP
 * - Firewall-friendly (single connection)
 * - Less commonly supported by PACS servers
 *
 * ## C-MOVE Message Flow
 *
 * ```
 * This Application (SCU)                PACS Server (SCP)
 *  |                                    |
 *  |  C-MOVE-RQ                         |
 *  |  +------------------------------+  |
 *  |  | MoveDestination: WORKSTATION |  |
 *  |  | QueryRetrieveLevel: STUDY    |  |
 *  |  | StudyInstanceUID: 1.2.3...   |  |
 *  |  +------------------------------+  |
 *  |----------------------------------->|
 *  |                                    |
 *  |                         Find Study |
 *  |                         50 images  |
 *  |                                    |
 *  |  C-MOVE-RSP (Pending)              |
 *  |  +------------------------------+  |
 *  |  | Status: 0xFF00 (Pending)     |  |
 *  |  | Remaining: 50                |  |
 *  |  | Completed: 0                 |  |
 *  |  +------------------------------+  |
 *  |<-----------------------------------|
 *  |                                    |
 *  |  ... (SCP sends to WORKSTATION)    |
 *  |                                    |
 *  |  C-MOVE-RSP (Success)              |
 *  |  +------------------------------+  |
 *  |  | Status: 0x0000 (Success)     |  |
 *  |  | Completed: 50                |  |
 *  |  | Failed: 0                    |  |
 *  |  +------------------------------+  |
 *  |<-----------------------------------|
 * ```
 *
 * @example Basic C-MOVE Usage
 * @code
 * // Establish association
 * association_config config;
 * config.calling_ae_title = "MY_SCU";
 * config.called_ae_title = "PACS_SCP";
 * config.proposed_contexts.push_back({
 *     1,
 *     std::string(study_root_move_sop_class_uid),
 *     {"1.2.840.10008.1.2.1"}
 * });
 *
 * auto assoc_result = association::connect("192.168.1.100", 104, config);
 * auto& assoc = assoc_result.value();
 *
 * // Create SCU and retrieve
 * retrieve_scu scu;
 * scu.set_move_destination("WORKSTATION");
 *
 * auto result = scu.retrieve_study(assoc, "1.2.840.113619.2.1.1.322");
 * if (result.is_ok() && result.value().is_success()) {
 *     std::cout << "Retrieved " << result.value().completed << " images\n";
 * }
 *
 * assoc.release();
 * @endcode
 *
 * @example C-MOVE with Progress Tracking
 * @code
 * retrieve_scu scu;
 * scu.set_move_destination("WORKSTATION");
 *
 * core::dicom_dataset query;
 * query.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");
 * query.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3...");
 *
 * auto result = scu.move(assoc, query, "WORKSTATION",
 *     [](const retrieve_progress& p) {
 *         std::cout << "Progress: " << p.percent() << "% "
 *                   << "(" << p.completed << "/" << p.total() << ")\n";
 *     });
 * @endcode
 *
 * @example C-GET with Instance Callback
 * @code
 * retrieve_scu_config cfg;
 * cfg.mode = retrieve_mode::c_get;
 * retrieve_scu scu(cfg);
 *
 * core::dicom_dataset query;
 * query.set_string(tags::query_retrieve_level, vr_type::CS, "SERIES");
 * query.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3...");
 *
 * auto result = scu.get(assoc, query,
 *     [](const retrieve_progress& p) {
 *         // Progress callback
 *     });
 *
 * if (result.is_ok()) {
 *     for (const auto& ds : result.value().received_instances) {
 *         // Process each received dataset
 *     }
 * }
 * @endcode
 */
class retrieve_scu {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct a Retrieve SCU with default configuration
     *
     * @param logger Logger instance for service logging (nullptr uses null_logger)
     */
    explicit retrieve_scu(std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Construct a Retrieve SCU with custom configuration
     *
     * @param config Configuration options
     * @param logger Logger instance for service logging (nullptr uses null_logger)
     */
    explicit retrieve_scu(const retrieve_scu_config& config,
                          std::shared_ptr<di::ILogger> logger = nullptr);

    ~retrieve_scu() = default;

    // Non-copyable, non-movable (due to atomic members)
    retrieve_scu(const retrieve_scu&) = delete;
    retrieve_scu& operator=(const retrieve_scu&) = delete;
    retrieve_scu(retrieve_scu&&) = delete;
    retrieve_scu& operator=(retrieve_scu&&) = delete;

    // =========================================================================
    // C-MOVE Operations
    // =========================================================================

    /**
     * @brief Perform a C-MOVE operation with raw dataset
     *
     * Sends a C-MOVE request with the provided query keys to retrieve
     * matching images. The SCP will send the images to the specified
     * destination AE title.
     *
     * @param assoc The established association to use
     * @param query_keys The DICOM dataset containing query keys
     * @param destination_ae The destination AE title to send images to
     * @param progress Optional callback for progress updates
     * @return Result containing retrieve_result on success, or error message
     */
    [[nodiscard]] network::Result<retrieve_result> move(
        network::association& assoc,
        const core::dicom_dataset& query_keys,
        std::string_view destination_ae,
        retrieve_progress_callback progress = nullptr);

    // =========================================================================
    // C-GET Operations
    // =========================================================================

    /**
     * @brief Perform a C-GET operation with raw dataset
     *
     * Sends a C-GET request with the provided query keys to retrieve
     * matching images. The images are received directly on the same
     * association via C-STORE sub-operations.
     *
     * @param assoc The established association to use
     * @param query_keys The DICOM dataset containing query keys
     * @param progress Optional callback for progress updates
     * @return Result containing retrieve_result on success, or error message
     */
    [[nodiscard]] network::Result<retrieve_result> get(
        network::association& assoc,
        const core::dicom_dataset& query_keys,
        retrieve_progress_callback progress = nullptr);

    // =========================================================================
    // Convenience Methods
    // =========================================================================

    /**
     * @brief Retrieve a study by Study Instance UID
     *
     * Uses C-MOVE or C-GET based on current configuration.
     *
     * @param assoc The established association to use
     * @param study_uid The Study Instance UID to retrieve
     * @param progress Optional callback for progress updates
     * @return Result containing retrieve_result on success
     */
    [[nodiscard]] network::Result<retrieve_result> retrieve_study(
        network::association& assoc,
        std::string_view study_uid,
        retrieve_progress_callback progress = nullptr);

    /**
     * @brief Retrieve a series by Series Instance UID
     *
     * Uses C-MOVE or C-GET based on current configuration.
     *
     * @param assoc The established association to use
     * @param series_uid The Series Instance UID to retrieve
     * @param progress Optional callback for progress updates
     * @return Result containing retrieve_result on success
     */
    [[nodiscard]] network::Result<retrieve_result> retrieve_series(
        network::association& assoc,
        std::string_view series_uid,
        retrieve_progress_callback progress = nullptr);

    /**
     * @brief Retrieve a single instance by SOP Instance UID
     *
     * Uses C-MOVE or C-GET based on current configuration.
     *
     * @param assoc The established association to use
     * @param sop_instance_uid The SOP Instance UID to retrieve
     * @param progress Optional callback for progress updates
     * @return Result containing retrieve_result on success
     */
    [[nodiscard]] network::Result<retrieve_result> retrieve_instance(
        network::association& assoc,
        std::string_view sop_instance_uid,
        retrieve_progress_callback progress = nullptr);

    // =========================================================================
    // C-CANCEL Support
    // =========================================================================

    /**
     * @brief Send a C-CANCEL request to stop an ongoing retrieve
     *
     * @param assoc The association on which the retrieve is running
     * @param message_id The message ID of the retrieve to cancel
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> cancel(
        network::association& assoc,
        uint16_t message_id);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Update the SCU configuration
     *
     * @param config New configuration options
     */
    void set_config(const retrieve_scu_config& config);

    /**
     * @brief Set the move destination AE title
     *
     * @param ae_title The destination AE title for C-MOVE operations
     */
    void set_move_destination(std::string_view ae_title);

    /**
     * @brief Get the current configuration
     *
     * @return Reference to the current configuration
     */
    [[nodiscard]] const retrieve_scu_config& config() const noexcept;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get the number of retrieves performed since construction
     * @return Count of C-MOVE/C-GET requests sent
     */
    [[nodiscard]] size_t retrieves_performed() const noexcept;

    /**
     * @brief Get the total number of instances retrieved since construction
     * @return Total count of successful sub-operations
     */
    [[nodiscard]] size_t instances_retrieved() const noexcept;

    /**
     * @brief Get the total bytes retrieved since construction (C-GET only)
     * @return Total bytes of dataset data received
     */
    [[nodiscard]] size_t bytes_retrieved() const noexcept;

    /**
     * @brief Reset statistics counters to zero
     */
    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /**
     * @brief Internal C-MOVE implementation
     */
    [[nodiscard]] network::Result<retrieve_result> perform_move(
        network::association& assoc,
        const core::dicom_dataset& query_keys,
        std::string_view destination_ae,
        retrieve_progress_callback progress);

    /**
     * @brief Internal C-GET implementation
     */
    [[nodiscard]] network::Result<retrieve_result> perform_get(
        network::association& assoc,
        const core::dicom_dataset& query_keys,
        retrieve_progress_callback progress);

    /**
     * @brief Get the next message ID for DIMSE operations
     */
    [[nodiscard]] uint16_t next_message_id() noexcept;

    /**
     * @brief Get MOVE SOP Class UID based on current configuration
     */
    [[nodiscard]] std::string_view get_move_sop_class_uid() const noexcept;

    /**
     * @brief Get GET SOP Class UID based on current configuration
     */
    [[nodiscard]] std::string_view get_get_sop_class_uid() const noexcept;

    /**
     * @brief Build query dataset for study retrieval
     */
    [[nodiscard]] core::dicom_dataset build_study_query(
        std::string_view study_uid) const;

    /**
     * @brief Build query dataset for series retrieval
     */
    [[nodiscard]] core::dicom_dataset build_series_query(
        std::string_view series_uid) const;

    /**
     * @brief Build query dataset for instance retrieval
     */
    [[nodiscard]] core::dicom_dataset build_instance_query(
        std::string_view sop_instance_uid) const;

    // =========================================================================
    // Private Members
    // =========================================================================

    /// Logger instance for service logging
    std::shared_ptr<di::ILogger> logger_;

    /// Configuration
    retrieve_scu_config config_;

    /// Message ID counter
    std::atomic<uint16_t> message_id_counter_{1};

    /// Statistics: number of retrieves performed
    std::atomic<size_t> retrieves_performed_{0};

    /// Statistics: total number of instances retrieved
    std::atomic<size_t> instances_retrieved_{0};

    /// Statistics: total bytes retrieved
    std::atomic<size_t> bytes_retrieved_{0};
};

}  // namespace pacs::services

#endif  // PACS_SERVICES_RETRIEVE_SCU_HPP
