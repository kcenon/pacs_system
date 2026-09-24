// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file retrieve_envelope.h
 * @brief SOAP 1.2 envelope builder for IHE XDS.b ITI-43 RetrieveDocumentSet.
 *
 * Produces an xdsb:RetrieveDocumentSetRequest body wrapped in a SOAP 1.2
 * Envelope with a WS-Addressing header and an empty WS-Security header that
 * wss_signer fills in. XML is serialized via pugixml; the output is UTF-8
 * without a BOM, with XML declaration, suitable for canonicalization.
 *
 * This is an internal header - not installed, not part of the public API.
 *
 * @see IHE ITI TF-2b §3.43.4.1.2 - RetrieveDocumentSet Request
 */

#pragma once

#include <kcenon/common/patterns/result.h>
#include <kcenon/pacs/ihe/xds/errors.h>
#include <kcenon/pacs/ihe/xds/submission_set.h>

#include "../common/soap_envelope.h"

namespace kcenon::pacs::ihe::xds::detail {

/**
 * @brief Build a SOAP envelope for ITI-43 from a retrieve_request.
 *
 * Emits one xdsb:DocumentRequest element per call. The envelope carries a
 * wsa:Action of "urn:ihe:iti:2007:RetrieveDocumentSet" and an empty
 * wsse:Security header containing a Timestamp and BinarySecurityToken that
 * sign_envelope fills in and signs afterwards.
 *
 * Returns consumer_retrieve_missing_document_id /
 * consumer_retrieve_missing_repository_id on empty fields;
 * envelope_serialization_failed if pugixml cannot serialize the document.
 */
kcenon::common::Result<built_envelope> build_iti43_envelope(
    const retrieve_request& req);

}  // namespace kcenon::pacs::ihe::xds::detail
