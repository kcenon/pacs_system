// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file http_client.h
 * @brief libcurl-backed HTTPS POST client for ITI-41 transport.
 *
 * A thin RAII wrapper over libcurl handles (easy and slist) that POSTs a
 * precomputed request body to a configured endpoint and returns the
 * response body and HTTP status.
 *
 * This is an internal header.
 */

#pragma once

#include <kcenon/common/patterns/result.h>
#include <kcenon/pacs/ihe/xds/errors.h>
#include <kcenon/pacs/ihe/xds/http_options.h>

#include <functional>
#include <string>

namespace kcenon::pacs::ihe::xds::detail {

/**
 * @brief HTTP response captured from a POST call.
 */
struct http_response {
    long status_code{0};
    std::string body;
};

/**
 * @brief Optional test hook: replaces the real libcurl transport.
 *
 * When set via set_http_transport_override, the HTTP client returns the
 * override's Result instead of invoking libcurl. The override receives
 * the fully-formed request body, content-type and endpoint so a test can
 * assert on them without having to mock a TCP server.
 */
struct transport_request {
    std::string url;
    std::string content_type;
    std::string soap_action;
    std::string body;
    bool verify_peer{true};
};

using transport_override =
    std::function<kcenon::common::Result<http_response>(const transport_request&)>;

void set_http_transport_override(transport_override fn);
void clear_http_transport_override();

/**
 * @brief POST @p body to @p opts.endpoint with the given Content-Type.
 *
 * Adds a SOAPAction header, a User-Agent, and forwards the TLS settings
 * from @p opts. Returns transport_curl_failed / transport_tls_error /
 * transport_http_error with context in the message.
 */
kcenon::common::Result<http_response> http_post(
    const http_options& opts, const std::string& content_type,
    const std::string& body);

}  // namespace kcenon::pacs::ihe::xds::detail
