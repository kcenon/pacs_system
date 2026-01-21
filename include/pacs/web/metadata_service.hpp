/**
 * @file metadata_service.hpp
 * @brief Selective DICOM metadata retrieval and series navigation service
 *
 * Provides APIs for selective tag retrieval, preset-based metadata,
 * series navigation, and window/level presets.
 *
 * @see Issue #544 - Implement Selective Metadata & Navigation APIs
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace pacs::storage {
class index_database;
}  // namespace pacs::storage

namespace pacs::web {

/**
 * @brief Metadata preset types for common use cases
 */
enum class metadata_preset {
    image_display,   ///< Rows, Columns, Bits, PhotometricInterpretation
    window_level,    ///< WindowCenter, WindowWidth, Rescale values
    patient_info,    ///< Patient demographics
    acquisition,     ///< KVP, ExposureTime, SliceThickness
    positioning,     ///< ImagePosition, ImageOrientation, PixelSpacing
    multiframe       ///< NumberOfFrames, FrameTime
};

/**
 * @brief Convert preset enum to string
 */
[[nodiscard]] std::string_view preset_to_string(metadata_preset preset);

/**
 * @brief Parse preset from string
 */
[[nodiscard]] std::optional<metadata_preset> preset_from_string(
    std::string_view str);

/**
 * @brief Parameters for selective metadata retrieval
 */
struct metadata_request {
    /// Specific tags to retrieve (hex format: "00280010")
    std::vector<std::string> tags;

    /// Preset to apply
    std::optional<metadata_preset> preset;

    /// Include private tags in response
    bool include_private{false};
};

/**
 * @brief DICOM tag value in metadata response
 */
struct tag_value {
    /// Tag in hex format (e.g., "00280010")
    std::string tag;

    /// Value as string (numeric values converted to string)
    std::string value;

    /// Whether this is an array/sequence (for multi-valued elements)
    bool is_array{false};
};

/**
 * @brief Response for selective metadata retrieval
 */
struct metadata_response {
    /// Whether the operation succeeded
    bool success{false};

    /// Error message if failed
    std::string error_message;

    /// Retrieved tag values
    std::unordered_map<std::string, std::string> tags;

    /// Create a success result
    static metadata_response ok(
        std::unordered_map<std::string, std::string> tag_map) {
        metadata_response r;
        r.success = true;
        r.tags = std::move(tag_map);
        return r;
    }

    /// Create an error result
    static metadata_response error(std::string message) {
        metadata_response r;
        r.success = false;
        r.error_message = std::move(message);
        return r;
    }
};

/**
 * @brief Instance info for series navigation
 */
struct sorted_instance {
    /// SOP Instance UID
    std::string sop_instance_uid;

    /// Instance number (if available)
    std::optional<int> instance_number;

    /// Slice location (if available)
    std::optional<double> slice_location;

    /// Image Position Patient (if available)
    std::optional<std::vector<double>> image_position_patient;

    /// Acquisition time (if available)
    std::optional<std::string> acquisition_time;
};

/**
 * @brief Sort order for series instances
 */
enum class sort_order {
    position,         ///< Sort by ImagePositionPatient/SliceLocation
    instance_number,  ///< Sort by InstanceNumber
    acquisition_time  ///< Sort by AcquisitionTime
};

/**
 * @brief Convert sort order enum to string
 */
[[nodiscard]] std::string_view sort_order_to_string(sort_order order);

/**
 * @brief Parse sort order from string
 */
[[nodiscard]] std::optional<sort_order> sort_order_from_string(
    std::string_view str);

/**
 * @brief Response for sorted instances query
 */
struct sorted_instances_response {
    /// Whether the operation succeeded
    bool success{false};

    /// Error message if failed
    std::string error_message;

    /// Sorted instances
    std::vector<sorted_instance> instances;

    /// Total number of instances
    size_t total{0};

    /// Create a success result
    static sorted_instances_response ok(std::vector<sorted_instance> inst,
                                         size_t count) {
        sorted_instances_response r;
        r.success = true;
        r.instances = std::move(inst);
        r.total = count;
        return r;
    }

    /// Create an error result
    static sorted_instances_response error(std::string message) {
        sorted_instances_response r;
        r.success = false;
        r.error_message = std::move(message);
        return r;
    }
};

/**
 * @brief Navigation info for an instance
 */
struct navigation_info {
    /// Whether the operation succeeded
    bool success{false};

    /// Error message if failed
    std::string error_message;

    /// Previous instance UID (empty if first)
    std::string previous;

    /// Next instance UID (empty if last)
    std::string next;

    /// Current index (0-based)
    size_t index{0};

    /// Total instances in series
    size_t total{0};

    /// First instance UID
    std::string first;

    /// Last instance UID
    std::string last;

    /// Create a success result
    static navigation_info ok() {
        navigation_info r;
        r.success = true;
        return r;
    }

    /// Create an error result
    static navigation_info error(std::string message) {
        navigation_info r;
        r.success = false;
        r.error_message = std::move(message);
        return r;
    }
};

/**
 * @brief Window/Level preset
 */
struct window_level_preset {
    /// Preset name (e.g., "Lung", "Bone")
    std::string name;

    /// Window center value
    double center;

    /// Window width value
    double width;
};

/**
 * @brief VOI LUT information from DICOM
 */
struct voi_lut_info {
    /// Whether the operation succeeded
    bool success{false};

    /// Error message if failed
    std::string error_message;

    /// Window center values
    std::vector<double> window_center;

    /// Window width values
    std::vector<double> window_width;

    /// Window explanations (optional descriptions)
    std::vector<std::string> window_explanations;

    /// Rescale slope
    double rescale_slope{1.0};

    /// Rescale intercept
    double rescale_intercept{0.0};

    /// Create a success result
    static voi_lut_info ok() {
        voi_lut_info r;
        r.success = true;
        return r;
    }

    /// Create an error result
    static voi_lut_info error(std::string message) {
        voi_lut_info r;
        r.success = false;
        r.error_message = std::move(message);
        return r;
    }
};

/**
 * @brief Multi-frame information
 */
struct frame_info {
    /// Whether the operation succeeded
    bool success{false};

    /// Error message if failed
    std::string error_message;

    /// Total number of frames
    uint32_t total_frames{1};

    /// Frame time in milliseconds (for cine)
    std::optional<double> frame_time;

    /// Frame rate (frames per second)
    std::optional<double> frame_rate;

    /// Image rows
    uint16_t rows{0};

    /// Image columns
    uint16_t columns{0};

    /// Create a success result
    static frame_info ok() {
        frame_info r;
        r.success = true;
        return r;
    }

    /// Create an error result
    static frame_info error(std::string message) {
        frame_info r;
        r.success = false;
        r.error_message = std::move(message);
        return r;
    }
};

/**
 * @class metadata_service
 * @brief Service for selective metadata retrieval and series navigation
 *
 * Provides APIs for:
 * - Selective DICOM tag retrieval with presets
 * - Series instance sorting and navigation
 * - Window/Level preset management
 * - Multi-frame information
 */
class metadata_service {
public:
    /**
     * @brief Construct metadata service with database
     * @param database Index database for instance lookups
     */
    explicit metadata_service(
        std::shared_ptr<storage::index_database> database);

    /// Destructor
    ~metadata_service();

    /// Non-copyable, non-movable
    metadata_service(const metadata_service&) = delete;
    metadata_service& operator=(const metadata_service&) = delete;
    metadata_service(metadata_service&&) = delete;
    metadata_service& operator=(metadata_service&&) = delete;

    // =========================================================================
    // Selective Metadata Retrieval
    // =========================================================================

    /**
     * @brief Get selective metadata for an instance
     * @param sop_instance_uid SOP Instance UID
     * @param request Metadata request parameters
     * @return Metadata response with requested tags
     */
    [[nodiscard]] metadata_response get_metadata(
        std::string_view sop_instance_uid,
        const metadata_request& request);

    /**
     * @brief Get tags for a specific preset
     * @param preset The metadata preset
     * @return Set of DICOM tags (hex format) for the preset
     */
    [[nodiscard]] static std::unordered_set<std::string> get_preset_tags(
        metadata_preset preset);

    // =========================================================================
    // Series Navigation
    // =========================================================================

    /**
     * @brief Get sorted instances for a series
     * @param series_uid Series Instance UID
     * @param order Sort order
     * @param ascending Sort direction
     * @return Sorted instances response
     */
    [[nodiscard]] sorted_instances_response get_sorted_instances(
        std::string_view series_uid,
        sort_order order = sort_order::position,
        bool ascending = true);

    /**
     * @brief Get navigation info for an instance
     * @param sop_instance_uid SOP Instance UID
     * @return Navigation info with previous/next instance UIDs
     */
    [[nodiscard]] navigation_info get_navigation(
        std::string_view sop_instance_uid);

    // =========================================================================
    // Window/Level Presets
    // =========================================================================

    /**
     * @brief Get window/level presets for a modality
     * @param modality Modality code (CT, MR, etc.)
     * @return Vector of window/level presets
     */
    [[nodiscard]] static std::vector<window_level_preset>
    get_window_level_presets(std::string_view modality);

    /**
     * @brief Get VOI LUT info from an instance
     * @param sop_instance_uid SOP Instance UID
     * @return VOI LUT information
     */
    [[nodiscard]] voi_lut_info get_voi_lut(std::string_view sop_instance_uid);

    // =========================================================================
    // Multi-frame Support
    // =========================================================================

    /**
     * @brief Get frame information for an instance
     * @param sop_instance_uid SOP Instance UID
     * @return Frame information
     */
    [[nodiscard]] frame_info get_frame_info(std::string_view sop_instance_uid);

private:
    /// Database for instance lookups
    std::shared_ptr<storage::index_database> database_;

    /**
     * @brief Read DICOM dataset from file
     * @param file_path Path to DICOM file
     * @return Map of tag values
     */
    [[nodiscard]] std::unordered_map<std::string, std::string> read_dicom_tags(
        std::string_view file_path,
        const std::unordered_set<std::string>& requested_tags,
        bool include_private);

    /**
     * @brief Get series UID for an instance
     * @param sop_instance_uid SOP Instance UID
     * @return Series UID if found
     */
    [[nodiscard]] std::optional<std::string> get_series_uid(
        std::string_view sop_instance_uid);
};

}  // namespace pacs::web
