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
#include <string>
#include <string_view>
#include <vector>

namespace pacs::core {
class dicom_dataset;
} // namespace pacs::core

namespace pacs::storage {
class index_database;
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

} // namespace dicomweb

namespace endpoints {

// Internal function - implementation in cpp file
// Registers DICOMweb endpoints with the Crow app
// Called from rest_server.cpp

} // namespace endpoints

} // namespace pacs::web
