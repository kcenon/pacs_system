// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file document_consumer.h
 * @brief IHE XDS.b Document Consumer Actor (ITI-43 Retrieve Document Set).
 *
 * The Document Consumer fetches a single document by (documentUniqueId,
 * repositoryUniqueId) pair from an IHE XDS.b Document Repository. Transport
 * is SOAP 1.2 + MTOM over HTTPS. The outbound RetrieveDocumentSetRequest
 * carries no attachments; the inbound RetrieveDocumentSetResponse is a
 * multipart/related MTOM body whose root part is the signed SOAP envelope
 * and whose attachment part is the document payload referenced by an
 * xop:Include element.
 *
 * Scope of this header (Issue #1129):
 *  - Document Consumer actor only, ITI-43 Retrieve transaction.
 *  - Registry Query (#1130) and ATNA wiring (#1131) are siblings and are
 *    intentionally not exposed here.
 *
 * Interoperability caveat: the Consumer verifies response signatures using
 * the same project-local canonicalization URI
 * "urn:kcenon:xds:c14n:pugixml-format-raw-v1" that the Source emits. It
 * is NOT interoperable with a third-party exc-c14n signer until the
 * follow-up full-c14n issue lands. Against a real IHE Gazelle / Apache CXF
 * / .NET WIF repository the verifier will reject otherwise-valid responses
 * at the Algorithm check. See common/wss_signer.cpp for the canonicalization
 * policy rationale.
 *
 * @see IHE ITI TF-2b §3.43
 * @see Issue #1129
 */

#pragma once

#include <kcenon/common/patterns/result.h>
#include <kcenon/pacs/ihe/xds/document_source.h>
#include <kcenon/pacs/ihe/xds/errors.h>
#include <kcenon/pacs/ihe/xds/http_options.h>
#include <kcenon/pacs/ihe/xds/submission_set.h>

#include <memory>
#include <string>

namespace kcenon::pacs::security {
class xds_audit_sink;
}  // namespace kcenon::pacs::security

namespace kcenon::pacs::ihe::xds {

/**
 * @brief ITI-43 Document Consumer actor.
 *
 * Binds one repository endpoint and one signing credential. To query
 * different repositories or sign with different certificates, instantiate
 * multiple document_consumer objects.
 *
 * The signing_material is reused from the Document Source type so a single
 * deployment can share its WS-Security credential between the push (ITI-41)
 * and pull (ITI-43) actors without re-declaring the struct.
 *
 * Thread-safety: retrieve() is NOT reentrant per instance - callers must
 * serialize retrieve calls per instance, or create one instance per worker
 * thread. The underlying libcurl handle is owned by the impl and may not
 * be shared between threads.
 */
class document_consumer {
public:
    /**
     * @brief Construct an ITI-43 Document Consumer bound to a repository
     *        endpoint and a signing credential.
     *
     * The endpoint in @p opts must be the Retrieve Document Set-b service
     * URL on the Document Repository. The default http_options.soap_action
     * ("urn:ihe:iti:2007:RegisterDocumentSet-b") is ITI-41-specific and is
     * overwritten internally by the ITI-43 action URI before the POST.
     * Callers need not clear it themselves.
     *
     * Signing material is copied into the instance; the caller may then
     * zeroize the original buffer. The constructor never throws - any
     * material validation failure is deferred to retrieve() and surfaces
     * as a typed error_code.
     */
    document_consumer(http_options opts, signing_material signing);

    document_consumer(document_consumer&&) noexcept;
    document_consumer& operator=(document_consumer&&) noexcept;

    document_consumer(const document_consumer&) = delete;
    document_consumer& operator=(const document_consumer&) = delete;

    ~document_consumer();

    /**
     * @brief Retrieve one document by its (documentUniqueId,
     *        repositoryUniqueId) pair.
     *
     * Sequence: validate identifiers -> build xdsb:RetrieveDocumentSetRequest
     * SOAP envelope -> WS-Security sign Timestamp+Body -> POST plain SOAP
     * (no outbound MTOM parts) over HTTPS -> receive multipart/related
     * MTOM response -> parse root envelope and attachment parts ->
     * verify_envelope_integrity on the response -> locate the
     * DocumentResponse matching the requested uid and resolve its
     * xop:Include to the attachment bytes.
     *
     * TRUST CAVEAT: the response signature check is integrity-only - it
     * binds the payload to the cert embedded in the response's BST, but
     * does NOT validate that cert against a trust anchor. An on-path
     * attacker who can serve any valid X.509 cert will pass this check.
     * Full trust-chain validation is deferred to the Gazelle conformance
     * integration (#1131). Callers relying on retrieve() for authenticity
     * must not treat a successful Result as signer-authenticated.
     *
     * All failure modes surface as typed error_code values; the function
     * never throws across the API boundary. A Registry-reported Failure
     * status or an absent DocumentResponse maps to
     * consumer_response_document_not_found.
     */
    kcenon::common::Result<document_response> retrieve(
        const std::string& document_unique_id,
        const std::string& repository_unique_id);

    /**
     * @brief Install an ATNA audit sink.
     *
     * When set, retrieve() emits exactly one audit event per invocation
     * - success or failure - carrying the requested document uid,
     * repository uid, source endpoint, and the outcome code.
     *
     * @see kcenon::pacs::security::xds_audit_sink
     * @see Issue #1131
     */
    void set_audit_sink(
        std::shared_ptr<kcenon::pacs::security::xds_audit_sink> sink);

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

}  // namespace kcenon::pacs::ihe::xds
