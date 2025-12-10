/**
 * @file sr_iod_validator.hpp
 * @brief Structured Report IOD Validator
 *
 * Provides validation for Structured Report Information Object Definitions
 * as specified in DICOM PS3.3 Section A.35 (SR Document IODs).
 *
 * @see DICOM PS3.3 Section A.35 - SR Document IODs
 * @see DICOM PS3.3 Section C.17 - SR Document Content Module
 */

#ifndef PACS_SERVICES_VALIDATION_SR_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_SR_IOD_VALIDATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/services/validation/us_iod_validator.hpp"  // For validation_result types

#include <optional>
#include <string>
#include <vector>

namespace pacs::services::validation {

// =============================================================================
// SR Validation Options
// =============================================================================

/**
 * @brief Options for SR IOD validation
 */
struct sr_validation_options {
    /// Check Type 1 (required) attributes
    bool check_type1 = true;

    /// Check Type 2 (required, can be empty) attributes
    bool check_type2 = true;

    /// Check Type 1C/2C (conditionally required) attributes
    bool check_conditional = true;

    /// Validate Content Sequence structure
    bool validate_content_sequence = true;

    /// Validate content item value types
    bool validate_value_types = true;

    /// Validate relationship types between content items
    bool validate_relationships = true;

    /// Validate referenced SOP instances
    bool validate_references = true;

    /// Validate coded entries (concept name codes, etc.)
    bool validate_coded_entries = true;

    /// Validate template identification if present
    bool validate_template_id = true;

    /// Validate completion and verification flags
    bool validate_document_status = true;

    /// Allow Key Object Selection document specific validation
    bool validate_key_object_selection = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// SR IOD Validator
// =============================================================================

/**
 * @brief Validator for Structured Report IODs
 *
 * Validates DICOM datasets against various SR Document IOD specifications.
 * Supports Basic Text SR, Enhanced SR, Comprehensive SR, and specialized
 * SR documents including CAD SR and Key Object Selection.
 *
 * ## Validated Modules
 *
 * ### Common Modules (All SR Types)
 * - Patient Module (M)
 * - General Study Module (M)
 * - SR Document Series Module (M)
 * - General Equipment Module (M)
 * - SR Document General Module (M)
 * - SR Document Content Module (M)
 * - SOP Common Module (M)
 *
 * ### Enhanced SR Additional Modules
 * - Current Requested Procedure Evidence Sequence (1C)
 * - Pertinent Other Evidence Sequence (1C)
 *
 * ### Comprehensive SR Additional Modules
 * - SCOORD/SCOORD3D support
 *
 * @example
 * @code
 * sr_iod_validator validator;
 * auto result = validator.validate(dataset);
 *
 * if (!result.is_valid) {
 *     for (const auto& finding : result.findings) {
 *         std::cerr << finding.code << ": " << finding.message << "\n";
 *     }
 * }
 * @endcode
 */
class sr_iod_validator {
public:
    /**
     * @brief Construct validator with default options
     */
    sr_iod_validator() = default;

    /**
     * @brief Construct validator with custom options
     * @param options Validation options
     */
    explicit sr_iod_validator(const sr_validation_options& options);

    /**
     * @brief Validate a DICOM dataset against SR IOD
     *
     * Automatically detects the SR type and applies appropriate validation.
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate a Basic Text SR dataset
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result
    validate_basic_text_sr(const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate an Enhanced SR dataset
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result
    validate_enhanced_sr(const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate a Comprehensive SR dataset
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result
    validate_comprehensive_sr(const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate a Key Object Selection document
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result
    validate_key_object_selection(const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate content tree structure
     *
     * Validates the Content Sequence hierarchy and relationships.
     *
     * @param dataset The dataset to validate
     * @return Validation result for content tree
     */
    [[nodiscard]] validation_result
    validate_content_tree(const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate referenced instances
     *
     * Checks that all referenced SOP instances are properly specified.
     *
     * @param dataset The dataset to validate
     * @return Validation result for references
     */
    [[nodiscard]] validation_result
    validate_references(const core::dicom_dataset& dataset) const;

    /**
     * @brief Quick check if dataset has minimum required attributes
     *
     * Faster than full validation - only checks Type 1 attributes.
     *
     * @param dataset The dataset to check
     * @return true if all Type 1 attributes are present
     */
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;

    /**
     * @brief Get the validation options
     */
    [[nodiscard]] const sr_validation_options& options() const noexcept;

    /**
     * @brief Set validation options
     */
    void set_options(const sr_validation_options& options);

private:
    // Module validation methods
    void validate_patient_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_general_study_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_sr_document_series_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_general_equipment_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_sr_document_general_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_sr_document_content_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_sop_common_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_evidence_sequences(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    // Content tree validation
    void validate_content_sequence(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_content_item(
        const core::dicom_dataset& item,
        size_t depth,
        std::string_view parent_value_type,
        std::vector<validation_finding>& findings) const;

    void validate_coded_entry(
        const core::dicom_dataset& coded_entry,
        std::string_view context,
        std::vector<validation_finding>& findings) const;

    // Value type specific validation
    void validate_text_content_item(
        const core::dicom_dataset& item,
        std::vector<validation_finding>& findings) const;

    void validate_code_content_item(
        const core::dicom_dataset& item,
        std::vector<validation_finding>& findings) const;

    void validate_num_content_item(
        const core::dicom_dataset& item,
        std::vector<validation_finding>& findings) const;

    void validate_image_content_item(
        const core::dicom_dataset& item,
        std::vector<validation_finding>& findings) const;

    void validate_scoord_content_item(
        const core::dicom_dataset& item,
        std::vector<validation_finding>& findings) const;

    void validate_scoord3d_content_item(
        const core::dicom_dataset& item,
        std::vector<validation_finding>& findings) const;

    // Attribute validation helpers
    void check_type1_attribute(
        const core::dicom_dataset& dataset,
        core::dicom_tag tag,
        std::string_view name,
        std::vector<validation_finding>& findings) const;

    void check_type2_attribute(
        const core::dicom_dataset& dataset,
        core::dicom_tag tag,
        std::string_view name,
        std::vector<validation_finding>& findings) const;

    void check_modality(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void check_completion_flag(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void check_verification_flag(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    sr_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate an SR dataset with default options
 *
 * @param dataset The dataset to validate
 * @return Validation result
 */
[[nodiscard]] validation_result validate_sr_iod(const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid SR document
 *
 * @param dataset The dataset to check
 * @return true if the dataset passes basic SR IOD validation
 */
[[nodiscard]] bool is_valid_sr_dataset(const core::dicom_dataset& dataset);

/**
 * @brief Check if SR document is complete
 *
 * @param dataset The dataset to check
 * @return true if Completion Flag is COMPLETE
 */
[[nodiscard]] bool is_sr_complete(const core::dicom_dataset& dataset);

/**
 * @brief Check if SR document is verified
 *
 * @param dataset The dataset to check
 * @return true if Verification Flag is VERIFIED
 */
[[nodiscard]] bool is_sr_verified(const core::dicom_dataset& dataset);

/**
 * @brief Get content item count from dataset
 *
 * @param dataset The dataset to check
 * @return Number of content items at root level, or 0 if cannot be determined
 */
[[nodiscard]] size_t get_content_item_count(const core::dicom_dataset& dataset);

/**
 * @brief Get SR document title from Concept Name Code Sequence
 *
 * @param dataset The dataset to check
 * @return Document title code meaning, or empty string if not found
 */
[[nodiscard]] std::string get_sr_document_title(const core::dicom_dataset& dataset);

}  // namespace pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_SR_IOD_VALIDATOR_HPP
