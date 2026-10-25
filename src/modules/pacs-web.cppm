// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-web.cppm
 * @brief C++20 module partition for REST API and DICOMweb.
 *
 * This module partition exports web-related types:
 * - rest_server: HTTP/REST server
 * - rest_server_config: Server configuration
 * - http_status, api_error: Request/Response types
 * - DICOMweb: WADO-RS, STOW-RS, QIDO-RS support
 *
 * The exports below exist only when pacs_web is built
 * (PACS_MODULES_WITH_WEB is defined by cmake/targets.cmake);
 * otherwise this partition is empty.
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_MODULES_WITH_WEB
// PACS web headers
#include <kcenon/pacs/web/rest_server.h>
#include <kcenon/pacs/web/rest_config.h>
#include <kcenon/pacs/web/rest_types.h>
#include <kcenon/pacs/web/endpoints/dicomweb_endpoints.h>
#endif // PACS_MODULES_WITH_WEB

export module kcenon.pacs:web;

#ifdef PACS_MODULES_WITH_WEB

// ============================================================================
// Re-export kcenon::pacs::web namespace
// ============================================================================

export namespace kcenon::pacs::web {

// REST server
using kcenon::pacs::web::rest_server;
using kcenon::pacs::web::rest_server_config;

// REST types
using kcenon::pacs::web::http_status;
using kcenon::pacs::web::api_error;
using kcenon::pacs::web::to_json;
using kcenon::pacs::web::make_error_json;
using kcenon::pacs::web::make_success_json;
using kcenon::pacs::web::json_escape;

} // namespace kcenon::pacs::web

// ============================================================================
// Re-export kcenon::pacs::web::dicomweb namespace
// ============================================================================

export namespace kcenon::pacs::web::dicomweb {

// Media types
using kcenon::pacs::web::dicomweb::media_type;

// Accept header parsing
using kcenon::pacs::web::dicomweb::accept_info;
using kcenon::pacs::web::dicomweb::parse_accept_header;
using kcenon::pacs::web::dicomweb::is_acceptable;

// Multipart response builder
using kcenon::pacs::web::dicomweb::multipart_builder;

// DicomJSON conversion
using kcenon::pacs::web::dicomweb::dataset_to_dicom_json;
using kcenon::pacs::web::dicomweb::vr_to_string;
using kcenon::pacs::web::dicomweb::is_bulk_data_tag;

// STOW-RS support
using kcenon::pacs::web::dicomweb::multipart_part;
using kcenon::pacs::web::dicomweb::multipart_parser;
using kcenon::pacs::web::dicomweb::store_instance_result;
using kcenon::pacs::web::dicomweb::store_response;
using kcenon::pacs::web::dicomweb::validation_result;
using kcenon::pacs::web::dicomweb::validate_instance;
using kcenon::pacs::web::dicomweb::build_store_response_json;

// QIDO-RS support
using kcenon::pacs::web::dicomweb::study_record_to_dicom_json;
using kcenon::pacs::web::dicomweb::series_record_to_dicom_json;
using kcenon::pacs::web::dicomweb::instance_record_to_dicom_json;
using kcenon::pacs::web::dicomweb::parse_study_query_params;
using kcenon::pacs::web::dicomweb::parse_series_query_params;
using kcenon::pacs::web::dicomweb::parse_instance_query_params;

// Frame retrieval (WADO-RS)
using kcenon::pacs::web::dicomweb::parse_frame_numbers;
using kcenon::pacs::web::dicomweb::extract_frame;

// Rendered images (WADO-RS)
using kcenon::pacs::web::dicomweb::rendered_format;
using kcenon::pacs::web::dicomweb::rendered_params;
using kcenon::pacs::web::dicomweb::rendered_result;
using kcenon::pacs::web::dicomweb::parse_rendered_params;
using kcenon::pacs::web::dicomweb::apply_window_level;
using kcenon::pacs::web::dicomweb::render_dicom_image;

} // namespace kcenon::pacs::web::dicomweb

#endif // PACS_MODULES_WITH_WEB
