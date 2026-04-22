// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file retrieve_response_parser.h
 * @brief Parser for IHE XDS.b ITI-43 RetrieveDocumentSetResponse.
 *
 * Locates the xdsb:RetrieveDocumentSetResponse body in the signed SOAP
 * envelope emitted by the Document Repository, reads its RegistryResponse
 * status, finds the xdsb:DocumentResponse that matches the requested
 * document/repository UID pair, and resolves its xop:Include href to the
 * corresponding MTOM attachment part.
 *
 * This is an internal header.
 */

#pragma once

#include <kcenon/common/patterns/result.h>
#include <kcenon/pacs/ihe/xds/errors.h>
#include <kcenon/pacs/ihe/xds/submission_set.h>

#include "../common/mtom_packager.h"

#include <string>
#include <vector>

namespace kcenon::pacs::ihe::xds::detail {

/**
 * @brief Parse a RetrieveDocumentSetResponse envelope and resolve the
 *        attachment that matches @p req.
 *
 * @p root_xml is the root part of the MTOM response (already verified).
 * @p parts are the remaining attachment parts, keyed on the bare
 * content-id that the xop:Include elements reference via "cid:".
 *
 * Returns consumer_response_document_not_found when the registry reports
 * a Failure status, when no DocumentResponse matches the requested
 * document_unique_id, or when the matching xop:Include points to a cid
 * that is not present in @p parts.
 */
kcenon::common::Result<document_response> parse_retrieve_response(
    const std::string& root_xml, const std::vector<mtom_part>& parts,
    const retrieve_request& req);

}  // namespace kcenon::pacs::ihe::xds::detail
