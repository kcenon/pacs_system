/**
 * @file test_data_generator.hpp
 * @brief Comprehensive DICOM test data generators for integration testing
 *
 * Provides a dedicated class for generating various types of DICOM datasets
 * including different modalities (CT, MR, XA, US), multi-frame images,
 * enhanced IODs, and edge case datasets for thorough testing.
 *
 * @see Issue #137 - Test Fixtures Extension
 * @see Issue #134 - Integration Test Enhancement Epic
 */

#ifndef PACS_INTEGRATION_TESTS_TEST_DATA_GENERATOR_HPP
#define PACS_INTEGRATION_TESTS_TEST_DATA_GENERATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/services/sop_classes/us_storage.hpp"
#include "pacs/services/sop_classes/xa_storage.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace pacs::integration_test {

// =============================================================================
// Forward Declarations and Types
// =============================================================================

/**
 * @brief Types of invalid datasets for error testing
 */
enum class invalid_dataset_type {
    missing_sop_class_uid,      ///< Missing SOP Class UID
    missing_sop_instance_uid,   ///< Missing SOP Instance UID
    missing_patient_id,         ///< Missing Patient ID
    missing_study_instance_uid, ///< Missing Study Instance UID
    invalid_vr,                 ///< Invalid Value Representation
    corrupted_pixel_data,       ///< Corrupted pixel data
    oversized_value             ///< Value exceeds VR length limit
};

/**
 * @brief Represents a complete patient study with multiple modalities
 *
 * Used for testing clinical workflow scenarios where a patient
 * undergoes multiple imaging procedures in a single study.
 */
struct multi_modal_study {
    std::string patient_id;                      ///< Patient identifier
    std::string patient_name;                    ///< Patient name
    std::string study_uid;                       ///< Study Instance UID
    std::vector<core::dicom_dataset> datasets;   ///< All datasets in study

    /**
     * @brief Get datasets filtered by modality
     * @param modality Modality code (CT, MR, XA, US, etc.)
     * @return Vector of datasets matching the modality
     */
    [[nodiscard]] std::vector<core::dicom_dataset>
    get_by_modality(const std::string& modality) const;

    /**
     * @brief Get total number of series in study
     * @return Count of unique series
     */
    [[nodiscard]] size_t series_count() const;
};

// =============================================================================
// Test Data Generator Class
// =============================================================================

/**
 * @brief Comprehensive DICOM test data generator
 *
 * Provides static methods to generate various DICOM datasets for testing.
 * All generated datasets contain valid DICOM structures suitable for
 * storage, query, and retrieval operations.
 *
 * Example usage:
 * @code
 * // Generate a simple CT dataset
 * auto ct = test_data_generator::ct();
 *
 * // Generate XA cine with 30 frames
 * auto xa = test_data_generator::xa_cine(30);
 *
 * // Generate a multi-modal study
 * auto study = test_data_generator::patient_journey("PATIENT001", {"CT", "MR", "XA"});
 * @endcode
 */
class test_data_generator {
public:
    // =========================================================================
    // Single Modality Generators
    // =========================================================================

    /**
     * @brief Generate a CT Image dataset
     * @param study_uid Study Instance UID (auto-generated if empty)
     * @return DICOM dataset for CT Image Storage
     */
    [[nodiscard]] static core::dicom_dataset ct(const std::string& study_uid = "");

    /**
     * @brief Generate an MR Image dataset
     * @param study_uid Study Instance UID (auto-generated if empty)
     * @return DICOM dataset for MR Image Storage
     */
    [[nodiscard]] static core::dicom_dataset mr(const std::string& study_uid = "");

    /**
     * @brief Generate a single-frame XA Image dataset
     * @param study_uid Study Instance UID (auto-generated if empty)
     * @return DICOM dataset for XA Image Storage
     */
    [[nodiscard]] static core::dicom_dataset xa(const std::string& study_uid = "");

    /**
     * @brief Generate a single-frame US Image dataset
     * @param study_uid Study Instance UID (auto-generated if empty)
     * @return DICOM dataset for US Image Storage
     */
    [[nodiscard]] static core::dicom_dataset us(const std::string& study_uid = "");

    // =========================================================================
    // Multi-frame Generators
    // =========================================================================

    /**
     * @brief Generate a multi-frame XA cine dataset
     * @param frames Number of frames (default: 30)
     * @param study_uid Study Instance UID (auto-generated if empty)
     * @return DICOM dataset for multi-frame XA Image Storage
     */
    [[nodiscard]] static core::dicom_dataset xa_cine(
        uint32_t frames = 30,
        const std::string& study_uid = "");

    /**
     * @brief Generate a multi-frame US cine dataset
     * @param frames Number of frames (default: 60)
     * @param study_uid Study Instance UID (auto-generated if empty)
     * @return DICOM dataset for multi-frame US Image Storage
     */
    [[nodiscard]] static core::dicom_dataset us_cine(
        uint32_t frames = 60,
        const std::string& study_uid = "");

    /**
     * @brief Generate an Enhanced CT multi-frame dataset
     * @param frames Number of frames (default: 100)
     * @param study_uid Study Instance UID (auto-generated if empty)
     * @return DICOM dataset for Enhanced CT Image Storage
     */
    [[nodiscard]] static core::dicom_dataset enhanced_ct(
        uint32_t frames = 100,
        const std::string& study_uid = "");

    /**
     * @brief Generate an Enhanced MR multi-frame dataset
     * @param frames Number of frames (default: 50)
     * @param study_uid Study Instance UID (auto-generated if empty)
     * @return DICOM dataset for Enhanced MR Image Storage
     */
    [[nodiscard]] static core::dicom_dataset enhanced_mr(
        uint32_t frames = 50,
        const std::string& study_uid = "");

    // =========================================================================
    // Clinical Workflow Generators
    // =========================================================================

    /**
     * @brief Generate a complete multi-modal patient study
     *
     * Creates a realistic patient study with multiple modalities,
     * all sharing the same Study Instance UID and patient information.
     *
     * @param patient_id Patient identifier
     * @param modalities List of modality codes (e.g., {"CT", "MR", "XA"})
     * @return Multi-modal study containing all generated datasets
     */
    [[nodiscard]] static multi_modal_study patient_journey(
        const std::string& patient_id,
        const std::vector<std::string>& modalities = {"CT", "MR", "XA"});

    /**
     * @brief Generate a worklist item dataset
     * @param patient_id Patient identifier (auto-generated if empty)
     * @param modality Scheduled modality (default: "CT")
     * @return DICOM dataset for Modality Worklist
     */
    [[nodiscard]] static core::dicom_dataset worklist(
        const std::string& patient_id = "",
        const std::string& modality = "CT");

    // =========================================================================
    // Edge Case Generators
    // =========================================================================

    /**
     * @brief Generate a large dataset for stress testing
     *
     * Creates a dataset with pixel data approaching the target size.
     * Useful for testing memory handling and transfer performance.
     *
     * @param target_size_mb Target size in megabytes
     * @return DICOM dataset with large pixel data
     */
    [[nodiscard]] static core::dicom_dataset large(size_t target_size_mb);

    /**
     * @brief Generate a dataset with Unicode patient names
     *
     * Tests character set handling with international characters
     * including CJK, Arabic, Hebrew, and special symbols.
     *
     * @return DICOM dataset with Unicode patient information
     */
    [[nodiscard]] static core::dicom_dataset unicode();

    /**
     * @brief Generate a dataset with private tags
     *
     * Creates a dataset containing custom private tags for testing
     * private tag preservation during storage and retrieval.
     *
     * @param creator_id Private creator identifier
     * @return DICOM dataset with private tags
     */
    [[nodiscard]] static core::dicom_dataset with_private_tags(
        const std::string& creator_id = "TEST PRIVATE CREATOR");

    /**
     * @brief Generate an intentionally invalid dataset
     *
     * Creates datasets with specific types of errors for testing
     * error handling and validation logic.
     *
     * @param type Type of invalidity to introduce
     * @return Invalid DICOM dataset
     */
    [[nodiscard]] static core::dicom_dataset invalid(invalid_dataset_type type);

    // =========================================================================
    // Utility Functions
    // =========================================================================

    /**
     * @brief Generate a unique UID for testing
     * @param root Root UID prefix (default: test UID root)
     * @return Unique UID string
     */
    [[nodiscard]] static std::string generate_uid(
        const std::string& root = "1.2.826.0.1.3680043.9.9999");

    /**
     * @brief Get current date in DICOM DA format (YYYYMMDD)
     * @return Date string
     */
    [[nodiscard]] static std::string current_date();

    /**
     * @brief Get current time in DICOM TM format (HHMMSS)
     * @return Time string
     */
    [[nodiscard]] static std::string current_time();

private:
    /**
     * @brief Add common patient module attributes
     */
    static void add_patient_module(
        core::dicom_dataset& ds,
        const std::string& patient_name,
        const std::string& patient_id,
        const std::string& birth_date = "19800101",
        const std::string& sex = "M");

    /**
     * @brief Add common study module attributes
     */
    static void add_study_module(
        core::dicom_dataset& ds,
        const std::string& study_uid,
        const std::string& accession_number,
        const std::string& study_id,
        const std::string& description);

    /**
     * @brief Add common series module attributes
     */
    static void add_series_module(
        core::dicom_dataset& ds,
        const std::string& series_uid,
        const std::string& modality,
        const std::string& series_number,
        const std::string& description);

    /**
     * @brief Add common image pixel module attributes
     */
    static void add_image_pixel_module(
        core::dicom_dataset& ds,
        uint16_t rows,
        uint16_t columns,
        uint16_t bits_allocated,
        uint16_t bits_stored,
        uint16_t samples_per_pixel = 1,
        const std::string& photometric = "MONOCHROME2");

    /**
     * @brief Generate pixel data for single-frame image
     */
    static void add_pixel_data(
        core::dicom_dataset& ds,
        uint16_t rows,
        uint16_t columns,
        uint16_t bits_allocated,
        uint16_t samples_per_pixel = 1,
        uint16_t fill_value = 512);

    /**
     * @brief Generate pixel data for multi-frame image
     */
    static void add_multiframe_pixel_data(
        core::dicom_dataset& ds,
        uint16_t rows,
        uint16_t columns,
        uint16_t bits_allocated,
        uint32_t num_frames,
        uint16_t samples_per_pixel = 1);
};

}  // namespace pacs::integration_test

#endif  // PACS_INTEGRATION_TESTS_TEST_DATA_GENERATOR_HPP
