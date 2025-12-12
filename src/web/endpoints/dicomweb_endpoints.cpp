/**
 * @file dicomweb_endpoints.cpp
 * @brief DICOMweb (WADO-RS) API endpoints implementation
 *
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

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_element.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/storage/index_database.hpp"
#include "pacs/storage/instance_record.hpp"
#include "pacs/storage/series_record.hpp"
#include "pacs/storage/study_record.hpp"
#include "pacs/web/endpoints/dicomweb_endpoints.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

namespace pacs::web::dicomweb {

namespace {

/**
 * @brief Trim whitespace from string
 */
std::string trim(std::string_view s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return "";
    }
    auto end = s.find_last_not_of(" \t\r\n");
    return std::string(s.substr(start, end - start + 1));
}

/**
 * @brief Split string by delimiter
 */
std::vector<std::string> split(std::string_view s, char delimiter) {
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = s.find(delimiter);

    while (end != std::string_view::npos) {
        result.push_back(std::string(s.substr(start, end - start)));
        start = end + 1;
        end = s.find(delimiter, start);
    }
    result.push_back(std::string(s.substr(start)));
    return result;
}

/**
 * @brief Convert VR enum to string representation
 */
std::string vr_enum_to_string(pacs::encoding::vr_type vr) {
    using vr_type = pacs::encoding::vr_type;
    switch (vr) {
    case vr_type::AE: return "AE";
    case vr_type::AS: return "AS";
    case vr_type::AT: return "AT";
    case vr_type::CS: return "CS";
    case vr_type::DA: return "DA";
    case vr_type::DS: return "DS";
    case vr_type::DT: return "DT";
    case vr_type::FD: return "FD";
    case vr_type::FL: return "FL";
    case vr_type::IS: return "IS";
    case vr_type::LO: return "LO";
    case vr_type::LT: return "LT";
    case vr_type::OB: return "OB";
    case vr_type::OD: return "OD";
    case vr_type::OF: return "OF";
    case vr_type::OL: return "OL";
    case vr_type::OW: return "OW";
    case vr_type::PN: return "PN";
    case vr_type::SH: return "SH";
    case vr_type::SL: return "SL";
    case vr_type::SQ: return "SQ";
    case vr_type::SS: return "SS";
    case vr_type::ST: return "ST";
    case vr_type::TM: return "TM";
    case vr_type::UC: return "UC";
    case vr_type::UI: return "UI";
    case vr_type::UL: return "UL";
    case vr_type::UN: return "UN";
    case vr_type::UR: return "UR";
    case vr_type::US: return "US";
    case vr_type::UT: return "UT";
    default: return "UN";
    }
}

} // anonymous namespace

// ============================================================================
// Accept Header Parsing
// ============================================================================

auto parse_accept_header(std::string_view accept_header)
    -> std::vector<accept_info> {
    std::vector<accept_info> result;

    if (accept_header.empty()) {
        // Default to application/dicom
        result.push_back({std::string(media_type::dicom), "", 1.0f});
        return result;
    }

    auto parts = split(accept_header, ',');
    for (const auto& part : parts) {
        accept_info info;
        auto params = split(part, ';');

        if (params.empty()) {
            continue;
        }

        info.media_type = trim(params[0]);

        for (size_t i = 1; i < params.size(); ++i) {
            auto param = trim(params[i]);
            if (param.starts_with("q=")) {
                try {
                    info.quality = std::stof(param.substr(2));
                } catch (...) {
                    info.quality = 1.0f;
                }
            } else if (param.starts_with("transfer-syntax=")) {
                info.transfer_syntax = param.substr(16);
                // Remove quotes if present
                if (!info.transfer_syntax.empty() &&
                    info.transfer_syntax.front() == '"') {
                    info.transfer_syntax = info.transfer_syntax.substr(
                        1, info.transfer_syntax.size() - 2);
                }
            }
        }

        result.push_back(std::move(info));
    }

    // Sort by quality (descending)
    std::sort(result.begin(), result.end(),
              [](const accept_info& a, const accept_info& b) {
                  return a.quality > b.quality;
              });

    return result;
}

auto is_acceptable(const std::vector<accept_info>& accept_infos,
                   std::string_view media_type) -> bool {
    if (accept_infos.empty()) {
        return true; // Accept all if no Accept header
    }

    for (const auto& info : accept_infos) {
        if (info.media_type == "*/*" ||
            info.media_type == media_type ||
            (info.media_type.ends_with("/*") &&
             media_type.starts_with(
                 info.media_type.substr(0, info.media_type.size() - 1)))) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Multipart Builder Implementation
// ============================================================================

multipart_builder::multipart_builder(std::string_view content_type)
    : boundary_(generate_boundary()),
      default_content_type_(content_type) {}

void multipart_builder::add_part(
    std::vector<uint8_t> data,
    std::optional<std::string_view> content_type) {
    part p;
    p.data = std::move(data);
    p.content_type = content_type.has_value()
                         ? std::string(*content_type)
                         : default_content_type_;
    parts_.push_back(std::move(p));
}

void multipart_builder::add_part_with_location(
    std::vector<uint8_t> data,
    std::string_view location,
    std::optional<std::string_view> content_type) {
    part p;
    p.data = std::move(data);
    p.content_type = content_type.has_value()
                         ? std::string(*content_type)
                         : default_content_type_;
    p.location = std::string(location);
    parts_.push_back(std::move(p));
}

auto multipart_builder::build() const -> std::string {
    std::ostringstream oss;

    for (const auto& p : parts_) {
        oss << "--" << boundary_ << "\r\n";
        oss << "Content-Type: " << p.content_type << "\r\n";
        if (!p.location.empty()) {
            oss << "Content-Location: " << p.location << "\r\n";
        }
        oss << "\r\n";
        oss.write(reinterpret_cast<const char*>(p.data.data()),
                  static_cast<std::streamsize>(p.data.size()));
        oss << "\r\n";
    }

    if (!parts_.empty()) {
        oss << "--" << boundary_ << "--\r\n";
    }

    return oss.str();
}

auto multipart_builder::content_type_header() const -> std::string {
    std::ostringstream oss;
    oss << "multipart/related; type=\"" << default_content_type_
        << "\"; boundary=" << boundary_;
    return oss.str();
}

auto multipart_builder::boundary() const -> std::string_view {
    return boundary_;
}

auto multipart_builder::empty() const noexcept -> bool {
    return parts_.empty();
}

auto multipart_builder::size() const noexcept -> size_t {
    return parts_.size();
}

auto multipart_builder::generate_boundary() -> std::string {
    // Generate a unique boundary using timestamp and random number
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                         now.time_since_epoch())
                         .count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999999);

    std::ostringstream oss;
    oss << "----=_Part_" << timestamp << "_" << dis(gen);
    return oss.str();
}

// ============================================================================
// DicomJSON Conversion
// ============================================================================

auto is_bulk_data_tag(uint32_t tag) -> bool {
    // Pixel Data and related bulk data tags
    uint16_t group = static_cast<uint16_t>(tag >> 16);

    // Audio Sample Data is in group 0x50xx (curve data)
    if (group >= 0x5000 && group <= 0x50FF) {
        uint16_t element = static_cast<uint16_t>(tag & 0xFFFF);
        if (element == 0x3000) {
            return true;  // Audio Sample Data
        }
    }

    return tag == 0x7FE00010 ||  // Pixel Data
           tag == 0x7FE00008 ||  // Float Pixel Data
           tag == 0x7FE00009 ||  // Double Float Pixel Data
           tag == 0x00420011 ||  // Encapsulated Document
           tag == 0x00660023 ||  // Triangle Point Index List
           tag == 0x00660024 ||  // Edge Point Index List
           tag == 0x00660025 ||  // Vertex Point Index List
           tag == 0x00660026 ||  // Triangle Strip Sequence
           tag == 0x00660027 ||  // Triangle Fan Sequence
           tag == 0x00660028 ||  // Line Sequence
           tag == 0x00660029;    // Primitive Point Index List
}

auto vr_to_string(uint16_t vr_code) -> std::string {
    // Convert 2-byte VR code to string
    char vr[3] = {static_cast<char>(vr_code & 0xFF),
                  static_cast<char>((vr_code >> 8) & 0xFF),
                  '\0'};
    return std::string(vr);
}

auto dataset_to_dicom_json(
    const core::dicom_dataset& dataset,
    bool include_bulk_data,
    std::string_view bulk_data_uri_prefix) -> std::string {
    std::ostringstream oss;
    oss << "{";

    bool first = true;
    // Iterate using begin()/end() - dataset is iterable as map<dicom_tag, dicom_element>
    for (const auto& [tag_key, elem] : dataset) {
        if (!first) {
            oss << ",";
        }
        first = false;

        // Format tag as 8-digit hex
        uint32_t tag = tag_key.combined();
        oss << "\"";
        oss << std::hex << std::setfill('0') << std::setw(8) << tag;
        oss << std::dec << "\":{";

        // Add VR
        std::string vr_str = vr_enum_to_string(elem.vr());
        oss << "\"vr\":\"" << vr_str << "\"";

        // Check if bulk data
        if (is_bulk_data_tag(tag) && !include_bulk_data) {
            if (!bulk_data_uri_prefix.empty()) {
                oss << ",\"BulkDataURI\":\"" << bulk_data_uri_prefix
                    << std::hex << std::setfill('0') << std::setw(8) << tag
                    << std::dec << "\"";
            }
        } else {
            // Add Value based on VR type
            auto value_str = elem.as_string();
            if (!value_str.empty()) {
                oss << ",\"Value\":[";

                // Handle different VR types
                using vr_type = pacs::encoding::vr_type;
                switch (elem.vr()) {
                case vr_type::PN:
                    // Person Name - object with Alphabetic component
                    oss << "{\"Alphabetic\":\"" << json_escape(value_str) << "\"}";
                    break;

                case vr_type::IS:
                case vr_type::SL:
                case vr_type::SS:
                case vr_type::UL:
                case vr_type::US:
                    // Integer types - output as number
                    oss << value_str;
                    break;

                case vr_type::DS:
                case vr_type::FL:
                case vr_type::FD:
                    // Decimal types - output as number
                    oss << value_str;
                    break;

                default:
                    // String types
                    oss << "\"" << json_escape(value_str) << "\"";
                    break;
                }

                oss << "]";
            }
        }

        oss << "}";
    }

    oss << "}";
    return oss.str();
}

} // namespace pacs::web::dicomweb

namespace pacs::web::endpoints {

namespace {

/**
 * @brief Add CORS headers to response
 */
void add_cors_headers(crow::response& res, const rest_server_context& ctx) {
    if (ctx.config && !ctx.config->cors_allowed_origins.empty()) {
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
 * @brief Build retrieval response for multiple DICOM files
 */
crow::response build_multipart_dicom_response(
    const std::vector<std::string>& file_paths,
    const rest_server_context& ctx,
    std::string_view base_uri = "") {

    crow::response res;
    add_cors_headers(res, ctx);

    if (file_paths.empty()) {
        res.code = 404;
        res.add_header("Content-Type", "application/json");
        res.body = make_error_json("NOT_FOUND", "No instances found");
        return res;
    }

    // Single file - return directly
    if (file_paths.size() == 1) {
        auto data = read_file_bytes(file_paths[0]);
        if (data.empty()) {
            res.code = 500;
            res.add_header("Content-Type", "application/json");
            res.body = make_error_json("READ_ERROR", "Failed to read DICOM file");
            return res;
        }

        res.code = 200;
        res.add_header("Content-Type", std::string(dicomweb::media_type::dicom));
        res.body = std::string(reinterpret_cast<char*>(data.data()), data.size());
        return res;
    }

    // Multiple files - use multipart response
    dicomweb::multipart_builder builder(dicomweb::media_type::dicom);

    for (size_t i = 0; i < file_paths.size(); ++i) {
        auto data = read_file_bytes(file_paths[i]);
        if (data.empty()) {
            continue; // Skip files that can't be read
        }

        if (base_uri.empty()) {
            builder.add_part(std::move(data));
        } else {
            std::ostringstream location;
            location << base_uri << "/" << i;
            builder.add_part_with_location(std::move(data), location.str());
        }
    }

    if (builder.empty()) {
        res.code = 500;
        res.add_header("Content-Type", "application/json");
        res.body = make_error_json("READ_ERROR", "Failed to read DICOM files");
        return res;
    }

    res.code = 200;
    res.add_header("Content-Type", builder.content_type_header());
    res.body = builder.build();
    return res;
}

/**
 * @brief Build metadata response (DicomJSON)
 */
crow::response build_metadata_response(
    const std::vector<std::string>& file_paths,
    const rest_server_context& ctx,
    std::string_view bulk_data_uri_prefix = "") {

    crow::response res;
    add_cors_headers(res, ctx);
    res.add_header("Content-Type", std::string(dicomweb::media_type::dicom_json));

    if (file_paths.empty()) {
        res.code = 404;
        res.body = make_error_json("NOT_FOUND", "No instances found");
        return res;
    }

    std::ostringstream oss;
    oss << "[";

    bool first = true;
    for (const auto& path : file_paths) {
        auto result = core::dicom_file::open(path);
        if (!result) {
            continue;
        }

        if (!first) {
            oss << ",";
        }
        first = false;

        oss << dicomweb::dataset_to_dicom_json(
            result->dataset(), false, bulk_data_uri_prefix);
    }

    oss << "]";

    res.code = 200;
    res.body = oss.str();
    return res;
}

} // anonymous namespace

void register_dicomweb_endpoints_impl(crow::SimpleApp& app,
                                      std::shared_ptr<rest_server_context> ctx) {
    // ========================================================================
    // Study Retrieval
    // ========================================================================

    // GET /dicomweb/studies/{studyUID} - Retrieve entire study
    CROW_ROUTE(app, "/dicomweb/studies/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                // Check Accept header
                auto accept = req.get_header_value("Accept");
                auto accept_infos = dicomweb::parse_accept_header(accept);

                // Check if metadata is requested
                if (dicomweb::is_acceptable(accept_infos,
                                            dicomweb::media_type::dicom_json)) {
                    auto files = ctx->database->get_study_files(study_uid);
                    std::string bulk_uri = "/dicomweb/studies/" + study_uid +
                                           "/bulkdata/";
                    return build_metadata_response(files, *ctx, bulk_uri);
                }

                // Return DICOM instances
                auto files = ctx->database->get_study_files(study_uid);
                std::string base_uri = "/dicomweb/studies/" + study_uid;
                return build_multipart_dicom_response(files, *ctx, base_uri);
            });

    // GET /dicomweb/studies/{studyUID}/metadata - Study metadata
    CROW_ROUTE(app, "/dicomweb/studies/<string>/metadata")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& /*req*/, const std::string& study_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                auto files = ctx->database->get_study_files(study_uid);
                std::string bulk_uri = "/dicomweb/studies/" + study_uid +
                                       "/bulkdata/";
                return build_metadata_response(files, *ctx, bulk_uri);
            });

    // ========================================================================
    // Series Retrieval
    // ========================================================================

    // GET /dicomweb/studies/{studyUID}/series/{seriesUID} - Retrieve series
    CROW_ROUTE(app, "/dicomweb/studies/<string>/series/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req,
                  const std::string& study_uid,
                  const std::string& series_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                // Verify study exists
                auto study = ctx->database->find_study(study_uid);
                if (!study) {
                    res.code = 404;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("NOT_FOUND", "Study not found");
                    return res;
                }

                // Check Accept header
                auto accept = req.get_header_value("Accept");
                auto accept_infos = dicomweb::parse_accept_header(accept);

                // Check if metadata is requested
                if (dicomweb::is_acceptable(accept_infos,
                                            dicomweb::media_type::dicom_json)) {
                    auto files = ctx->database->get_series_files(series_uid);
                    std::string bulk_uri = "/dicomweb/studies/" + study_uid +
                                           "/series/" + series_uid + "/bulkdata/";
                    return build_metadata_response(files, *ctx, bulk_uri);
                }

                auto files = ctx->database->get_series_files(series_uid);
                std::string base_uri = "/dicomweb/studies/" + study_uid +
                                       "/series/" + series_uid;
                return build_multipart_dicom_response(files, *ctx, base_uri);
            });

    // GET /dicomweb/studies/{studyUID}/series/{seriesUID}/metadata
    CROW_ROUTE(app, "/dicomweb/studies/<string>/series/<string>/metadata")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& /*req*/,
                  const std::string& study_uid,
                  const std::string& series_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                auto files = ctx->database->get_series_files(series_uid);
                std::string bulk_uri = "/dicomweb/studies/" + study_uid +
                                       "/series/" + series_uid + "/bulkdata/";
                return build_metadata_response(files, *ctx, bulk_uri);
            });

    // ========================================================================
    // Instance Retrieval
    // ========================================================================

    // GET /dicomweb/studies/{studyUID}/series/{seriesUID}/instances/{sopUID}
    CROW_ROUTE(app,
               "/dicomweb/studies/<string>/series/<string>/instances/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req,
                  const std::string& study_uid,
                  const std::string& series_uid,
                  const std::string& sop_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                auto file_path = ctx->database->get_file_path(sop_uid);
                if (!file_path) {
                    res.code = 404;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("NOT_FOUND", "Instance not found");
                    return res;
                }

                // Check Accept header
                auto accept = req.get_header_value("Accept");
                auto accept_infos = dicomweb::parse_accept_header(accept);

                // Check if metadata is requested
                if (dicomweb::is_acceptable(accept_infos,
                                            dicomweb::media_type::dicom_json)) {
                    std::vector<std::string> files = {*file_path};
                    std::string bulk_uri = "/dicomweb/studies/" + study_uid +
                                           "/series/" + series_uid +
                                           "/instances/" + sop_uid + "/bulkdata/";
                    return build_metadata_response(files, *ctx, bulk_uri);
                }

                // Return DICOM instance
                auto data = read_file_bytes(*file_path);
                if (data.empty()) {
                    res.code = 500;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("READ_ERROR",
                                               "Failed to read DICOM file");
                    return res;
                }

                res.code = 200;
                res.add_header("Content-Type",
                               std::string(dicomweb::media_type::dicom));
                res.body = std::string(reinterpret_cast<char*>(data.data()),
                                       data.size());
                return res;
            });

    // GET /dicomweb/studies/{studyUID}/series/{seriesUID}/instances/{sopUID}/metadata
    CROW_ROUTE(app,
               "/dicomweb/studies/<string>/series/<string>/instances/<string>/metadata")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& /*req*/,
                  const std::string& study_uid,
                  const std::string& series_uid,
                  const std::string& sop_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                auto file_path = ctx->database->get_file_path(sop_uid);
                if (!file_path) {
                    res.code = 404;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("NOT_FOUND", "Instance not found");
                    return res;
                }

                std::vector<std::string> files = {*file_path};
                std::string bulk_uri = "/dicomweb/studies/" + study_uid +
                                       "/series/" + series_uid +
                                       "/instances/" + sop_uid + "/bulkdata/";
                return build_metadata_response(files, *ctx, bulk_uri);
            });

    // ========================================================================
    // CORS Preflight Handler for DICOMweb
    // ========================================================================

    CROW_ROUTE(app, "/dicomweb/<path>")
        .methods(crow::HTTPMethod::OPTIONS)(
            [ctx](const crow::request& /*req*/, const std::string& /*path*/) {
                crow::response res(204);
                if (ctx->config) {
                    res.add_header("Access-Control-Allow-Origin",
                                   ctx->config->cors_allowed_origins);
                }
                res.add_header("Access-Control-Allow-Methods", "GET, OPTIONS");
                res.add_header("Access-Control-Allow-Headers",
                               "Content-Type, Accept, Authorization");
                res.add_header("Access-Control-Max-Age", "86400");
                return res;
            });
}

} // namespace pacs::web::endpoints
