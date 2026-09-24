// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file query_response_parser.h
 * @brief Parser for IHE XDS.b ITI-18 AdhocQueryResponse.
 *
 * Locates the query:AdhocQueryResponse body in the signed SOAP envelope
 * emitted by the Document Registry, reads its status attribute, and walks
 * the ExtrinsicObject entries in the embedded RegistryObjectList into a
 * registry_query_result.
 *
 * This is an internal header.
 */

#pragma once

#include <kcenon/common/patterns/result.h>
#include <kcenon/pacs/ihe/xds/errors.h>
#include <kcenon/pacs/ihe/xds/submission_set.h>

#include <string>

namespace kcenon::pacs::ihe::xds::detail {

/**
 * @brief Parse an AdhocQueryResponse envelope into a registry_query_result.
 *
 * @p root_xml is the response envelope (already signature-verified).
 *
 * Returns registry_failure_response when the registry reports a Failure
 * status. Returns registry_partial_success on PartialSuccess - callers
 * wanting to tolerate that state must inspect the status string of the
 * returned error context.
 *
 * An empty RegistryObjectList on Success is a successful empty result,
 * not an error: the function returns a registry_query_result with an
 * empty entries vector.
 */
kcenon::common::Result<registry_query_result> parse_query_response(
    const std::string& root_xml);

}  // namespace kcenon::pacs::ihe::xds::detail
