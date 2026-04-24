// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file registry_query.h
 * @brief IHE XDS.b Registry Query Actor (ITI-18 RegistryStoredQuery).
 *
 * The Registry Query actor issues ebRS/ebRIM StoredQuery requests against
 * an IHE XDS.b Document Registry. Transport is SOAP 1.2 over HTTPS with
 * WS-Security signing for both the outbound request and the returned
 * response envelope. Responses are plain SOAP bodies (no MTOM attachments)
 * because the registry reports metadata only - document payloads are
 * fetched via ITI-43 afterwards.
 *
 * Scope of this header (Issue #1130):
 *  - FindDocuments and GetDocuments stored queries (intra-community).
 *  - ATNA audit emission is deferred to #1131.
 *  - Document retrieval (Consumer, ITI-43) is a sibling actor (#1129).
 *
 * Interoperability caveat: the actor verifies response signatures with the
 * same project-local canonicalization URI as document_consumer
 * ("urn:kcenon:xds:c14n:pugixml-format-raw-v1"). It is NOT interoperable
 * with a third-party exc-c14n signer until the follow-up full-c14n work
 * lands. See common/wss_signer.cpp for rationale.
 *
 * @see IHE ITI TF-2a §3.18 - Registry Stored Query
 * @see Issue #1130
 */

#pragma once

#include <kcenon/common/patterns/result.h>
#include <kcenon/pacs/ihe/xds/document_source.h>
#include <kcenon/pacs/ihe/xds/errors.h>
#include <kcenon/pacs/ihe/xds/http_options.h>
#include <kcenon/pacs/ihe/xds/submission_set.h>

#include <memory>
#include <string>
#include <vector>

namespace kcenon::pacs::ihe::xds {

/**
 * @brief ITI-18 Registry Query actor.
 *
 * Binds one registry endpoint and one signing credential. To query
 * different registries or sign with different certificates, instantiate
 * multiple registry_query objects.
 *
 * The signing_material type is reused from document_source so a single
 * deployment can share the WS-Security credential between the ITI-41
 * (push), ITI-43 (retrieve), and ITI-18 (query) actors without
 * re-declaring the struct.
 *
 * Thread-safety: find_documents / get_documents are NOT reentrant per
 * instance - callers must serialize calls per instance, or create one
 * instance per worker thread. The underlying libcurl handle is owned by
 * the impl and may not be shared between threads.
 */
class registry_query {
public:
    /**
     * @brief Construct an ITI-18 Registry Query actor bound to a registry
     *        endpoint and a signing credential.
     *
     * The endpoint in @p opts must be the Registry Stored Query service URL
     * on the Document Registry. The default http_options::soap_action
     * ("urn:ihe:iti:2007:RegisterDocumentSet-b") is ITI-41-specific and is
     * overwritten internally by the ITI-18 action URI before the POST.
     * Callers need not clear it themselves.
     *
     * Signing material is copied into the instance; the caller may then
     * zeroize the original buffer. The constructor never throws - any
     * material validation failure is deferred to the query methods and
     * surfaces as a typed error_code.
     */
    registry_query(http_options opts, signing_material signing);

    registry_query(registry_query&&) noexcept;
    registry_query& operator=(registry_query&&) noexcept;

    registry_query(const registry_query&) = delete;
    registry_query& operator=(const registry_query&) = delete;

    ~registry_query();

    /**
     * @brief Execute the FindDocuments stored query
     *        (urn:uuid:14d4debf-8f97-4251-9a1e-a3a9d68b2a6f).
     *
     * Returns the list of XDSDocumentEntry metadata the registry holds for
     * @p patient_id. An empty result list on Success means the query was
     * well-formed but no documents matched - callers should branch on the
     * result's entries vector rather than treating empty as an error.
     *
     * @p options is optional; pass a default-constructed registry_query_options
     * to let the actor emit the default "Approved" status filter and no
     * creation-time window.
     *
     * TRUST CAVEAT: the response signature check is integrity-only - it
     * binds the payload to the cert embedded in the response's BST, but
     * does NOT validate that cert against a trust anchor. Callers must
     * not treat a successful Result as signer-authenticated.
     *
     * All failure modes surface as typed error_code values; the function
     * never throws across the API boundary.
     */
    kcenon::common::Result<registry_query_result> find_documents(
        const std::string& patient_id,
        const registry_query_options& options = {});

    /**
     * @brief Execute the GetDocuments stored query
     *        (urn:uuid:5737b14c-8a1a-4539-b659-e03a34a5e1e4).
     *
     * Resolves explicit XDSDocumentEntry UUIDs to their metadata. @p uuids
     * must carry at least one entry_uuid in urn:uuid: form; an empty list
     * is rejected with registry_query_empty_uuid_list before any transport
     * call is made.
     *
     * Returns the entries the registry has for the requested UUIDs. The
     * registry may return fewer entries than requested when some UUIDs are
     * unknown; callers wanting a strict all-or-nothing semantic must
     * compare entries.size() to uuids.size() themselves.
     */
    kcenon::common::Result<registry_query_result> get_documents(
        const std::vector<std::string>& uuids);

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

}  // namespace kcenon::pacs::ihe::xds
