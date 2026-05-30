// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file query_envelope.h
 * @brief SOAP 1.2 envelope builder for IHE XDS.b ITI-18 RegistryStoredQuery.
 *
 * Produces an AdhocQueryRequest body wrapped in a SOAP 1.2 Envelope with a
 * WS-Addressing header and an empty WS-Security header that wss_signer
 * fills in. Two stored-query IDs are supported:
 *  - FindDocuments: urn:uuid:14d4debf-8f97-4251-9a1e-a3a9d68b2a6f
 *  - GetDocuments:  urn:uuid:5737b14c-8a1a-4539-b659-e03a34a5e1e4
 *
 * This is an internal header - not installed, not part of the public API.
 *
 * @see IHE ITI TF-2a §3.18.4.1.2 - RegistryStoredQuery Request
 */

#pragma once

#include <kcenon/common/patterns/result.h>
#include <kcenon/pacs/ihe/xds/errors.h>
#include <kcenon/pacs/ihe/xds/submission_set.h>

#include "../common/soap_envelope.h"

namespace kcenon::pacs::ihe::xds::detail {

/**
 * @brief Discriminator for the two stored queries this actor supports.
 */
enum class stored_query_kind {
    find_documents,
    get_documents,
};

/**
 * @brief Build a SOAP envelope for ITI-18 from a registry_query_request.
 *
 * Emits an AdhocQueryRequest containing one AdhocQuery with the stored
 * query id corresponding to @p kind. The envelope carries a wsa:Action of
 * "urn:ihe:iti:2007:RegistryStoredQuery" and an empty wsse:Security header
 * containing a Timestamp and BinarySecurityToken that sign_envelope fills
 * in and signs afterwards.
 *
 * For find_documents: emits $XDSDocumentEntryPatientId and
 * $XDSDocumentEntryStatus slots, plus optional $XDSDocumentEntryCreationTimeFrom
 * / $XDSDocumentEntryCreationTimeTo slots when @p req.options populates them.
 *
 * For get_documents: emits $XDSDocumentEntryEntryUUID with the list
 * literal form that ebRIM StoredQuery slots require.
 *
 * Returns registry_query_missing_patient_id / registry_query_empty_uuid_list
 * on empty required fields; envelope_serialization_failed if pugixml cannot
 * serialize the document.
 */
kcenon::common::Result<built_envelope> build_iti18_envelope(
    const registry_query_request& req, stored_query_kind kind);

}  // namespace kcenon::pacs::ihe::xds::detail
