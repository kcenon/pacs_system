// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file wado_uri_endpoints.hpp
 * @brief WADO-URI (Web Access to DICOM Objects — URI-based) API endpoints
 *
 * Implements the legacy WADO-URI retrieval mechanism as defined in
 * DICOM PS3.18 Section 6.2. Provides HTTP GET-based access to DICOM
 * objects using query parameters.
 *
 * @see DICOM PS3.18 Section 6.2 — WADO-URI
 * @see Issue #798 - Add WADO-URI legacy web access support
 * @copyright Copyright (c) 2025
 * @license MIT
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace pacs::web {

struct rest_server_context;

namespace wado_uri {

/**
 * @brief Parsed WADO-URI request parameters
 *
 * Represents the query parameters from a WADO-URI HTTP GET request:
 * GET /wado?requestType=WADO&studyUID=...&seriesUID=...&objectUID=...
 */
struct wado_uri_request {
    /// Study Instance UID (required)
    std::string study_uid;

    /// Series Instance UID (required)
    std::string series_uid;

    /// SOP Instance UID (required)
    std::string object_uid;

    /// Requested content type (default: application/dicom)
    std::string content_type{"application/dicom"};

    /// Transfer Syntax UID for transcoding (optional)
    std::optional<std::string> transfer_syntax;

    /// Whether to anonymize the response (optional)
    bool anonymize{false};

    /// Output viewport rows (optional, for rendered images)
    std::optional<uint16_t> rows;

    /// Output viewport columns (optional, for rendered images)
    std::optional<uint16_t> columns;

    /// Window center for rendered images (optional)
    std::optional<double> window_center;

    /// Window width for rendered images (optional)
    std::optional<double> window_width;

    /// Frame number for multi-frame images (1-based, optional)
    std::optional<uint32_t> frame_number;
};

/**
 * @brief Result of WADO-URI request validation
 */
struct validation_result {
    bool valid{true};
    int http_status{200};
    std::string error_code;
    std::string error_message;

    [[nodiscard]] static validation_result ok() { return {true, 200, "", ""}; }
    [[nodiscard]] static validation_result error(int status,
                                                  std::string code,
                                                  std::string message) {
        return {false, status, std::move(code), std::move(message)};
    }
};

/**
 * @brief Parse WADO-URI query parameters from an HTTP request
 * @param request_type The requestType parameter value
 * @param study_uid The studyUID parameter value
 * @param series_uid The seriesUID parameter value
 * @param object_uid The objectUID parameter value
 * @param content_type The contentType parameter value (optional)
 * @param transfer_syntax The transferSyntax parameter value (optional)
 * @param anonymize The anonymize parameter value (optional)
 * @param rows The rows parameter value (optional)
 * @param columns The columns parameter value (optional)
 * @param window_center The windowCenter parameter value (optional)
 * @param window_width The windowWidth parameter value (optional)
 * @param frame_number The frameNumber parameter value (optional)
 * @return Parsed WADO-URI request parameters
 */
[[nodiscard]] wado_uri_request parse_wado_uri_params(
    const char* study_uid,
    const char* series_uid,
    const char* object_uid,
    const char* content_type,
    const char* transfer_syntax,
    const char* anonymize,
    const char* rows,
    const char* columns,
    const char* window_center,
    const char* window_width,
    const char* frame_number);

/**
 * @brief Validate a WADO-URI request
 * @param request The parsed request parameters
 * @return Validation result
 */
[[nodiscard]] validation_result validate_wado_uri_request(
    const wado_uri_request& request);

/**
 * @brief Check if a content type is supported by WADO-URI
 * @param content_type The content type to check
 * @return true if the content type is supported
 */
[[nodiscard]] bool is_supported_content_type(std::string_view content_type);

}  // namespace wado_uri

namespace endpoints {

// Internal function - implementation in cpp file
// Registers WADO-URI endpoints with the Crow app
// Called from rest_server.cpp

}  // namespace endpoints

}  // namespace pacs::web
