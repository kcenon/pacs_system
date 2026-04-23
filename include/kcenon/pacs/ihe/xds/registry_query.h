// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file registry_query.h
 * @brief IHE XDS.b Registry Query Actor (ITI-18 Registry Stored Query).
 *
 * The Registry Query actor discovers documents in an IHE XDS.b Document
 * Registry by running the registry's pre-defined stored queries. Transport
 * is SOAP 1.2 over HTTPS; no MTOM on request or response (the response body
 * is a plain SOAP envelope containing a query:AdhocQueryResponse with
 * rim:ExtrinsicObject metadata entries).
 *
 * Scope of this header (Issue #1130):
 *  - Two stored queries with the widest deployment footprint:
 *      * FindDocuments (urn:uuid:14d4debf-8f97-4251-9a1e-8aabd7b6f011)
 *      * GetDocuments  (urn:uuid:5c4f972b-d56b-40ac-a5fc-c8ca9b40b9d4)
 *  - ATNA audit emission and Document retrieval are deliberately NOT exposed
 *    here; see issues #1131 and #1129 respectively.
 *
 * Interoperability caveat: the SOAP envelope this actor emits uses the same
 * project-local canonicalization policy as the Source / Consumer. The
 * Registry does not verify the request signature; the Registry's own
 * response is NOT signed in ITI-18, so no verification step is performed on
 * the return path. When the follow-up full-c14n work lands, this actor will
 * interoperate with any IHE Gazelle-style Registry without changes to its
 * public API.
 *
 * @see IHE ITI TF-2a §3.18
 * @see Issue #1130
 */

#pragma once

#include <kcenon/common/patterns/result.h>
#include <kcenon/pacs/ihe/xds/document_source.h>
#include <kcenon/pacs/ihe/xds/errors.h>
#include <kcenon/pacs/ihe/xds/http_options.h>

#include <memory>
#include <string>
#include <vector>

namespace kcenon::pacs::ihe::xds {

/**
 * @brief Metadata for one rim:ExtrinsicObject entry returned by the Registry.
 *
 * Fields populated from the ExtrinsicObject attributes and the
 * well-known Slot / ExternalIdentifier children. Empty strings mean the
 * Registry omitted the element; callers should treat empty entry_uuid /
 * document_unique_id as diagnostic evidence that the response was
 * schema-legal but lacked required XDSDocumentEntry slots.
 */
struct document_entry {
    std::string entry_uuid;            ///< ExtrinsicObject @id
    std::string document_unique_id;    ///< XDSDocumentEntry.uniqueId
    std::string patient_id;            ///< XDSDocumentEntry.patientId (CX form)
    std::string mime_type;             ///< ExtrinsicObject @mimeType
    std::string status;                ///< ExtrinsicObject @status URI
    std::string creation_time;         ///< Slot creationTime (YYYYMMDDhhmmss)
    std::string repository_unique_id;  ///< Slot repositoryUniqueId
};

/**
 * @brief Result of one stored-query call.
 *
 * registry_response_status carries the literal status URI the Registry
 * emitted (Success / PartialSuccess / Failure). An empty entries vector is
 * a successful "no matches" response, not an error. Callers that need to
 * distinguish zero-results from a malformed response should inspect the
 * Result's is_ok() first and then the entries vector.
 */
struct registry_query_result {
    std::string registry_response_status;
    std::vector<document_entry> entries;
};

/**
 * @brief ITI-18 Registry Query actor.
 *
 * Binds one Registry endpoint and one signing credential. Thread-safety
 * mirrors document_consumer: per-instance serialization is required;
 * create one instance per worker thread for concurrent queries.
 *
 * The signing_material is reused from document_source so a single
 * deployment can share its WS-Security credential across the push
 * (ITI-41), pull (ITI-43) and query (ITI-18) actors.
 */
class registry_query {
public:
    /**
     * @brief Construct an ITI-18 Registry Query actor.
     *
     * The endpoint in @p opts must be the Registry Stored Query service
     * URL. The default http_options.soap_action is ITI-41-specific and is
     * overwritten internally by the ITI-18 action URI before each POST.
     */
    registry_query(http_options opts, signing_material signing);

    registry_query(registry_query&&) noexcept;
    registry_query& operator=(registry_query&&) noexcept;

    registry_query(const registry_query&) = delete;
    registry_query& operator=(const registry_query&) = delete;

    ~registry_query();

    /**
     * @brief FindDocuments stored query (urn:uuid:14d4debf-...).
     *
     * Discovers documents by patient identifier, optionally narrowed by
     * status URI. @p patient_id is a CX-formatted XDS patient identifier
     * (for example "12345^^^&1.2.3.4.5&ISO"). Empty string is rejected
     * with consumer_query_missing_patient_id without issuing a network
     * request.
     *
     * @p statuses defaults to {"urn:oasis:names:tc:ebxml-regrep:
     * StatusType:Approved"} to match the most common client pattern
     * (discover live entries only). Callers asking for deprecated entries
     * can pass both Approved and Deprecated URIs.
     *
     * A Registry response with a Failure status or a malformed envelope
     * surfaces as consumer_query_response_parse_failed; a successful
     * response with zero ExtrinsicObject children is a valid empty
     * registry_query_result.
     */
    kcenon::common::Result<registry_query_result> find_documents(
        const std::string& patient_id,
        const std::vector<std::string>& statuses = {
            "urn:oasis:names:tc:ebxml-regrep:StatusType:Approved"});

    /**
     * @brief GetDocuments stored query (urn:uuid:5c4f972b-...).
     *
     * Resolves explicit XDSDocumentEntry UUIDs to their metadata entries.
     * Empty @p uuids is rejected with consumer_query_empty_uuid_list.
     *
     * The Registry response is not required to preserve input order; the
     * returned entries vector is emitted in document order.
     */
    kcenon::common::Result<registry_query_result> get_documents(
        const std::vector<std::string>& uuids);

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

}  // namespace kcenon::pacs::ihe::xds
