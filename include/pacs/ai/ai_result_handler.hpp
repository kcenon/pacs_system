/**
 * @file ai_result_handler.hpp
 * @brief Handler for AI-generated DICOM objects (SR, SEG, PR)
 *
 * This file provides the ai_result_handler class for receiving, validating,
 * and storing AI inference outputs such as Structured Reports (SR),
 * Segmentation objects (SEG), and Presentation States (PR).
 *
 * @see DICOM PS3.3 - SR, SEG, PR IODs
 * @see Issue #204 - AI result handler implementation
 */

#pragma once

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/storage/index_database.hpp>
#include <pacs/storage/storage_interface.hpp>

#include <kcenon/common/patterns/result.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::ai {

/// Result type alias for operations returning a value
template <typename T>
using Result = kcenon::common::Result<T>;

/// Result type alias for void operations
using VoidResult = kcenon::common::VoidResult;

// ─────────────────────────────────────────────────────
// Enumerations
// ─────────────────────────────────────────────────────

/**
 * @brief Types of AI-generated DICOM objects
 */
enum class ai_result_type {
    structured_report,    ///< DICOM SR (Structured Report)
    segmentation,         ///< DICOM SEG (Segmentation)
    presentation_state    ///< DICOM PR (Presentation State)
};

/**
 * @brief Validation status for AI results
 */
enum class validation_status {
    valid,                    ///< All validations passed
    missing_required_tags,    ///< Required DICOM tags are missing
    invalid_reference,        ///< Referenced source images not found
    invalid_template,         ///< SR template conformance failed
    invalid_segment_data,     ///< Segmentation data is malformed
    unknown_error             ///< Unexpected validation error
};

// ─────────────────────────────────────────────────────
// Data Structures
// ─────────────────────────────────────────────────────

/**
 * @brief Information about an AI result stored in the system
 */
struct ai_result_info {
    /// SOP Instance UID of the AI result
    std::string sop_instance_uid;

    /// Type of AI result
    ai_result_type type;

    /// SOP Class UID
    std::string sop_class_uid;

    /// Series Instance UID
    std::string series_instance_uid;

    /// Study Instance UID of the source study
    std::string source_study_uid;

    /// AI model/algorithm identifier
    std::string algorithm_name;

    /// Algorithm version
    std::string algorithm_version;

    /// Timestamp when the result was received
    std::chrono::system_clock::time_point received_at;

    /// Optional description
    std::optional<std::string> description;
};

/**
 * @brief Source reference linking AI result to original images
 */
struct source_reference {
    /// Study Instance UID
    std::string study_instance_uid;

    /// Series Instance UID (optional, for series-level reference)
    std::optional<std::string> series_instance_uid;

    /// SOP Instance UIDs (optional, for instance-level reference)
    std::vector<std::string> sop_instance_uids;
};

/**
 * @brief CAD finding extracted from Structured Report
 */
struct cad_finding {
    /// Finding type/category
    std::string finding_type;

    /// Location/site description
    std::string location;

    /// Confidence score (0.0 to 1.0)
    std::optional<double> confidence;

    /// Additional measurement or annotation data
    std::optional<std::string> measurement;

    /// Reference to specific image where finding was detected
    std::optional<std::string> referenced_sop_instance_uid;
};

/**
 * @brief Segment information from Segmentation object
 */
struct segment_info {
    /// Segment number (1-based)
    uint16_t segment_number;

    /// Segment label
    std::string segment_label;

    /// Segment description
    std::optional<std::string> description;

    /// Algorithm type that created this segment
    std::string algorithm_type;

    /// RGB color for visualization (optional)
    std::optional<std::tuple<uint8_t, uint8_t, uint8_t>> recommended_display_color;
};

/**
 * @brief Validation result containing status and details
 */
struct validation_result {
    /// Overall validation status
    validation_status status;

    /// Detailed error message if validation failed
    std::optional<std::string> error_message;

    /// List of missing required tags (if applicable)
    std::vector<std::string> missing_tags;

    /// List of invalid references (if applicable)
    std::vector<std::string> invalid_references;
};

/**
 * @brief Configuration for AI result handler
 */
struct ai_handler_config {
    /// Whether to validate source references exist in storage
    bool validate_source_references = true;

    /// Whether to validate SR template conformance
    bool validate_sr_templates = true;

    /// Whether to auto-link results to source studies
    bool auto_link_to_source = true;

    /// Accepted SR template identifiers (empty = accept all)
    std::vector<std::string> accepted_sr_templates;

    /// Maximum segment count for SEG objects (0 = unlimited)
    uint16_t max_segments = 256;
};

// ─────────────────────────────────────────────────────
// Callback Types
// ─────────────────────────────────────────────────────

/**
 * @brief Callback for notification when AI result is received
 *
 * @param info Information about the received AI result
 */
using ai_result_received_callback = std::function<void(const ai_result_info& info)>;

/**
 * @brief Callback for pre-storage validation
 *
 * @param dataset The AI result dataset to validate
 * @param type The type of AI result
 * @return true to proceed with storage, false to reject
 */
using pre_store_validator = std::function<bool(
    const core::dicom_dataset& dataset,
    ai_result_type type)>;

// ─────────────────────────────────────────────────────
// Main Class
// ─────────────────────────────────────────────────────

/**
 * @brief Handler for AI-generated DICOM objects
 *
 * The ai_result_handler manages the reception, validation, and storage of
 * AI inference outputs. It supports:
 * - Structured Reports (SR) with CAD findings
 * - Segmentation objects (SEG) with binary/fractional segments
 * - Presentation States (PR) with annotations and measurements
 *
 * Thread Safety: This class is NOT thread-safe. External synchronization
 * is required for concurrent access.
 *
 * @example
 * @code
 * // Create handler with storage and database
 * auto handler = ai_result_handler::create(storage, database);
 *
 * // Configure validation options
 * ai_handler_config config;
 * config.validate_source_references = true;
 * config.auto_link_to_source = true;
 * handler->configure(config);
 *
 * // Receive AI-generated structured report
 * core::dicom_dataset sr_dataset;
 * // ... populate SR dataset ...
 * auto result = handler->receive_structured_report(sr_dataset);
 * if (result.is_ok()) {
 *     // Successfully stored
 *     auto findings = handler->get_cad_findings(sr_dataset.get_string(...));
 * }
 *
 * // Query AI results for a study
 * auto ai_results = handler->find_ai_results_for_study("1.2.3...");
 * @endcode
 */
class ai_result_handler {
public:
    /**
     * @brief Create an AI result handler
     *
     * @param storage Storage backend for persisting DICOM objects
     * @param database Index database for metadata queries
     * @return Unique pointer to the handler instance
     */
    [[nodiscard]] static auto create(
        std::shared_ptr<storage::storage_interface> storage,
        std::shared_ptr<storage::index_database> database)
        -> std::unique_ptr<ai_result_handler>;

    /**
     * @brief Destructor
     */
    virtual ~ai_result_handler();

    // Non-copyable, movable
    ai_result_handler(const ai_result_handler&) = delete;
    auto operator=(const ai_result_handler&) -> ai_result_handler& = delete;
    ai_result_handler(ai_result_handler&&) noexcept;
    auto operator=(ai_result_handler&&) noexcept -> ai_result_handler&;

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * @brief Configure handler behavior
     *
     * @param config Configuration options
     */
    void configure(const ai_handler_config& config);

    /**
     * @brief Get current configuration
     *
     * @return Current configuration
     */
    [[nodiscard]] auto get_config() const -> ai_handler_config;

    /**
     * @brief Set callback for AI result reception notifications
     *
     * @param callback Function to call when AI result is received
     */
    void set_received_callback(ai_result_received_callback callback);

    /**
     * @brief Set custom pre-storage validator
     *
     * @param validator Function to validate before storage
     */
    void set_pre_store_validator(pre_store_validator validator);

    // ========================================================================
    // Structured Report (SR) Operations
    // ========================================================================

    /**
     * @brief Receive and store an AI-generated Structured Report
     *
     * Validates the SR dataset for required tags and template conformance,
     * then stores it and links it to the source study.
     *
     * @param sr The Structured Report dataset
     * @return VoidResult Success or error information
     *
     * @note The dataset must contain valid Content Sequence with CAD findings
     */
    [[nodiscard]] auto receive_structured_report(const core::dicom_dataset& sr)
        -> VoidResult;

    /**
     * @brief Validate SR template conformance
     *
     * @param sr The Structured Report dataset to validate
     * @return Validation result with status and details
     */
    [[nodiscard]] auto validate_sr_template(const core::dicom_dataset& sr)
        -> validation_result;

    /**
     * @brief Extract CAD findings from a Structured Report
     *
     * @param sr_sop_instance_uid SOP Instance UID of the SR
     * @return Result containing list of CAD findings or error
     */
    [[nodiscard]] auto get_cad_findings(std::string_view sr_sop_instance_uid)
        -> Result<std::vector<cad_finding>>;

    // ========================================================================
    // Segmentation (SEG) Operations
    // ========================================================================

    /**
     * @brief Receive and store an AI-generated Segmentation object
     *
     * Validates the SEG dataset for required tags and segment sequence,
     * then stores it and links it to the source images.
     *
     * @param seg The Segmentation dataset
     * @return VoidResult Success or error information
     *
     * @note Supports both BINARY and FRACTIONAL segment types
     */
    [[nodiscard]] auto receive_segmentation(const core::dicom_dataset& seg)
        -> VoidResult;

    /**
     * @brief Validate segmentation data integrity
     *
     * @param seg The Segmentation dataset to validate
     * @return Validation result with status and details
     */
    [[nodiscard]] auto validate_segmentation(const core::dicom_dataset& seg)
        -> validation_result;

    /**
     * @brief Get segment information from a stored Segmentation
     *
     * @param seg_sop_instance_uid SOP Instance UID of the SEG
     * @return Result containing list of segment information or error
     */
    [[nodiscard]] auto get_segment_info(std::string_view seg_sop_instance_uid)
        -> Result<std::vector<segment_info>>;

    // ========================================================================
    // Presentation State (PR) Operations
    // ========================================================================

    /**
     * @brief Receive and store an AI-generated Presentation State
     *
     * Validates the PR dataset and stores it linked to source images.
     *
     * @param pr The Presentation State dataset
     * @return VoidResult Success or error information
     *
     * @note Handles annotations and measurements from AI analysis
     */
    [[nodiscard]] auto receive_presentation_state(const core::dicom_dataset& pr)
        -> VoidResult;

    /**
     * @brief Validate Presentation State
     *
     * @param pr The Presentation State dataset to validate
     * @return Validation result with status and details
     */
    [[nodiscard]] auto validate_presentation_state(const core::dicom_dataset& pr)
        -> validation_result;

    // ========================================================================
    // Source Linking Operations
    // ========================================================================

    /**
     * @brief Link an AI result to its source study
     *
     * Creates a reference association between the AI result and
     * the original study/series/instances it was derived from.
     *
     * @param result_uid SOP Instance UID of the AI result
     * @param source_study_uid Study Instance UID of the source
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto link_to_source(
        std::string_view result_uid,
        std::string_view source_study_uid) -> VoidResult;

    /**
     * @brief Link an AI result with detailed source references
     *
     * @param result_uid SOP Instance UID of the AI result
     * @param references Source references (study, series, instances)
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto link_to_source(
        std::string_view result_uid,
        const source_reference& references) -> VoidResult;

    /**
     * @brief Get source references for an AI result
     *
     * @param result_uid SOP Instance UID of the AI result
     * @return Result containing source reference or error
     */
    [[nodiscard]] auto get_source_reference(std::string_view result_uid)
        -> Result<source_reference>;

    // ========================================================================
    // Query Operations
    // ========================================================================

    /**
     * @brief Find all AI results linked to a study
     *
     * @param study_instance_uid Study Instance UID
     * @return Result containing list of AI result info or error
     */
    [[nodiscard]] auto find_ai_results_for_study(std::string_view study_instance_uid)
        -> Result<std::vector<ai_result_info>>;

    /**
     * @brief Find AI results by type
     *
     * @param study_instance_uid Study Instance UID
     * @param type Type of AI result to search for
     * @return Result containing list of AI result info or error
     */
    [[nodiscard]] auto find_ai_results_by_type(
        std::string_view study_instance_uid,
        ai_result_type type) -> Result<std::vector<ai_result_info>>;

    /**
     * @brief Get AI result information by SOP Instance UID
     *
     * @param sop_instance_uid SOP Instance UID of the AI result
     * @return Optional containing AI result info if found
     */
    [[nodiscard]] auto get_ai_result_info(std::string_view sop_instance_uid)
        -> std::optional<ai_result_info>;

    /**
     * @brief Check if an AI result exists
     *
     * @param sop_instance_uid SOP Instance UID to check
     * @return true if the AI result exists, false otherwise
     */
    [[nodiscard]] auto exists(std::string_view sop_instance_uid) const -> bool;

    // ========================================================================
    // Removal Operations
    // ========================================================================

    /**
     * @brief Remove an AI result and its source links
     *
     * @param sop_instance_uid SOP Instance UID of the AI result to remove
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto remove(std::string_view sop_instance_uid) -> VoidResult;

    /**
     * @brief Remove all AI results linked to a study
     *
     * @param study_instance_uid Study Instance UID
     * @return Result containing count of removed results or error
     */
    [[nodiscard]] auto remove_ai_results_for_study(std::string_view study_instance_uid)
        -> Result<std::size_t>;

protected:
    /**
     * @brief Protected constructor - use create() factory method
     */
    ai_result_handler(
        std::shared_ptr<storage::storage_interface> storage,
        std::shared_ptr<storage::index_database> database);

private:
    /// Implementation details (pimpl idiom)
    class impl;
    std::unique_ptr<impl> pimpl_;
};

}  // namespace pacs::ai
