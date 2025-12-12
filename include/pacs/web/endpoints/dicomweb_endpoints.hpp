/**
 * @file dicomweb_endpoints.hpp
 * @brief DICOMweb (WADO-RS) API endpoints for REST server
 *
 * This file provides the DICOMweb endpoints for retrieving DICOM objects
 * over HTTP following the WADO-RS (Web Access to DICOM Objects - RESTful)
 * specification as defined in DICOM PS3.18.
 *
 * @see DICOM PS3.18 - Web Services
 * @see https://www.dicomstandard.org/dicomweb
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::core {
class dicom_dataset;
} // namespace pacs::core

namespace pacs::storage {
class index_database;
struct study_record;
struct series_record;
struct instance_record;
struct study_query;
struct series_query;
struct instance_query;
} // namespace pacs::storage

namespace pacs::web {

struct rest_server_context;

namespace dicomweb {

/**
 * @brief Media types supported by WADO-RS
 */
struct media_type {
    static constexpr std::string_view dicom = "application/dicom";
    static constexpr std::string_view dicom_json = "application/dicom+json";
    static constexpr std::string_view dicom_xml = "application/dicom+xml";
    static constexpr std::string_view octet_stream = "application/octet-stream";
    static constexpr std::string_view jpeg = "image/jpeg";
    static constexpr std::string_view png = "image/png";
    static constexpr std::string_view multipart_related = "multipart/related";
};

/**
 * @brief Parsed Accept header information
 */
struct accept_info {
    std::string media_type;
    std::string transfer_syntax;
    float quality = 1.0f;
};

/**
 * @brief Parse Accept header into structured format
 * @param accept_header The raw Accept header value
 * @return Vector of parsed accept info, sorted by quality
 */
[[nodiscard]] auto parse_accept_header(std::string_view accept_header)
    -> std::vector<accept_info>;

/**
 * @brief Check if a media type is acceptable based on Accept header
 * @param accept_infos Parsed accept header
 * @param media_type The media type to check
 * @return true if acceptable
 */
[[nodiscard]] auto is_acceptable(const std::vector<accept_info>& accept_infos,
                                  std::string_view media_type) -> bool;

/**
 * @brief Builder for multipart MIME responses
 *
 * Generates multipart/related responses as required by WADO-RS
 * for returning multiple DICOM objects.
 */
class multipart_builder {
public:
    /**
     * @brief Construct a multipart builder
     * @param content_type The content type for parts (default: application/dicom)
     */
    explicit multipart_builder(
        std::string_view content_type = media_type::dicom);

    /**
     * @brief Add a part to the multipart response
     * @param data The binary data for this part
     * @param content_type Optional override content type for this part
     */
    void add_part(std::vector<uint8_t> data,
                  std::optional<std::string_view> content_type = std::nullopt);

    /**
     * @brief Add a part with location header
     * @param data The binary data for this part
     * @param location The Content-Location header value
     * @param content_type Optional override content type for this part
     */
    void add_part_with_location(
        std::vector<uint8_t> data,
        std::string_view location,
        std::optional<std::string_view> content_type = std::nullopt);

    /**
     * @brief Build the complete multipart response body
     * @return The multipart response body as string
     */
    [[nodiscard]] auto build() const -> std::string;

    /**
     * @brief Get the Content-Type header value for this multipart response
     * @return Complete Content-Type header including boundary
     */
    [[nodiscard]] auto content_type_header() const -> std::string;

    /**
     * @brief Get the boundary string
     * @return The boundary string
     */
    [[nodiscard]] auto boundary() const -> std::string_view;

    /**
     * @brief Check if any parts have been added
     * @return true if at least one part exists
     */
    [[nodiscard]] auto empty() const noexcept -> bool;

    /**
     * @brief Get the number of parts
     * @return Number of parts added
     */
    [[nodiscard]] auto size() const noexcept -> size_t;

private:
    struct part {
        std::vector<uint8_t> data;
        std::string content_type;
        std::string location;
    };

    std::string boundary_;
    std::string default_content_type_;
    std::vector<part> parts_;

    /**
     * @brief Generate a unique boundary string
     * @return Boundary string
     */
    [[nodiscard]] static auto generate_boundary() -> std::string;
};

/**
 * @brief Convert a DICOM dataset to DicomJSON format
 * @param dataset The DICOM dataset to convert
 * @param include_bulk_data Whether to include bulk data inline
 * @param bulk_data_uri_prefix URI prefix for bulk data references
 * @return DicomJSON string
 */
[[nodiscard]] auto dataset_to_dicom_json(
    const core::dicom_dataset& dataset,
    bool include_bulk_data = false,
    std::string_view bulk_data_uri_prefix = "") -> std::string;

/**
 * @brief Convert a VR type code to DicomJSON VR string
 * @param vr_code The VR type code
 * @return VR string (e.g., "PN", "LO", "UI")
 */
[[nodiscard]] auto vr_to_string(uint16_t vr_code) -> std::string;

/**
 * @brief Check if a DICOM tag contains bulk data
 * @param tag The DICOM tag
 * @return true if this is a bulk data tag
 */
[[nodiscard]] auto is_bulk_data_tag(uint32_t tag) -> bool;

// ============================================================================
// STOW-RS Support (Store Over the Web)
// ============================================================================

/**
 * @brief Parsed part from multipart request
 *
 * Represents a single part extracted from a multipart/related request body.
 */
struct multipart_part {
    std::string content_type;       ///< Content-Type of this part
    std::string content_location;   ///< Content-Location header (optional)
    std::string content_id;         ///< Content-ID header (optional)
    std::vector<uint8_t> data;      ///< Binary data of this part
};

/**
 * @brief Parser for multipart/related request bodies
 *
 * Parses incoming multipart/related requests as used by STOW-RS
 * for uploading DICOM instances.
 *
 * @par Example
 * @code
 * auto parts = multipart_parser::parse(content_type_header, request_body);
 * for (const auto& part : parts) {
 *     if (part.content_type == "application/dicom") {
 *         // Process DICOM data
 *     }
 * }
 * @endcode
 */
class multipart_parser {
public:
    /**
     * @brief Parse error information
     */
    struct parse_error {
        std::string code;       ///< Error code (e.g., "INVALID_BOUNDARY")
        std::string message;    ///< Human-readable error message
    };

    /**
     * @brief Parse result - either parts or error
     */
    struct parse_result {
        std::vector<multipart_part> parts;  ///< Parsed parts (empty on error)
        std::optional<parse_error> error;   ///< Error if parsing failed

        [[nodiscard]] bool success() const noexcept { return !error.has_value(); }
        [[nodiscard]] explicit operator bool() const noexcept { return success(); }
    };

    /**
     * @brief Parse a multipart/related request body
     * @param content_type The Content-Type header value (must include boundary)
     * @param body The raw request body
     * @return Parse result containing parts or error
     */
    [[nodiscard]] static auto parse(std::string_view content_type,
                                    std::string_view body) -> parse_result;

    /**
     * @brief Extract boundary from Content-Type header
     * @param content_type The Content-Type header value
     * @return Boundary string or nullopt if not found
     */
    [[nodiscard]] static auto extract_boundary(std::string_view content_type)
        -> std::optional<std::string>;

    /**
     * @brief Extract type parameter from Content-Type header
     * @param content_type The Content-Type header value
     * @return Type value or nullopt if not found
     */
    [[nodiscard]] static auto extract_type(std::string_view content_type)
        -> std::optional<std::string>;

private:
    /**
     * @brief Parse headers from a part's header section
     * @param header_section The raw header section
     * @return Map of header name to value
     */
    [[nodiscard]] static auto parse_part_headers(std::string_view header_section)
        -> std::vector<std::pair<std::string, std::string>>;
};

/**
 * @brief STOW-RS store result for a single instance
 */
struct store_instance_result {
    bool success = false;                   ///< Whether storage succeeded
    std::string sop_class_uid;              ///< SOP Class UID of the instance
    std::string sop_instance_uid;           ///< SOP Instance UID of the instance
    std::string retrieve_url;               ///< URL to retrieve the stored instance
    std::optional<std::string> error_code;  ///< Error code if failed
    std::optional<std::string> error_message; ///< Error message if failed
};

/**
 * @brief STOW-RS overall store response
 */
struct store_response {
    std::vector<store_instance_result> referenced_instances;  ///< Successfully stored
    std::vector<store_instance_result> failed_instances;      ///< Failed to store

    [[nodiscard]] bool all_success() const noexcept {
        return failed_instances.empty() && !referenced_instances.empty();
    }

    [[nodiscard]] bool all_failed() const noexcept {
        return referenced_instances.empty() && !failed_instances.empty();
    }

    [[nodiscard]] bool partial_success() const noexcept {
        return !referenced_instances.empty() && !failed_instances.empty();
    }
};

/**
 * @brief Validation result for DICOM instance
 */
struct validation_result {
    bool valid = true;                      ///< Whether validation passed
    std::string error_code;                 ///< Error code if invalid
    std::string error_message;              ///< Error message if invalid

    [[nodiscard]] explicit operator bool() const noexcept { return valid; }

    static validation_result ok() { return {true, "", ""}; }
    static validation_result error(std::string code, std::string message) {
        return {false, std::move(code), std::move(message)};
    }
};

/**
 * @brief Validate a DICOM instance for STOW-RS storage
 * @param dataset The DICOM dataset to validate
 * @param target_study_uid Optional target study UID (for existing study)
 * @return Validation result
 */
[[nodiscard]] auto validate_instance(
    const core::dicom_dataset& dataset,
    std::optional<std::string_view> target_study_uid = std::nullopt)
    -> validation_result;

/**
 * @brief Build STOW-RS response in DicomJSON format
 * @param response The store response
 * @param base_url Base URL for retrieve URLs
 * @return DicomJSON response body
 */
[[nodiscard]] auto build_store_response_json(
    const store_response& response,
    std::string_view base_url) -> std::string;

// ============================================================================
// QIDO-RS Support (Query based on ID for DICOM Objects)
// ============================================================================

/**
 * @brief Convert a study record to DicomJSON format for QIDO-RS response
 * @param record The study record to convert
 * @param patient_id The patient ID for this study
 * @param patient_name The patient name for this study
 * @return DicomJSON string for the study
 */
[[nodiscard]] auto study_record_to_dicom_json(
    const storage::study_record& record,
    std::string_view patient_id,
    std::string_view patient_name) -> std::string;

/**
 * @brief Convert a series record to DicomJSON format for QIDO-RS response
 * @param record The series record to convert
 * @param study_uid The Study Instance UID for this series
 * @return DicomJSON string for the series
 */
[[nodiscard]] auto series_record_to_dicom_json(
    const storage::series_record& record,
    std::string_view study_uid) -> std::string;

/**
 * @brief Convert an instance record to DicomJSON format for QIDO-RS response
 * @param record The instance record to convert
 * @param series_uid The Series Instance UID for this instance
 * @param study_uid The Study Instance UID for this instance
 * @return DicomJSON string for the instance
 */
[[nodiscard]] auto instance_record_to_dicom_json(
    const storage::instance_record& record,
    std::string_view series_uid,
    std::string_view study_uid) -> std::string;

/**
 * @brief Parse QIDO-RS query parameters from HTTP request
 * @param url_params The URL query parameters
 * @return Study query parameters
 */
[[nodiscard]] auto parse_study_query_params(
    const std::string& url_params) -> storage::study_query;

/**
 * @brief Parse QIDO-RS series query parameters from HTTP request
 * @param url_params The URL query parameters
 * @return Series query parameters
 */
[[nodiscard]] auto parse_series_query_params(
    const std::string& url_params) -> storage::series_query;

/**
 * @brief Parse QIDO-RS instance query parameters from HTTP request
 * @param url_params The URL query parameters
 * @return Instance query parameters
 */
[[nodiscard]] auto parse_instance_query_params(
    const std::string& url_params) -> storage::instance_query;

// ============================================================================
// Frame Retrieval (WADO-RS)
// ============================================================================

/**
 * @brief Parse frame numbers from URL path
 * @param frame_list Comma-separated frame numbers (e.g., "1,3,5" or "1-5")
 * @return Vector of frame numbers (1-based), empty on parse error
 *
 * @par Examples
 * - "1" -> {1}
 * - "1,3,5" -> {1, 3, 5}
 * - "1-5" -> {1, 2, 3, 4, 5}
 * - "1,3-5,7" -> {1, 3, 4, 5, 7}
 */
[[nodiscard]] auto parse_frame_numbers(std::string_view frame_list)
    -> std::vector<uint32_t>;

/**
 * @brief Extract a single frame from pixel data
 * @param pixel_data Complete pixel data buffer
 * @param frame_number Frame number to extract (1-based)
 * @param frame_size Size of each frame in bytes
 * @return Frame data, or empty vector if frame doesn't exist
 */
[[nodiscard]] auto extract_frame(
    std::span<const uint8_t> pixel_data,
    uint32_t frame_number,
    size_t frame_size) -> std::vector<uint8_t>;

// ============================================================================
// Rendered Images (WADO-RS)
// ============================================================================

/**
 * @brief Rendered image output format
 */
enum class rendered_format {
    jpeg,   ///< JPEG format (default)
    png     ///< PNG format
};

/**
 * @brief Parameters for rendered image requests
 */
struct rendered_params {
    /// Output format (jpeg or png)
    rendered_format format{rendered_format::jpeg};

    /// JPEG quality (1-100, default 75)
    int quality{75};

    /// Window center (default: auto from DICOM or calculated)
    std::optional<double> window_center;

    /// Window width (default: auto from DICOM or calculated)
    std::optional<double> window_width;

    /// Output viewport width (0 = original size)
    uint16_t viewport_width{0};

    /// Output viewport height (0 = original size)
    uint16_t viewport_height{0};

    /// Frame number for multi-frame images (1-based, default 1)
    uint32_t frame{1};

    /// Presentation state SOP Instance UID (optional)
    std::optional<std::string> presentation_state_uid;

    /// Annotation (burned-in or removed)
    bool burn_annotations{false};
};

/**
 * @brief Parse rendered image parameters from HTTP request
 * @param query_string The URL query string
 * @param accept_header The Accept header value
 * @return Parsed rendered parameters
 */
[[nodiscard]] auto parse_rendered_params(
    std::string_view query_string,
    std::string_view accept_header) -> rendered_params;

/**
 * @brief Result of rendered image operation
 */
struct rendered_result {
    std::vector<uint8_t> data;      ///< Encoded image data
    std::string content_type;       ///< MIME type (image/jpeg or image/png)
    bool success{false};            ///< Operation succeeded
    std::string error_message;      ///< Error message if failed

    [[nodiscard]] static rendered_result ok(
        std::vector<uint8_t> d, std::string_view mime_type) {
        return {std::move(d), std::string(mime_type), true, ""};
    }

    [[nodiscard]] static rendered_result error(std::string msg) {
        return {{}, "", false, std::move(msg)};
    }
};

/**
 * @brief Apply window/level transformation to pixel data
 * @param pixel_data Raw pixel values (16-bit)
 * @param width Image width
 * @param height Image height
 * @param bits_stored Bits stored per pixel
 * @param is_signed Whether pixel values are signed
 * @param window_center Window center value
 * @param window_width Window width value
 * @param rescale_slope Rescale slope (default 1.0)
 * @param rescale_intercept Rescale intercept (default 0.0)
 * @return 8-bit grayscale image data
 */
[[nodiscard]] auto apply_window_level(
    std::span<const uint8_t> pixel_data,
    uint16_t width,
    uint16_t height,
    uint16_t bits_stored,
    bool is_signed,
    double window_center,
    double window_width,
    double rescale_slope = 1.0,
    double rescale_intercept = 0.0) -> std::vector<uint8_t>;

/**
 * @brief Render a DICOM image to JPEG or PNG
 * @param file_path Path to DICOM file
 * @param params Rendering parameters
 * @return Rendered image result
 */
[[nodiscard]] auto render_dicom_image(
    std::string_view file_path,
    const rendered_params& params) -> rendered_result;

} // namespace dicomweb

namespace endpoints {

// Internal function - implementation in cpp file
// Registers DICOMweb endpoints with the Crow app
// Called from rest_server.cpp

} // namespace endpoints

} // namespace pacs::web
