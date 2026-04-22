// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file soap_envelope.h
 * @brief SOAP 1.2 envelope builder for IHE XDS.b ITI-41 requests.
 *
 * Produces a SubmitObjectsRequest body wrapped in an Envelope with a
 * WS-Addressing header and an empty WS-Security header that wss_signer
 * fills in. XML is serialized via pugixml; the output is UTF-8 without a
 * BOM, with XML declaration, suitable for canonicalization.
 *
 * This is an internal header - not installed, not part of the public API.
 *
 * @see IHE ITI TF-2a §3.41.4.1.2 - ProvideAndRegisterDocumentSet-b Request
 */

#pragma once

#include <kcenon/common/patterns/result.h>
#include <kcenon/pacs/ihe/xds/errors.h>
#include <kcenon/pacs/ihe/xds/submission_set.h>

#include <string>

namespace kcenon::pacs::ihe::xds::detail {

/**
 * @brief Result of envelope construction.
 *
 * xml is the full envelope serialized as UTF-8 XML. body_id is the
 * xml:id attribute placed on the <soap:Body> element; wss_signer
 * references it from the ds:Signature.
 */
struct built_envelope {
    std::string xml;
    std::string body_id;
    std::string timestamp_id;
    std::string binary_security_token_id;
};

/**
 * @brief Build a SOAP envelope for ITI-41 from submission metadata and a
 *        list of content-id references for MTOM-attached document bytes.
 *
 * The cid_refs vector must be the same length as s.documents. Each cid is
 * placed inside the matching ExtrinsicObject as an xop:Include.
 *
 * Returns envelope_build_failed on any input inconsistency;
 * envelope_serialization_failed if pugixml cannot serialize the document.
 */
kcenon::common::Result<built_envelope> build_iti41_envelope(
    const submission_set& s, const std::vector<std::string>& cid_refs);

}  // namespace kcenon::pacs::ihe::xds::detail
