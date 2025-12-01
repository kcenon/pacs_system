/**
 * @file storage_scu.hpp
 * @brief DICOM Storage SCU service (C-STORE sender)
 *
 * This file provides the storage_scu class for sending DICOM images via C-STORE.
 * The Storage SCU sends images to SCP applications (PACS servers, archives)
 * for permanent storage.
 *
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.7 Section 9.1.1 - C-STORE Service
 * @see DES-SVC-003 - Storage SCU Design Specification
 */

#ifndef PACS_SERVICES_STORAGE_SCU_HPP
#define PACS_SERVICES_STORAGE_SCU_HPP

#include "storage_status.hpp"
#include "pacs/core/dicom_dataset.hpp"
#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace pacs::services {

/**
 * @brief Result of a C-STORE operation
 *
 * Contains information about the outcome of storing a single DICOM instance.
 */
struct store_result {
    /// SOP Instance UID of the stored instance
    std::string sop_instance_uid;

    /// DIMSE status code (0x0000 = success)
    uint16_t status{0};

    /// Error comment from the SCP (if any)
    std::string error_comment;

    /// Check if the store operation was successful
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
 * @brief Progress callback type for batch operations
 *
 * @param completed Number of completed operations
 * @param total Total number of operations
 */
using store_progress_callback = std::function<void(size_t completed, size_t total)>;

/**
 * @brief Configuration for Storage SCU service
 */
struct storage_scu_config {
    /// Default priority for C-STORE requests (0=medium, 1=high, 2=low)
    uint16_t default_priority = 0;

    /// Timeout for receiving C-STORE response (milliseconds)
    std::chrono::milliseconds response_timeout{30000};

    /// Continue batch operation on error (true) or stop on first error (false)
    bool continue_on_error = true;
};

/**
 * @brief Storage SCU service for sending DICOM images via C-STORE
 *
 * The Storage SCU (Service Class User) sends DICOM images to remote PACS
 * servers, archives, or other storage systems via the C-STORE operation.
 *
 * ## C-STORE Message Flow
 *
 * ```
 * This Application (SCU)                PACS Server (SCP)
 *  |                                    |
 *  |  C-STORE-RQ                        |
 *  |  +------------------------------+  |
 *  |  | CommandField: 0x0001         |  |
 *  |  | AffectedSOPClassUID: CT Image|  |
 *  |  | AffectedSOPInstanceUID: ...  |  |
 *  |  | Priority: MEDIUM             |  |
 *  |  +------------------------------+  |
 *  |----------------------------------->|
 *  |                                    |
 *  |  Dataset (pixel data)              |
 *  |----------------------------------->|
 *  |                                    |
 *  |                         Validate   |
 *  |                         Store file |
 *  |                         Update index
 *  |                                    |
 *  |  C-STORE-RSP                       |
 *  |  +------------------------------+  |
 *  |  | Status: 0x0000 (Success)     |  |
 *  |  +------------------------------+  |
 *  |<-----------------------------------|
 *  |                                    |
 * ```
 *
 * @example Basic Usage
 * @code
 * // Establish association with presentation contexts for storage
 * association_config config;
 * config.calling_ae_title = "MY_SCU";
 * config.called_ae_title = "PACS_SCP";
 * config.proposed_contexts.push_back({
 *     1,
 *     ct_image_storage_uid,
 *     {"1.2.840.10008.1.2.1"}  // Explicit VR LE
 * });
 *
 * auto assoc_result = association::connect("192.168.1.100", 104, config);
 * if (assoc_result.is_err()) {
 *     // Handle connection error
 *     return;
 * }
 * auto& assoc = assoc_result.value();
 *
 * // Create SCU and send image
 * storage_scu scu;
 * auto result = scu.store(assoc, my_dataset);
 * if (result.is_ok() && result.value().is_success()) {
 *     // Image stored successfully
 * }
 *
 * assoc.release();
 * @endcode
 *
 * @example Batch Store with Progress
 * @code
 * storage_scu scu;
 * auto results = scu.store_batch(assoc, datasets,
 *     [](size_t done, size_t total) {
 *         std::cout << "Progress: " << done << "/" << total << "\n";
 *     });
 *
 * size_t succeeded = std::count_if(results.begin(), results.end(),
 *     [](const auto& r) { return r.is_success(); });
 * @endcode
 */
class storage_scu {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct a Storage SCU with default configuration
     */
    storage_scu();

    /**
     * @brief Construct a Storage SCU with custom configuration
     * @param config Configuration options
     */
    explicit storage_scu(const storage_scu_config& config);

    ~storage_scu() = default;

    // Non-copyable, non-movable (due to atomic members)
    storage_scu(const storage_scu&) = delete;
    storage_scu& operator=(const storage_scu&) = delete;
    storage_scu(storage_scu&&) = delete;
    storage_scu& operator=(storage_scu&&) = delete;

    // =========================================================================
    // Single Image Operations
    // =========================================================================

    /**
     * @brief Store a single DICOM dataset
     *
     * Sends the dataset via C-STORE to the remote SCP. The dataset must
     * contain valid SOP Class UID and SOP Instance UID attributes.
     *
     * @param assoc The established association to use
     * @param dataset The DICOM dataset to store
     * @return Result containing store_result on success, or error message
     */
    [[nodiscard]] network::Result<store_result> store(
        network::association& assoc,
        const core::dicom_dataset& dataset);

    // =========================================================================
    // Batch Operations
    // =========================================================================

    /**
     * @brief Store multiple DICOM datasets
     *
     * Sends multiple datasets via C-STORE operations. If continue_on_error
     * is true (default), continues with remaining datasets after failures.
     *
     * @param assoc The established association to use
     * @param datasets Vector of datasets to store
     * @param progress_callback Optional callback for progress updates
     * @return Vector of store_result for each dataset
     */
    [[nodiscard]] std::vector<store_result> store_batch(
        network::association& assoc,
        const std::vector<core::dicom_dataset>& datasets,
        store_progress_callback progress_callback = nullptr);

    // =========================================================================
    // File-based Operations
    // =========================================================================

    /**
     * @brief Store a DICOM file
     *
     * Reads and parses a DICOM file, then sends it via C-STORE.
     *
     * @param assoc The established association to use
     * @param file_path Path to the DICOM file
     * @return Result containing store_result on success, or error message
     */
    [[nodiscard]] network::Result<store_result> store_file(
        network::association& assoc,
        const std::filesystem::path& file_path);

    /**
     * @brief Store all DICOM files in a directory
     *
     * Scans a directory for DICOM files and stores them via C-STORE.
     *
     * @param assoc The established association to use
     * @param directory Path to the directory
     * @param recursive If true, scan subdirectories recursively
     * @param progress_callback Optional callback for progress updates
     * @return Vector of store_result for each file
     */
    [[nodiscard]] std::vector<store_result> store_directory(
        network::association& assoc,
        const std::filesystem::path& directory,
        bool recursive = true,
        store_progress_callback progress_callback = nullptr);

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get the number of images sent since construction
     * @return Count of successfully sent images
     */
    [[nodiscard]] size_t images_sent() const noexcept;

    /**
     * @brief Get the number of failed store operations since construction
     * @return Count of failed operations
     */
    [[nodiscard]] size_t failures() const noexcept;

    /**
     * @brief Get the total bytes sent since construction
     * @return Total bytes of dataset data sent
     */
    [[nodiscard]] size_t bytes_sent() const noexcept;

    /**
     * @brief Reset statistics counters to zero
     */
    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /**
     * @brief Internal implementation of single store operation
     */
    [[nodiscard]] network::Result<store_result> store_impl(
        network::association& assoc,
        const core::dicom_dataset& dataset,
        uint16_t message_id);

    /**
     * @brief Get the next message ID for DIMSE operations
     */
    [[nodiscard]] uint16_t next_message_id() noexcept;

    /**
     * @brief Collect DICOM files from a directory
     */
    [[nodiscard]] std::vector<std::filesystem::path> collect_dicom_files(
        const std::filesystem::path& directory,
        bool recursive) const;

    // =========================================================================
    // Private Members
    // =========================================================================

    /// Configuration
    storage_scu_config config_;

    /// Message ID counter
    std::atomic<uint16_t> message_id_counter_{1};

    /// Statistics: number of images sent successfully
    std::atomic<size_t> images_sent_{0};

    /// Statistics: number of failed operations
    std::atomic<size_t> failures_{0};

    /// Statistics: total bytes sent
    std::atomic<size_t> bytes_sent_{0};
};

}  // namespace pacs::services

#endif  // PACS_SERVICES_STORAGE_SCU_HPP
