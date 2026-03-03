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
 * @file wado_uri_endpoints.cpp
 * @brief WADO-URI (Web Access to DICOM Objects — URI-based) endpoint implementation
 *
 * @see DICOM PS3.18 Section 6.2 — WADO-URI
 * @see Issue #798 - Add WADO-URI legacy web access support
 * @copyright Copyright (c) 2025
 * @license MIT
 */

// IMPORTANT: Include Crow FIRST before any PACS headers to avoid forward
// declaration conflicts
#include "crow.h"

// Workaround for Windows: DELETE is defined as a macro in <winnt.h>
// which conflicts with crow::HTTPMethod::DELETE
#ifdef DELETE
#undef DELETE
#endif

#include "pacs/core/dicom_file.hpp"
#include "pacs/storage/index_database.hpp"
#include "pacs/web/endpoints/dicomweb_endpoints.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/endpoints/wado_uri_endpoints.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace pacs::web {

namespace wado_uri {

wado_uri_request parse_wado_uri_params(
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
    const char* frame_number) {

    wado_uri_request request;

    if (study_uid != nullptr) {
        request.study_uid = study_uid;
    }
    if (series_uid != nullptr) {
        request.series_uid = series_uid;
    }
    if (object_uid != nullptr) {
        request.object_uid = object_uid;
    }
    if (content_type != nullptr && content_type[0] != '\0') {
        request.content_type = content_type;
    }
    if (transfer_syntax != nullptr && transfer_syntax[0] != '\0') {
        request.transfer_syntax = std::string(transfer_syntax);
    }
    if (anonymize != nullptr) {
        std::string anon_str(anonymize);
        request.anonymize = (anon_str == "yes" || anon_str == "true"
                             || anon_str == "1");
    }
    if (rows != nullptr) {
        try {
            int val = std::stoi(rows);
            if (val > 0 && val <= 65535) {
                request.rows = static_cast<uint16_t>(val);
            }
        } catch (...) {
            // Ignore invalid values
        }
    }
    if (columns != nullptr) {
        try {
            int val = std::stoi(columns);
            if (val > 0 && val <= 65535) {
                request.columns = static_cast<uint16_t>(val);
            }
        } catch (...) {
            // Ignore invalid values
        }
    }
    if (window_center != nullptr) {
        try {
            request.window_center = std::stod(window_center);
        } catch (...) {
            // Ignore invalid values
        }
    }
    if (window_width != nullptr) {
        try {
            request.window_width = std::stod(window_width);
        } catch (...) {
            // Ignore invalid values
        }
    }
    if (frame_number != nullptr) {
        try {
            int val = std::stoi(frame_number);
            if (val > 0) {
                request.frame_number = static_cast<uint32_t>(val);
            }
        } catch (...) {
            // Ignore invalid values
        }
    }

    return request;
}

validation_result validate_wado_uri_request(const wado_uri_request& request) {
    if (request.study_uid.empty()) {
        return validation_result::error(
            400, "MISSING_PARAMETER", "studyUID parameter is required");
    }
    if (request.series_uid.empty()) {
        return validation_result::error(
            400, "MISSING_PARAMETER", "seriesUID parameter is required");
    }
    if (request.object_uid.empty()) {
        return validation_result::error(
            400, "MISSING_PARAMETER", "objectUID parameter is required");
    }
    if (!is_supported_content_type(request.content_type)) {
        return validation_result::error(
            406, "UNSUPPORTED_MEDIA_TYPE",
            "Unsupported contentType: " + request.content_type);
    }
    return validation_result::ok();
}

bool is_supported_content_type(std::string_view content_type) {
    return content_type == "application/dicom"
        || content_type == "image/jpeg"
        || content_type == "image/png"
        || content_type == "application/dicom+json";
}

}  // namespace wado_uri

namespace endpoints {

namespace {

/**
 * @brief Add CORS headers to response
 */
void add_cors_headers(crow::response& res, const rest_server_context& ctx) {
    if (ctx.config != nullptr && !ctx.config->cors_allowed_origins.empty()) {
        res.add_header("Access-Control-Allow-Origin",
                       ctx.config->cors_allowed_origins);
    }
}

/**
 * @brief Read file contents into byte vector
 */
std::vector<uint8_t> read_file_bytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return {};
    }

    return buffer;
}

/**
 * @brief Handle application/dicom content type — return raw DICOM Part 10 file
 */
crow::response handle_dicom_response(
    const std::string& file_path,
    const rest_server_context& ctx) {
    crow::response res;
    add_cors_headers(res, ctx);

    auto data = read_file_bytes(file_path);
    if (data.empty()) {
        res.code = 500;
        res.add_header("Content-Type", "application/json");
        res.body = make_error_json("READ_ERROR", "Failed to read DICOM file");
        return res;
    }

    res.code = 200;
    res.add_header("Content-Type", "application/dicom");
    res.body = std::string(reinterpret_cast<char*>(data.data()), data.size());
    return res;
}

/**
 * @brief Handle image content types (JPEG, PNG) — return rendered image
 */
crow::response handle_rendered_response(
    const std::string& file_path,
    const wado_uri::wado_uri_request& request,
    const rest_server_context& ctx) {
    crow::response res;
    add_cors_headers(res, ctx);

    dicomweb::rendered_params params;

    if (request.content_type == "image/png") {
        params.format = dicomweb::rendered_format::png;
    } else {
        params.format = dicomweb::rendered_format::jpeg;
    }

    params.window_center = request.window_center;
    params.window_width = request.window_width;

    if (request.rows.has_value()) {
        params.viewport_height = request.rows.value();
    }
    if (request.columns.has_value()) {
        params.viewport_width = request.columns.value();
    }
    if (request.frame_number.has_value()) {
        params.frame = request.frame_number.value();
    }

    auto result = dicomweb::render_dicom_image(file_path, params);

    if (!result.success) {
        res.code = 400;
        res.add_header("Content-Type", "application/json");
        res.body = make_error_json("RENDER_ERROR", result.error_message);
        return res;
    }

    res.code = 200;
    res.add_header("Content-Type", result.content_type);
    res.body = std::string(
        reinterpret_cast<char*>(result.data.data()),
        result.data.size());
    return res;
}

/**
 * @brief Handle application/dicom+json content type — return DicomJSON metadata
 */
crow::response handle_dicom_json_response(
    const std::string& file_path,
    const rest_server_context& ctx) {
    crow::response res;
    add_cors_headers(res, ctx);

    auto data = read_file_bytes(file_path);
    if (data.empty()) {
        res.code = 500;
        res.add_header("Content-Type", "application/json");
        res.body = make_error_json("READ_ERROR", "Failed to read DICOM file");
        return res;
    }

    auto dicom_result = core::dicom_file::from_bytes(
        std::span<const uint8_t>(data.data(), data.size()));
    if (dicom_result.is_err()) {
        res.code = 500;
        res.add_header("Content-Type", "application/json");
        res.body = make_error_json("PARSE_ERROR", "Failed to parse DICOM file");
        return res;
    }

    auto json = dicomweb::dataset_to_dicom_json(
        dicom_result.value().dataset(), false, "");

    res.code = 200;
    res.add_header("Content-Type", "application/dicom+json");
    res.body = "[" + json + "]";
    return res;
}

}  // anonymous namespace

void register_wado_uri_endpoints_impl(
    crow::SimpleApp& app,
    std::shared_ptr<rest_server_context> ctx) {

    // GET /wado?requestType=WADO&studyUID=...&seriesUID=...&objectUID=...
    CROW_ROUTE(app, "/wado")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req) {
                crow::response res;
                add_cors_headers(res, *ctx);

                // Validate requestType parameter
                auto request_type = req.url_params.get("requestType");
                if (request_type == nullptr
                    || std::string(request_type) != "WADO") {
                    res.code = 400;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json(
                        "INVALID_REQUEST_TYPE",
                        "requestType must be 'WADO'");
                    return res;
                }

                // Check database availability
                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json(
                        "DATABASE_UNAVAILABLE", "Database not configured");
                    return res;
                }

                // Parse and validate parameters
                auto wado_request = wado_uri::parse_wado_uri_params(
                    req.url_params.get("studyUID"),
                    req.url_params.get("seriesUID"),
                    req.url_params.get("objectUID"),
                    req.url_params.get("contentType"),
                    req.url_params.get("transferSyntax"),
                    req.url_params.get("anonymize"),
                    req.url_params.get("rows"),
                    req.url_params.get("columns"),
                    req.url_params.get("windowCenter"),
                    req.url_params.get("windowWidth"),
                    req.url_params.get("frameNumber"));

                auto validation = wado_uri::validate_wado_uri_request(
                    wado_request);
                if (!validation.valid) {
                    res.code = validation.http_status;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json(
                        validation.error_code, validation.error_message);
                    return res;
                }

                // Look up the DICOM instance
                auto file_path_result = ctx->database->get_file_path(
                    wado_request.object_uid);
                if (!file_path_result.is_ok()) {
                    res.code = 500;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json(
                        "QUERY_ERROR", file_path_result.error().message);
                    return res;
                }
                const auto& file_path = file_path_result.value();
                if (!file_path) {
                    res.code = 404;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json(
                        "NOT_FOUND", "DICOM instance not found");
                    return res;
                }

                // Route to appropriate handler based on content type
                if (wado_request.content_type == "application/dicom") {
                    return handle_dicom_response(*file_path, *ctx);
                }
                if (wado_request.content_type == "image/jpeg"
                    || wado_request.content_type == "image/png") {
                    return handle_rendered_response(
                        *file_path, wado_request, *ctx);
                }
                if (wado_request.content_type == "application/dicom+json") {
                    return handle_dicom_json_response(*file_path, *ctx);
                }

                // Should not reach here due to validation, but handle defensively
                res.code = 406;
                res.add_header("Content-Type", "application/json");
                res.body = make_error_json(
                    "UNSUPPORTED_MEDIA_TYPE",
                    "Unsupported contentType: " + wado_request.content_type);
                return res;
            });
}

}  // namespace endpoints

}  // namespace pacs::web
