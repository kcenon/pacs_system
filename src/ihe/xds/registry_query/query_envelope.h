// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file query_envelope.h
 * @brief SOAP 1.2 envelope builder for IHE XDS.b ITI-18 RegistryStoredQuery.
 *
 * Produces a query:AdhocQueryRequest wrapped in a SOAP 1.2 Envelope with a
 * WS-Addressing header and an empty WS-Security header that wss_signer
 * fills in. XML is serialized via pugixml using the same format flags as
 * the ITI-41 / ITI-43 builders so canonicalization stays policy-consistent.
 *
 * This is an internal header - not installed, not part of the public API.
 *
 * @see IHE ITI TF-2a §3.18.4.1.2 - Registry Stored Query Request
 */

#pragma once

#include <kcenon/common/patterns/result.h>
#include <kcenon/pacs/ihe/xds/errors.h>

#include "../common/soap_envelope.h"

#include <string>
#include <vector>

namespace kcenon::pacs::ihe::xds::detail {

/**
 * @brief Build an ITI-18 FindDocuments SOAP envelope.
 *
 * Emits query:AdhocQueryRequest / query:AdhocQuery with
 * id="urn:uuid:14d4debf-8f97-4251-9a1e-8aabd7b6f011" and two Slot
 * children: $XDSDocumentEntryPatientId and $XDSDocumentEntryStatus. The
 * wsa:Action is "urn:ihe:iti:2007:RegistryStoredQuery".
 *
 * @p patient_id is passed through verbatim; validation happens in the
 * registry_query impl so the envelope builder can stay pure. @p statuses
 * expands to one quoted list inside the slot value as required by the
 * ebRS stored-query grammar.
 *
 * Returns consumer_query_missing_patient_id when the caller leaks an
 * empty patient_id through; envelope_serialization_failed on pugixml
 * serialization failure.
 */
kcenon::common::Result<built_envelope> build_iti18_find_documents_envelope(
    const std::string& patient_id,
    const std::vector<std::string>& statuses);

/**
 * @brief Build an ITI-18 GetDocuments SOAP envelope.
 *
 * Emits query:AdhocQueryRequest / query:AdhocQuery with
 * id="urn:uuid:5c4f972b-d56b-40ac-a5fc-c8ca9b40b9d4" and one Slot
 * child: $XDSDocumentEntryUUID carrying a quoted list of the requested
 * entry UUIDs.
 *
 * Returns consumer_query_empty_uuid_list on empty input;
 * envelope_serialization_failed on pugixml serialization failure.
 */
kcenon::common::Result<built_envelope> build_iti18_get_documents_envelope(
    const std::vector<std::string>& uuids);

}  // namespace kcenon::pacs::ihe::xds::detail
