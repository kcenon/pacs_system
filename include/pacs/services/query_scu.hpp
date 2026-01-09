/**
 * @file query_scu.hpp
 * @brief DICOM Query SCU service (C-FIND sender)
 *
 * This file provides the query_scu class for performing DICOM C-FIND queries
 * to remote PACS servers. It supports Patient Root and Study Root Query models
 * at Patient, Study, Series, and Image levels.
 *
 * @see DICOM PS3.4 Section C.4 - Query/Retrieve Service Class
 * @see DICOM PS3.7 Section 9.1.2 - C-FIND Service
 * @see Issue #531 - Implement query_scu Library (C-FIND SCU)
 */

#ifndef PACS_SERVICES_QUERY_SCU_HPP
#define PACS_SERVICES_QUERY_SCU_HPP

#include "query_scp.hpp"
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
// Query Model Enumeration
// =============================================================================

/**
 * @brief DICOM Query/Retrieve Information Model
 *
 * Defines which information model to use for queries.
 */
enum class query_model {
    patient_root,  ///< Patient Root Query/Retrieve Information Model
    study_root     ///< Study Root Query/Retrieve Information Model
};

/**
 * @brief Convert query_model to string representation
 * @param model The query model to convert
 * @return String representation
 */
[[nodiscard]] constexpr std::string_view to_string(query_model model) noexcept {
    switch (model) {
        case query_model::patient_root: return "Patient Root";
        case query_model::study_root: return "Study Root";
        default: return "Unknown";
    }
}

/**
 * @brief Get the FIND SOP Class UID for a query model
 * @param model The query model
 * @return The corresponding SOP Class UID
 */
[[nodiscard]] constexpr std::string_view get_find_sop_class_uid(
    query_model model) noexcept {
    switch (model) {
        case query_model::patient_root:
            return patient_root_find_sop_class_uid;
        case query_model::study_root:
            return study_root_find_sop_class_uid;
        default:
            return patient_root_find_sop_class_uid;
    }
}

// =============================================================================
// Query Result Structure
// =============================================================================

/**
 * @brief Result of a C-FIND query operation
 *
 * Contains all matching datasets and metadata about the query execution.
 */
struct query_result {
    /// Matching datasets returned by the SCP
    std::vector<core::dicom_dataset> matches;

    /// Final DIMSE status code (0x0000 = success)
    uint16_t status{0};

    /// Query execution time
    std::chrono::milliseconds elapsed{0};

    /// Number of pending responses received (may differ from matches.size()
    /// if max_results was enforced)
    size_t total_pending{0};

    /// Check if the query was successful
    [[nodiscard]] bool is_success() const noexcept {
        return status == 0x0000;
    }

    /// Check if the query was cancelled
    [[nodiscard]] bool is_cancelled() const noexcept {
        return status == 0xFE00;
    }
};

// =============================================================================
// Typed Query Key Structures
// =============================================================================

/**
 * @brief Query keys for PATIENT level queries
 */
struct patient_query_keys {
    std::string patient_name;        ///< Patient's Name (0010,0010)
    std::string patient_id;          ///< Patient ID (0010,0020)
    std::string birth_date;          ///< Patient's Birth Date (0010,0030)
    std::string sex;                 ///< Patient's Sex (0010,0040)
};

/**
 * @brief Query keys for STUDY level queries
 */
struct study_query_keys {
    std::string patient_id;          ///< Patient ID (0010,0020) - for filtering
    std::string study_uid;           ///< Study Instance UID (0020,000D)
    std::string study_date;          ///< Study Date (0008,0020) - YYYYMMDD or range
    std::string accession_number;    ///< Accession Number (0008,0050)
    std::string modality;            ///< Modalities in Study (0008,0061)
    std::string study_description;   ///< Study Description (0008,1030)
};

/**
 * @brief Query keys for SERIES level queries
 */
struct series_query_keys {
    std::string study_uid;           ///< Study Instance UID (0020,000D) - Required
    std::string series_uid;          ///< Series Instance UID (0020,000E)
    std::string modality;            ///< Modality (0008,0060)
    std::string series_number;       ///< Series Number (0020,0011)
};

/**
 * @brief Query keys for IMAGE (Instance) level queries
 */
struct instance_query_keys {
    std::string series_uid;          ///< Series Instance UID (0020,000E) - Required
    std::string sop_instance_uid;    ///< SOP Instance UID (0008,0018)
    std::string instance_number;     ///< Instance Number (0020,0013)
};

// =============================================================================
// Query SCU Configuration
// =============================================================================

/**
 * @brief Configuration for Query SCU service
 */
struct query_scu_config {
    /// Query information model (Patient Root or Study Root)
    query_model model{query_model::study_root};

    /// Query level (Patient, Study, Series, or Image)
    query_level level{query_level::study};

    /// Timeout for receiving query responses (milliseconds)
    std::chrono::milliseconds timeout{30000};

    /// Maximum number of results to return (0 = unlimited)
    size_t max_results{0};

    /// Send C-CANCEL when max_results is reached
    bool cancel_on_max{true};
};

// =============================================================================
// Streaming Callback Types
// =============================================================================

/**
 * @brief Callback type for streaming query results
 *
 * Called for each pending response received from the SCP.
 *
 * @param dataset The matching dataset from the SCP
 * @return true to continue receiving, false to cancel the query
 */
using query_streaming_callback = std::function<bool(const core::dicom_dataset&)>;

// =============================================================================
// Query SCU Class
// =============================================================================

/**
 * @brief Query SCU service for performing DICOM C-FIND queries
 *
 * The Query SCU (Service Class User) sends C-FIND requests to remote PACS
 * servers to query for patient, study, series, or instance information.
 *
 * ## C-FIND Message Flow
 *
 * ```
 * This Application (SCU)                PACS Server (SCP)
 *  |                                    |
 *  |  C-FIND-RQ                         |
 *  |  +------------------------------+  |
 *  |  | QueryRetrieveLevel: STUDY    |  |
 *  |  | PatientName: "DOE^J*"        |  |
 *  |  | StudyDate: "20240101-"       |  |
 *  |  +------------------------------+  |
 *  |----------------------------------->|
 *  |                                    |
 *  |                         Query DB   |
 *  |                         (3 results)|
 *  |                                    |
 *  |  C-FIND-RSP (Pending)              |
 *  |  +------------------------------+  |
 *  |  | Status: 0xFF00 (Pending)     |  |
 *  |  | PatientName: "DOE^JOHN"      |  |
 *  |  | StudyInstanceUID: 1.2.3...   |  |
 *  |  +------------------------------+  |
 *  |<-----------------------------------|
 *  |                                    |
 *  |  ... (repeat for each result)      |
 *  |                                    |
 *  |  C-FIND-RSP (Success)              |
 *  |  +------------------------------+  |
 *  |  | Status: 0x0000 (Success)     |  |
 *  |  +------------------------------+  |
 *  |<-----------------------------------|
 * ```
 *
 * @example Basic Usage
 * @code
 * // Establish association with presentation contexts for query
 * association_config config;
 * config.calling_ae_title = "MY_SCU";
 * config.called_ae_title = "PACS_SCP";
 * config.proposed_contexts.push_back({
 *     1,
 *     std::string(study_root_find_sop_class_uid),
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
 * // Create SCU and query
 * query_scu scu;
 * study_query_keys keys;
 * keys.patient_id = "12345";
 * keys.study_date = "20240101-20241231";
 *
 * auto result = scu.find_studies(assoc, keys);
 * if (result.is_ok() && result.value().is_success()) {
 *     for (const auto& ds : result.value().matches) {
 *         auto study_uid = ds.get_string(tags::study_instance_uid);
 *         // Process study...
 *     }
 * }
 *
 * assoc.release();
 * @endcode
 *
 * @example Streaming Query for Large Results
 * @code
 * query_scu scu;
 * core::dicom_dataset query;
 * query.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");
 * query.set_string(tags::patient_name, vr_type::PN, "");  // Return all
 *
 * size_t count = 0;
 * auto result = scu.find_streaming(assoc, query,
 *     [&count](const core::dicom_dataset& ds) {
 *         ++count;
 *         // Process each result as it arrives
 *         return count < 1000;  // Stop after 1000 results
 *     });
 * @endcode
 */
class query_scu {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct a Query SCU with default configuration
     *
     * @param logger Logger instance for service logging (nullptr uses null_logger)
     */
    explicit query_scu(std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Construct a Query SCU with custom configuration
     *
     * @param config Configuration options
     * @param logger Logger instance for service logging (nullptr uses null_logger)
     */
    explicit query_scu(const query_scu_config& config,
                       std::shared_ptr<di::ILogger> logger = nullptr);

    ~query_scu() = default;

    // Non-copyable, non-movable (due to atomic members)
    query_scu(const query_scu&) = delete;
    query_scu& operator=(const query_scu&) = delete;
    query_scu(query_scu&&) = delete;
    query_scu& operator=(query_scu&&) = delete;

    // =========================================================================
    // Generic Query Operations
    // =========================================================================

    /**
     * @brief Perform a C-FIND query with raw dataset
     *
     * Sends a C-FIND request with the provided query keys and collects
     * all matching datasets from the SCP.
     *
     * @param assoc The established association to use
     * @param query_keys The DICOM dataset containing query keys
     * @return Result containing query_result on success, or error message
     */
    [[nodiscard]] network::Result<query_result> find(
        network::association& assoc,
        const core::dicom_dataset& query_keys);

    /**
     * @brief Perform a streaming C-FIND query for large result sets
     *
     * Sends a C-FIND request and calls the callback for each pending
     * response. This is more memory-efficient for large result sets.
     *
     * @param assoc The established association to use
     * @param query_keys The DICOM dataset containing query keys
     * @param callback Called for each matching dataset; return false to cancel
     * @return Result containing the number of results processed, or error
     */
    [[nodiscard]] network::Result<size_t> find_streaming(
        network::association& assoc,
        const core::dicom_dataset& query_keys,
        query_streaming_callback callback);

    // =========================================================================
    // Typed Convenience Methods
    // =========================================================================

    /**
     * @brief Query for patients
     *
     * @param assoc The established association to use
     * @param keys Patient-level query keys
     * @return Result containing query_result on success
     */
    [[nodiscard]] network::Result<query_result> find_patients(
        network::association& assoc,
        const patient_query_keys& keys);

    /**
     * @brief Query for studies
     *
     * @param assoc The established association to use
     * @param keys Study-level query keys
     * @return Result containing query_result on success
     */
    [[nodiscard]] network::Result<query_result> find_studies(
        network::association& assoc,
        const study_query_keys& keys);

    /**
     * @brief Query for series within a study
     *
     * @param assoc The established association to use
     * @param keys Series-level query keys (study_uid required)
     * @return Result containing query_result on success
     */
    [[nodiscard]] network::Result<query_result> find_series(
        network::association& assoc,
        const series_query_keys& keys);

    /**
     * @brief Query for instances within a series
     *
     * @param assoc The established association to use
     * @param keys Instance-level query keys (series_uid required)
     * @return Result containing query_result on success
     */
    [[nodiscard]] network::Result<query_result> find_instances(
        network::association& assoc,
        const instance_query_keys& keys);

    // =========================================================================
    // C-CANCEL Support
    // =========================================================================

    /**
     * @brief Send a C-CANCEL request to stop an ongoing query
     *
     * @param assoc The association on which the query is running
     * @param message_id The message ID of the query to cancel
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
    void set_config(const query_scu_config& config);

    /**
     * @brief Get the current configuration
     *
     * @return Reference to the current configuration
     */
    [[nodiscard]] const query_scu_config& config() const noexcept;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get the number of queries performed since construction
     * @return Count of C-FIND requests sent
     */
    [[nodiscard]] size_t queries_performed() const noexcept;

    /**
     * @brief Get the total number of matches received since construction
     * @return Total count of matching datasets received
     */
    [[nodiscard]] size_t total_matches() const noexcept;

    /**
     * @brief Reset statistics counters to zero
     */
    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /**
     * @brief Internal query implementation
     */
    [[nodiscard]] network::Result<query_result> find_impl(
        network::association& assoc,
        const core::dicom_dataset& query_keys,
        uint16_t message_id);

    /**
     * @brief Get the next message ID for DIMSE operations
     */
    [[nodiscard]] uint16_t next_message_id() noexcept;

    /**
     * @brief Build query dataset from patient keys
     */
    [[nodiscard]] core::dicom_dataset build_query_dataset(
        const patient_query_keys& keys) const;

    /**
     * @brief Build query dataset from study keys
     */
    [[nodiscard]] core::dicom_dataset build_query_dataset(
        const study_query_keys& keys) const;

    /**
     * @brief Build query dataset from series keys
     */
    [[nodiscard]] core::dicom_dataset build_query_dataset(
        const series_query_keys& keys) const;

    /**
     * @brief Build query dataset from instance keys
     */
    [[nodiscard]] core::dicom_dataset build_query_dataset(
        const instance_query_keys& keys) const;

    /**
     * @brief Get SOP Class UID based on current configuration
     */
    [[nodiscard]] std::string_view get_sop_class_uid() const noexcept;

    // =========================================================================
    // Private Members
    // =========================================================================

    /// Logger instance for service logging
    std::shared_ptr<di::ILogger> logger_;

    /// Configuration
    query_scu_config config_;

    /// Message ID counter
    std::atomic<uint16_t> message_id_counter_{1};

    /// Statistics: number of queries performed
    std::atomic<size_t> queries_performed_{0};

    /// Statistics: total number of matches received
    std::atomic<size_t> total_matches_{0};
};

}  // namespace pacs::services

#endif  // PACS_SERVICES_QUERY_SCU_HPP
