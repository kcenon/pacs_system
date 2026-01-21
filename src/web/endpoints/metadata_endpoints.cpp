/**
 * @file metadata_endpoints.cpp
 * @brief Metadata REST API endpoints implementation
 *
 * @see Issue #544 - Implement Selective Metadata & Navigation APIs
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

#include "pacs/storage/index_database.hpp"
#include "pacs/web/endpoints/metadata_endpoints.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/metadata_service.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

#include <memory>
#include <sstream>

namespace pacs::web::endpoints {

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
 * @brief Escape string for JSON
 */
std::string escape_json(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"':
                oss << "\\\"";
                break;
            case '\\':
                oss << "\\\\";
                break;
            case '\b':
                oss << "\\b";
                break;
            case '\f':
                oss << "\\f";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            default:
                oss << c;
                break;
        }
    }
    return oss.str();
}

/**
 * @brief Parse metadata request from query parameters
 */
metadata_request parse_metadata_request(const crow::request& req) {
    metadata_request request;

    // Parse tags parameter (comma-separated)
    auto tags_param = req.url_params.get("tags");
    if (tags_param != nullptr) {
        std::string tags_str = tags_param;
        std::istringstream iss(tags_str);
        std::string tag;
        while (std::getline(iss, tag, ',')) {
            // Trim whitespace
            size_t start = tag.find_first_not_of(" \t");
            size_t end = tag.find_last_not_of(" \t");
            if (start != std::string::npos) {
                request.tags.push_back(tag.substr(start, end - start + 1));
            }
        }
    }

    // Parse preset parameter
    auto preset_param = req.url_params.get("preset");
    if (preset_param != nullptr) {
        request.preset = preset_from_string(preset_param);
    }

    // Parse include_private parameter
    auto private_param = req.url_params.get("include_private");
    if (private_param != nullptr) {
        std::string val = private_param;
        request.include_private = (val == "true" || val == "1");
    }

    return request;
}

/**
 * @brief Convert metadata response to JSON
 */
std::string metadata_response_to_json(const metadata_response& resp) {
    std::ostringstream oss;
    oss << R"({"tags":{)";

    bool first = true;
    for (const auto& [tag, value] : resp.tags) {
        if (!first) {
            oss << ",";
        }
        first = false;

        // Check if value looks numeric
        bool is_numeric = !value.empty();
        for (char c : value) {
            if (!std::isdigit(c) && c != '.' && c != '-' && c != '+' &&
                c != 'e' && c != 'E' && c != '\\') {
                is_numeric = false;
                break;
            }
        }

        // Multi-valued numeric fields contain backslash separators
        if (is_numeric && value.find('\\') != std::string::npos) {
            // Output as array
            oss << "\"" << tag << "\":[";
            std::istringstream vals(value);
            std::string v;
            bool first_val = true;
            while (std::getline(vals, v, '\\')) {
                if (!first_val) {
                    oss << ",";
                }
                first_val = false;
                oss << v;
            }
            oss << "]";
        } else if (is_numeric && value.find('\\') == std::string::npos) {
            oss << "\"" << tag << "\":" << value;
        } else {
            oss << "\"" << tag << "\":\"" << escape_json(value) << "\"";
        }
    }

    oss << "}}";
    return oss.str();
}

/**
 * @brief Convert sorted instances response to JSON
 */
std::string sorted_instances_to_json(const sorted_instances_response& resp) {
    std::ostringstream oss;
    oss << R"({"instances":[)";

    bool first = true;
    for (const auto& inst : resp.instances) {
        if (!first) {
            oss << ",";
        }
        first = false;

        oss << "{\"sop_instance_uid\":\"" << inst.sop_instance_uid << "\"";

        if (inst.instance_number.has_value()) {
            oss << ",\"instance_number\":" << inst.instance_number.value();
        }

        if (inst.slice_location.has_value()) {
            oss << ",\"slice_location\":" << inst.slice_location.value();
        }

        oss << "}";
    }

    oss << "],\"total\":" << resp.total << "}";
    return oss.str();
}

/**
 * @brief Convert navigation info to JSON
 */
std::string navigation_info_to_json(const navigation_info& nav) {
    std::ostringstream oss;
    oss << "{";

    if (!nav.previous.empty()) {
        oss << "\"previous\":\"" << nav.previous << "\",";
    }

    if (!nav.next.empty()) {
        oss << "\"next\":\"" << nav.next << "\",";
    }

    oss << "\"index\":" << nav.index << ",\"total\":" << nav.total
        << ",\"first\":\"" << nav.first << "\",\"last\":\"" << nav.last << "\"}";

    return oss.str();
}

/**
 * @brief Convert window level presets to JSON
 */
std::string presets_to_json(const std::vector<window_level_preset>& presets) {
    std::ostringstream oss;
    oss << R"({"presets":[)";

    bool first = true;
    for (const auto& p : presets) {
        if (!first) {
            oss << ",";
        }
        first = false;

        oss << "{\"name\":\"" << escape_json(p.name) << "\",\"center\":"
            << p.center << ",\"width\":" << p.width << "}";
    }

    oss << "]}";
    return oss.str();
}

/**
 * @brief Convert VOI LUT info to JSON
 */
std::string voi_lut_to_json(const voi_lut_info& info) {
    std::ostringstream oss;
    oss << "{\"window_center\":[";

    for (size_t i = 0; i < info.window_center.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << info.window_center[i];
    }

    oss << "],\"window_width\":[";
    for (size_t i = 0; i < info.window_width.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << info.window_width[i];
    }

    oss << "],\"window_explanations\":[";
    for (size_t i = 0; i < info.window_explanations.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << "\"" << escape_json(info.window_explanations[i]) << "\"";
    }

    oss << "],\"rescale_slope\":" << info.rescale_slope
        << ",\"rescale_intercept\":" << info.rescale_intercept << "}";

    return oss.str();
}

/**
 * @brief Convert frame info to JSON
 */
std::string frame_info_to_json(const frame_info& info) {
    std::ostringstream oss;
    oss << "{\"total_frames\":" << info.total_frames;

    if (info.frame_time.has_value()) {
        oss << ",\"frame_time\":" << info.frame_time.value();
    }

    if (info.frame_rate.has_value()) {
        oss << ",\"frame_rate\":" << info.frame_rate.value();
    }

    oss << ",\"rows\":" << info.rows << ",\"columns\":" << info.columns << "}";

    return oss.str();
}

/// Shared metadata service instance
std::shared_ptr<metadata_service> g_metadata_service;

}  // namespace

// Internal implementation function called from rest_server.cpp
void register_metadata_endpoints_impl(crow::SimpleApp& app,
                                       std::shared_ptr<rest_server_context> ctx) {
    // Initialize metadata service if database is available
    if (ctx->database != nullptr && g_metadata_service == nullptr) {
        g_metadata_service = std::make_shared<metadata_service>(ctx->database);
    }

    // =========================================================================
    // Selective Metadata
    // =========================================================================

    // GET /api/v1/instances/{sopInstanceUid}/metadata
    CROW_ROUTE(app, "/api/v1/instances/<string>/metadata")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& sop_uid) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (g_metadata_service == nullptr) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Metadata service not configured");
                    return res;
                }

                auto request = parse_metadata_request(req);
                auto result = g_metadata_service->get_metadata(sop_uid, request);

                if (!result.success) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", result.error_message);
                    return res;
                }

                res.code = 200;
                res.body = metadata_response_to_json(result);
                return res;
            });

    // =========================================================================
    // Series Navigation
    // =========================================================================

    // GET /api/v1/series/{seriesUid}/instances/sorted
    CROW_ROUTE(app, "/api/v1/series/<string>/instances/sorted")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& series_uid) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (g_metadata_service == nullptr) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Metadata service not configured");
                    return res;
                }

                // Parse sort parameters
                sort_order order = sort_order::position;
                auto sort_param = req.url_params.get("sort_by");
                if (sort_param != nullptr) {
                    auto parsed = sort_order_from_string(sort_param);
                    if (parsed.has_value()) {
                        order = parsed.value();
                    }
                }

                bool ascending = true;
                auto dir_param = req.url_params.get("direction");
                if (dir_param != nullptr) {
                    std::string dir = dir_param;
                    ascending = (dir != "desc");
                }

                auto result = g_metadata_service->get_sorted_instances(
                    series_uid, order, ascending);

                if (!result.success) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", result.error_message);
                    return res;
                }

                res.code = 200;
                res.body = sorted_instances_to_json(result);
                return res;
            });

    // GET /api/v1/instances/{sopInstanceUid}/navigation
    CROW_ROUTE(app, "/api/v1/instances/<string>/navigation")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& /*req*/, const std::string& sop_uid) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (g_metadata_service == nullptr) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Metadata service not configured");
                    return res;
                }

                auto result = g_metadata_service->get_navigation(sop_uid);

                if (!result.success) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", result.error_message);
                    return res;
                }

                res.code = 200;
                res.body = navigation_info_to_json(result);
                return res;
            });

    // =========================================================================
    // Window/Level Presets
    // =========================================================================

    // GET /api/v1/presets/window-level
    CROW_ROUTE(app, "/api/v1/presets/window-level")
        .methods(crow::HTTPMethod::GET)([ctx](const crow::request& req) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            std::string modality = "CT";  // Default modality
            auto modality_param = req.url_params.get("modality");
            if (modality_param != nullptr) {
                modality = modality_param;
            }

            auto presets = metadata_service::get_window_level_presets(modality);

            res.code = 200;
            res.body = presets_to_json(presets);
            return res;
        });

    // GET /api/v1/instances/{sopInstanceUid}/voi-lut
    CROW_ROUTE(app, "/api/v1/instances/<string>/voi-lut")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& /*req*/, const std::string& sop_uid) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (g_metadata_service == nullptr) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Metadata service not configured");
                    return res;
                }

                auto result = g_metadata_service->get_voi_lut(sop_uid);

                if (!result.success) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", result.error_message);
                    return res;
                }

                res.code = 200;
                res.body = voi_lut_to_json(result);
                return res;
            });

    // =========================================================================
    // Multi-frame Support
    // =========================================================================

    // GET /api/v1/instances/{sopInstanceUid}/frame-info
    CROW_ROUTE(app, "/api/v1/instances/<string>/frame-info")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& /*req*/, const std::string& sop_uid) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (g_metadata_service == nullptr) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Metadata service not configured");
                    return res;
                }

                auto result = g_metadata_service->get_frame_info(sop_uid);

                if (!result.success) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", result.error_message);
                    return res;
                }

                res.code = 200;
                res.body = frame_info_to_json(result);
                return res;
            });
}

}  // namespace pacs::web::endpoints
