// BSD 3-Clause License
// Copyright (c) 2025, kcenon
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

// PACS web headers
#include <pacs/web/rest_server.hpp>
#include <pacs/web/rest_config.hpp>
#include <pacs/web/rest_types.hpp>
#include <pacs/web/endpoints/dicomweb_endpoints.hpp>

export module kcenon.pacs:web;

// ============================================================================
// Re-export pacs::web namespace
// ============================================================================

export namespace pacs::web {

// REST server
using pacs::web::rest_server;
using pacs::web::rest_server_config;

// REST types
using pacs::web::http_status;
using pacs::web::api_error;
using pacs::web::to_json;
using pacs::web::make_error_json;
using pacs::web::make_success_json;
using pacs::web::json_escape;

} // namespace pacs::web

// ============================================================================
// Re-export pacs::web::dicomweb namespace
// ============================================================================

export namespace pacs::web::dicomweb {

// Media types
using pacs::web::dicomweb::media_type;

// Accept header parsing
using pacs::web::dicomweb::accept_info;
using pacs::web::dicomweb::parse_accept_header;
using pacs::web::dicomweb::is_acceptable;

// Multipart response builder
using pacs::web::dicomweb::multipart_builder;

// DicomJSON conversion
using pacs::web::dicomweb::dataset_to_dicom_json;
using pacs::web::dicomweb::vr_to_string;
using pacs::web::dicomweb::is_bulk_data_tag;

// STOW-RS support
using pacs::web::dicomweb::multipart_part;
using pacs::web::dicomweb::multipart_parser;
using pacs::web::dicomweb::store_instance_result;
using pacs::web::dicomweb::store_response;
using pacs::web::dicomweb::validation_result;
using pacs::web::dicomweb::validate_instance;
using pacs::web::dicomweb::build_store_response_json;

// QIDO-RS support
using pacs::web::dicomweb::study_record_to_dicom_json;
using pacs::web::dicomweb::series_record_to_dicom_json;
using pacs::web::dicomweb::instance_record_to_dicom_json;
using pacs::web::dicomweb::parse_study_query_params;
using pacs::web::dicomweb::parse_series_query_params;
using pacs::web::dicomweb::parse_instance_query_params;

// Frame retrieval (WADO-RS)
using pacs::web::dicomweb::parse_frame_numbers;
using pacs::web::dicomweb::extract_frame;

// Rendered images (WADO-RS)
using pacs::web::dicomweb::rendered_format;
using pacs::web::dicomweb::rendered_params;
using pacs::web::dicomweb::rendered_result;
using pacs::web::dicomweb::parse_rendered_params;
using pacs::web::dicomweb::apply_window_level;
using pacs::web::dicomweb::render_dicom_image;

} // namespace pacs::web::dicomweb
