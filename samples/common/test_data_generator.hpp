/**
 * @file test_data_generator.hpp
 * @brief Test data generation utilities for developer samples
 *
 * This file provides utilities for generating test DICOM datasets
 * with realistic patient, study, and image data for sample applications.
 */

#pragma once

#include <pacs/core/dicom_dataset.hpp>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::samples {

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief Patient information for test data generation
 */
struct patient_info {
    std::string patient_id{"PAT001"};
    std::string patient_name{"DOE^JOHN"};
    std::string birth_date{"19800101"};
    std::string sex{"M"};
};

/**
 * @brief Study information for test data generation
 */
struct study_info {
    std::string study_uid;
    std::string study_date;
    std::string study_time;
    std::string accession_number;
    std::string description;
    std::string referring_physician;
};

/**
 * @brief Image parameters for test data generation
 */
struct image_params {
    uint16_t rows{512};
    uint16_t columns{512};
    uint16_t bits_allocated{16};
    uint16_t bits_stored{12};
    uint16_t high_bit{11};
    uint16_t pixel_representation{0};  // 0 = unsigned, 1 = signed
    std::string modality{"CT"};
    std::string photometric{"MONOCHROME2"};
    double window_center{40.0};
    double window_width{400.0};
};

// ============================================================================
// Test Data Generator
// ============================================================================

/**
 * @brief Utility class for generating test DICOM datasets
 *
 * Provides static methods for creating realistic DICOM datasets
 * for testing and sample applications. Supports various modalities
 * including CT, MR, CR, and DX.
 *
 * Thread Safety: UID generation is thread-safe using atomic counters.
 *
 * @example
 * @code
 * // Generate a simple CT dataset
 * auto ds = test_data_generator::create_ct_dataset();
 *
 * // Generate with custom patient info
 * patient_info patient{"PAT123", "SMITH^JANE", "19900515", "F"};
 * auto ds = test_data_generator::create_ct_dataset(patient);
 *
 * // Generate a complete study
 * auto datasets = test_data_generator::create_study(
 *     patient, study, 2, 10);  // 2 series, 10 instances each
 *
 * // Save to directory
 * test_data_generator::save_to_directory(datasets, "./test_data");
 * @endcode
 */
class test_data_generator {
public:
    // ========================================================================
    // Single Dataset Generation
    // ========================================================================

    /**
     * @brief Create a CT dataset with default or custom parameters
     * @param patient Patient information (default if not provided)
     * @param params Image parameters (default CT params if not provided)
     * @return A valid DICOM dataset for CT Storage
     */
    [[nodiscard]] static auto create_ct_dataset(
        const patient_info& patient = {},
        const image_params& params = {}) -> core::dicom_dataset;

    /**
     * @brief Create an MR dataset with default or custom parameters
     * @param patient Patient information
     * @param params Image parameters (default MR params if not provided)
     * @return A valid DICOM dataset for MR Storage
     */
    [[nodiscard]] static auto create_mr_dataset(
        const patient_info& patient = {},
        image_params params = {}) -> core::dicom_dataset;

    /**
     * @brief Create a CR (Computed Radiography) dataset
     * @param patient Patient information
     * @param params Image parameters (default CR params if not provided)
     * @return A valid DICOM dataset for CR Storage
     */
    [[nodiscard]] static auto create_cr_dataset(
        const patient_info& patient = {},
        image_params params = {}) -> core::dicom_dataset;

    /**
     * @brief Create a DX (Digital X-Ray) dataset
     * @param patient Patient information
     * @param params Image parameters (default DX params if not provided)
     * @return A valid DICOM dataset for DX Storage
     */
    [[nodiscard]] static auto create_dx_dataset(
        const patient_info& patient = {},
        image_params params = {}) -> core::dicom_dataset;

    // ========================================================================
    // Study Generation
    // ========================================================================

    /**
     * @brief Generate a complete study with multiple series and instances
     * @param patient Patient information
     * @param study Study information
     * @param series_count Number of series to generate
     * @param instances_per_series Number of instances per series
     * @return Vector of DICOM datasets comprising the study
     */
    [[nodiscard]] static auto create_study(
        const patient_info& patient,
        const study_info& study,
        int series_count = 1,
        int instances_per_series = 5) -> std::vector<core::dicom_dataset>;

    // ========================================================================
    // File Operations
    // ========================================================================

    /**
     * @brief Save datasets to a directory structure
     * @param datasets The datasets to save
     * @param directory Target directory path
     *
     * Creates a hierarchical directory structure:
     * directory/PatientID/StudyUID/SeriesUID/InstanceUID.dcm
     */
    static void save_to_directory(
        const std::vector<core::dicom_dataset>& datasets,
        const std::filesystem::path& directory);

    // ========================================================================
    // UID Generation
    // ========================================================================

    /**
     * @brief Generate a unique DICOM UID
     * @param root UID root (default: pacs_system sample root)
     * @return A unique UID string
     *
     * Generated UIDs follow the format: root.timestamp.counter
     * Thread-safe using atomic counter.
     */
    [[nodiscard]] static auto generate_uid(
        std::string_view root = "1.2.410.200001.1.1") -> std::string;

    // ========================================================================
    // Date/Time Utilities
    // ========================================================================

    /**
     * @brief Get current date in DICOM format (YYYYMMDD)
     * @return Current date string
     */
    [[nodiscard]] static auto current_date() -> std::string;

    /**
     * @brief Get current time in DICOM format (HHMMSS.FFFFFF)
     * @return Current time string
     */
    [[nodiscard]] static auto current_time() -> std::string;

private:
    /**
     * @brief Create a base dataset with common elements
     * @param patient Patient information
     * @param params Image parameters
     * @return Dataset with common elements populated
     */
    [[nodiscard]] static auto create_base_dataset(
        const patient_info& patient,
        const image_params& params) -> core::dicom_dataset;

    /**
     * @brief Generate test pixel data
     * @param rows Number of rows
     * @param columns Number of columns
     * @param bits_allocated Bits allocated per pixel
     * @return Vector of pixel data bytes
     */
    [[nodiscard]] static auto generate_pixel_data(
        uint16_t rows,
        uint16_t columns,
        uint16_t bits_allocated) -> std::vector<uint8_t>;

    /// Atomic counter for UID generation
    static std::atomic<uint64_t> uid_counter_;
};

}  // namespace pacs::samples
