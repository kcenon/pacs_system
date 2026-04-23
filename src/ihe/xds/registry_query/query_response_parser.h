// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file query_response_parser.h
 * @brief Parser for IHE XDS.b ITI-18 query:AdhocQueryResponse.
 *
 * Locates the query:AdhocQueryResponse body in the SOAP envelope emitted
 * by the Document Registry, reads its RegistryResponse status, and walks
 * each rim:ExtrinsicObject entry into a registry_query_result /
 * document_entry pair.
 *
 * This is an internal header.
 */

#pragma once

#include <kcenon/common/patterns/result.h>
#include <kcenon/pacs/ihe/xds/errors.h>
#include <kcenon/pacs/ihe/xds/registry_query.h>

#include <string>

namespace kcenon::pacs::ihe::xds::detail {

/**
 * @brief Parse an ITI-18 query:AdhocQueryResponse envelope.
 *
 * @p root_xml is the full SOAP envelope body returned by the Registry
 * (ITI-18 does NOT use MTOM; the response is plain application/soap+xml).
 *
 * Returns consumer_query_response_parse_failed when the body is not a
 * valid query:AdhocQueryResponse, when the RegistryResponse status is
 * Failure, or when the parser cannot locate the RegistryObjectList.
 * registry_partial_success surfaces when the Registry reports
 * PartialSuccess — callers should treat that as a diagnostic, not silent
 * success. An empty entries vector with Success status is a legitimate
 * "no matches" response and surfaces as an ok Result.
 */
kcenon::common::Result<registry_query_result> parse_query_response(
    const std::string& root_xml);

}  // namespace kcenon::pacs::ihe::xds::detail
