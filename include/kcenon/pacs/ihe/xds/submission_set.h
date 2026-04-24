// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file submission_set.h
 * @brief XDS.b submission set metadata for ITI-41 Provide and Register
 *        Document Set-b.
 *
 * These are plain data carriers. They mirror the subset of ebRIM/ebRS
 * attributes required for a minimally conforming XDS.b submission set
 * containing one or more documents. Fields are deliberately string-typed
 * because the upstream DICOM dataset and the downstream ebXML registry
 * both canonicalize to strings.
 *
 * @see IHE ITI TF-2a §3.41 - Provide and Register Document Set-b
 * @see Issue #1128
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace kcenon::pacs::ihe::xds {

/**
 * @brief A single XDS document entry plus its binary payload.
 *
 * unique_id is the XDSDocumentEntry.uniqueId classification - typically the
 * OID of the source document (for DICOM KOS this is the SOP Instance UID
 * prefixed with "urn:oid:").
 *
 * mime_type must be an RFC 2046 media type (e.g. "application/dicom",
 * "application/hl7-v3+xml").
 *
 * content holds the raw bytes that become the MTOM-attached part. Ownership
 * is by-copy; document_source::submit does not outlive the call.
 */
struct xds_document {
    std::string entry_uuid;
    std::string unique_id;
    std::string mime_type{"application/dicom"};
    std::string title;
    std::string format_code;
    std::string class_code;
    std::string type_code;
    std::string language_code{"en-US"};
    std::string creation_time;
    std::vector<std::uint8_t> content;
};

/**
 * @brief ebRIM/ebRS-shaped submission set metadata.
 *
 * patient_id is the XDS affinity-domain patient ID in CX form
 * ("id^^^&assigningAuthority&ISO"). source_id is the Document Source actor's
 * OID. submission_set_unique_id is the OID assigned to this submission.
 *
 * All fields are required - validation happens inside document_source::submit
 * and returns a typed error_code on failure rather than throwing.
 */
struct submission_set {
    std::string submission_set_unique_id;
    std::string entry_uuid;
    std::string source_id;
    std::string patient_id;
    std::string content_type_code;
    std::string submission_time;
    std::string author_institution;
    std::string author_person;

    std::vector<xds_document> documents;
};

/**
 * @brief ITI-41 response summary.
 *
 * registry_response_status is the ebRS RegistryResponse/@status value, e.g.
 * "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success" or
 * "...:Failure". Callers should treat any non-Success status as an error;
 * document_source::submit already maps non-Success to
 * error_code::registry_failure_response, so receiving a submit_response
 * implies Success.
 *
 * submission_set_unique_id echoes what was submitted, for logging.
 */
struct submit_response {
    std::string registry_response_status;
    std::string submission_set_unique_id;
};

/**
 * @brief ITI-43 RetrieveDocumentSet request target.
 *
 * document_unique_id is the XDSDocumentEntry.uniqueId of the document to
 * retrieve (without the "cid:" or "urn:oid:" prefix; the wire format used
 * in the XDS.b request is the raw OID/UID string).
 *
 * repository_unique_id is the Document Repository's sourceId (OID) that
 * holds the document; the registry tracks this as the
 * XDSDocumentEntry.repositoryUniqueId slot value and the Consumer must
 * echo it back so the repository knows which of its stored documents is
 * being requested.
 *
 * TODO(#1129-follow-up): add a home_community_id field back when XCA
 * Cross-Community Access (ITI-39 / XCA.b-Retrieve) is implemented. The
 * Consumer's retrieve() public API is intra-community only today; the
 * XCA follow-up will also plumb this field through retrieve_envelope
 * and surface a wider overload.
 */
struct retrieve_request {
    std::string document_unique_id;
    std::string repository_unique_id;
};

/**
 * @brief ITI-43 RetrieveDocumentSet response payload.
 *
 * mime_type is echoed from the MTOM part's Content-Type header. content
 * holds the retrieved document bytes decoded from the multipart/related
 * attachment. registry_response_status is the ebRS status echoed from the
 * response envelope; document_consumer::retrieve already maps non-Success
 * to a typed error, so receiving a document_response implies the registry
 * reported Success.
 */
struct document_response {
    std::string registry_response_status;
    std::string document_unique_id;
    std::string repository_unique_id;
    std::string mime_type;
    std::vector<std::uint8_t> content;
};

/**
 * @brief Optional creation-time window for ITI-18 FindDocuments queries.
 *
 * When @ref creation_time_from or @ref creation_time_to is non-empty the
 * Registry Query actor emits the corresponding
 * $XDSDocumentEntryCreationTimeFrom / $XDSDocumentEntryCreationTimeTo slot.
 * Both fields are DTM-formatted (YYYYMMDDHHMMSS) and the registry treats
 * the range as inclusive on both ends. Empty strings disable that bound.
 *
 * status_values is the list of XDSDocumentEntry status URNs to match.
 * When empty, the actor defaults to "Approved" (the conformant FindDocuments
 * behavior for most deployments). Callers that need Deprecated documents or
 * multi-status queries populate this list explicitly.
 */
struct registry_query_options {
    std::string creation_time_from;
    std::string creation_time_to;
    std::vector<std::string> status_values;
};

/**
 * @brief ITI-18 RegistryStoredQuery request target.
 *
 * Exactly one of @ref patient_id or @ref document_uuids is populated; the
 * Registry Query actor dispatches to FindDocuments
 * (urn:uuid:14d4debf-8f97-4251-9a1e-a3a9d68b2a6f) when patient_id is set and
 * to GetDocuments (urn:uuid:5737b14c-8a1a-4539-b659-e03a34a5e1e4) when
 * document_uuids is non-empty. Callers pick the variant via the
 * registry_query::find_documents / get_documents overloads; this shared
 * struct keeps the wire builder single-path.
 *
 * patient_id follows the same XDS affinity-domain CX form as
 * submission_set::patient_id ("id^^^&assigningAuthority&ISO").
 *
 * document_uuids carries urn:uuid: entry UUIDs; the actor wraps them in the
 * ('...','...') list literal that the ebRIM StoredQuery slot accepts.
 */
struct registry_query_request {
    std::string patient_id;
    std::vector<std::string> document_uuids;
    registry_query_options options;
};

/**
 * @brief One XDSDocumentEntry extracted from an AdhocQueryResponse.
 *
 * Slots reflect the ebRIM ExtrinsicObject attributes the registry echoes.
 * Fields are filled best-effort: repositories that omit a slot leave the
 * corresponding string empty. Callers needing strict conformance should
 * validate critical fields (unique_id, repository_unique_id) are non-empty.
 *
 * size_bytes is parsed from the $XDSDocumentEntrySize slot. Zero indicates
 * either an omitted slot or a document of unknown size; callers cannot
 * distinguish the two from this field alone.
 */
struct registry_document_entry {
    std::string entry_uuid;
    std::string unique_id;
    std::string repository_unique_id;
    std::string home_community_id;
    std::string patient_id;
    std::string mime_type;
    std::string status;
    std::string creation_time;
    std::string title;
    std::string author_person;
    std::string format_code;
    std::string class_code;
    std::string type_code;
    std::string hash;
    std::uint64_t size_bytes{0};
};

/**
 * @brief ITI-18 RegistryStoredQueryResponse payload.
 *
 * entries carries one registry_document_entry per matching document. An
 * empty list on a Success status means the query was well-formed but no
 * documents match the criteria - callers should branch on entries.empty()
 * rather than treating no-results as an error.
 */
struct registry_query_result {
    std::string registry_response_status;
    std::vector<registry_document_entry> entries;
};

}  // namespace kcenon::pacs::ihe::xds
