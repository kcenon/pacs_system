/**
 * @file thumbnail_endpoints.cpp
 * @brief Thumbnail REST API endpoints implementation
 *
 * @see Issue #543 - Implement Thumbnail API for DICOM Viewer
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
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/endpoints/thumbnail_endpoints.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"
#include "pacs/web/thumbnail_service.hpp"

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
 * @brief Parse thumbnail parameters from request
 * @param req The HTTP request
 * @return Parsed thumbnail parameters
 */
thumbnail_params parse_thumbnail_params(const crow::request& req) {
    thumbnail_params params;

    // Parse size
    auto size_param = req.url_params.get("size");
    if (size_param != nullptr) {
        try {
            int size = std::stoi(size_param);
            if (size == 64 || size == 128 || size == 256 || size == 512) {
                params.size = static_cast<uint16_t>(size);
            }
        } catch (...) {
            // Use default
        }
    }

    // Parse format
    auto format_param = req.url_params.get("format");
    if (format_param != nullptr) {
        std::string format = format_param;
        if (format == "jpeg" || format == "png") {
            params.format = format;
        }
    }

    // Parse quality
    auto quality_param = req.url_params.get("quality");
    if (quality_param != nullptr) {
        try {
            int quality = std::stoi(quality_param);
            if (quality >= 1 && quality <= 100) {
                params.quality = quality;
            }
        } catch (...) {
            // Use default
        }
    }

    // Parse frame
    auto frame_param = req.url_params.get("frame");
    if (frame_param != nullptr) {
        try {
            int frame = std::stoi(frame_param);
            if (frame >= 1) {
                params.frame = static_cast<uint32_t>(frame);
            }
        } catch (...) {
            // Use default
        }
    }

    return params;
}

/// Shared thumbnail service instance
std::shared_ptr<thumbnail_service> g_thumbnail_service;

}  // namespace

// Internal implementation function called from rest_server.cpp
void register_thumbnail_endpoints_impl(crow::SimpleApp& app,
                                        std::shared_ptr<rest_server_context> ctx) {
    // Initialize thumbnail service if database is available
    if (ctx->database != nullptr && g_thumbnail_service == nullptr) {
        g_thumbnail_service =
            std::make_shared<thumbnail_service>(ctx->database);
    }

    // GET /api/v1/thumbnails/instances/{sopInstanceUid}
    CROW_ROUTE(app, "/api/v1/thumbnails/instances/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& sop_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (g_thumbnail_service == nullptr) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Thumbnail service not configured");
                    return res;
                }

                auto params = parse_thumbnail_params(req);
                auto result = g_thumbnail_service->get_thumbnail(sop_uid, params);

                if (!result.success) {
                    res.code = 404;
                    res.add_header("Content-Type", "application/json");
                    res.body =
                        make_error_json("NOT_FOUND", result.error_message);
                    return res;
                }

                res.code = 200;
                res.add_header("Content-Type", result.entry.content_type);
                res.add_header("Cache-Control", "max-age=3600");
                res.body = std::string(
                    reinterpret_cast<const char*>(result.entry.data.data()),
                    result.entry.data.size());
                return res;
            });

    // GET /api/v1/thumbnails/series/{seriesUid}
    CROW_ROUTE(app, "/api/v1/thumbnails/series/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& series_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (g_thumbnail_service == nullptr) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Thumbnail service not configured");
                    return res;
                }

                auto params = parse_thumbnail_params(req);
                auto result =
                    g_thumbnail_service->get_series_thumbnail(series_uid, params);

                if (!result.success) {
                    res.code = 404;
                    res.add_header("Content-Type", "application/json");
                    res.body =
                        make_error_json("NOT_FOUND", result.error_message);
                    return res;
                }

                res.code = 200;
                res.add_header("Content-Type", result.entry.content_type);
                res.add_header("Cache-Control", "max-age=3600");
                res.body = std::string(
                    reinterpret_cast<const char*>(result.entry.data.data()),
                    result.entry.data.size());
                return res;
            });

    // GET /api/v1/thumbnails/studies/{studyUid}
    CROW_ROUTE(app, "/api/v1/thumbnails/studies/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (g_thumbnail_service == nullptr) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Thumbnail service not configured");
                    return res;
                }

                auto params = parse_thumbnail_params(req);
                auto result =
                    g_thumbnail_service->get_study_thumbnail(study_uid, params);

                if (!result.success) {
                    res.code = 404;
                    res.add_header("Content-Type", "application/json");
                    res.body =
                        make_error_json("NOT_FOUND", result.error_message);
                    return res;
                }

                res.code = 200;
                res.add_header("Content-Type", result.entry.content_type);
                res.add_header("Cache-Control", "max-age=3600");
                res.body = std::string(
                    reinterpret_cast<const char*>(result.entry.data.data()),
                    result.entry.data.size());
                return res;
            });

    // DELETE /api/v1/thumbnails/cache - Clear all cached thumbnails
    CROW_ROUTE(app, "/api/v1/thumbnails/cache")
        .methods(crow::HTTPMethod::DELETE)([ctx](const crow::request& /*req*/) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (g_thumbnail_service == nullptr) {
                res.code = 503;
                res.body = make_error_json("SERVICE_UNAVAILABLE",
                                           "Thumbnail service not configured");
                return res;
            }

            g_thumbnail_service->clear_cache();

            res.code = 200;
            res.body = make_success_json("Cache cleared successfully");
            return res;
        });

    // GET /api/v1/thumbnails/cache/stats - Get cache statistics
    CROW_ROUTE(app, "/api/v1/thumbnails/cache/stats")
        .methods(crow::HTTPMethod::GET)([ctx](const crow::request& /*req*/) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (g_thumbnail_service == nullptr) {
                res.code = 503;
                res.body = make_error_json("SERVICE_UNAVAILABLE",
                                           "Thumbnail service not configured");
                return res;
            }

            std::ostringstream oss;
            oss << R"({"cache_size":)" << g_thumbnail_service->cache_size()
                << R"(,"entry_count":)" << g_thumbnail_service->cache_entry_count()
                << R"(,"max_size":)" << g_thumbnail_service->max_cache_size()
                << "}";

            res.code = 200;
            res.body = oss.str();
            return res;
        });
}

}  // namespace pacs::web::endpoints
