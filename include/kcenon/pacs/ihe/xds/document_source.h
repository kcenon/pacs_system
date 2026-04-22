// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file document_source.h
 * @brief IHE XDS.b Document Source Actor (ITI-41 Provide and Register
 *        Document Set-b).
 *
 * The Document Source transmits an ebXML submission set plus one or more
 * documents to a Document Registry over SOAP 1.2 + MTOM + HTTPS. This class
 * orchestrates envelope construction, WS-Security signing, MTOM packaging,
 * and libcurl transport, and returns a typed Result<submit_response>.
 *
 * Scope of this header (Issue #1128):
 *  - Document Source actor only.
 *  - Consumer (#1129), Registry Query (#1130), and ATNA wiring (#1131) are
 *    siblings and intentionally not exposed here.
 *
 * @see IHE ITI TF-2a §3.41
 * @see Issue #1128
 */

#pragma once

#include <kcenon/common/patterns/result.h>
#include <kcenon/pacs/ihe/xds/errors.h>
#include <kcenon/pacs/ihe/xds/http_options.h>
#include <kcenon/pacs/ihe/xds/submission_set.h>

#include <memory>
#include <string>

namespace kcenon::pacs::ihe::xds {

/**
 * @brief Signing material for WS-Security XML-DSig.
 *
 * certificate_pem is the X.509 signer certificate in PEM form. The
 * Document Source embeds its DER-encoded form as a BinarySecurityToken.
 *
 * private_key_pem is the matching RSA or EC private key in PEM form.
 * If password-protected, the PEM passphrase dialog is bypassed by the
 * provided password field.
 *
 * PEM strings are loaded directly rather than using the security
 * classes so callers can source them from an HSM-backed buffer that
 * never hits disk. Both the certificate and key are validated lazily
 * on the first submit() call; malformed material produces a typed
 * error_code value rather than a constructor exception.
 */
struct signing_material {
    std::string certificate_pem;
    std::string private_key_pem;
    std::string private_key_password;
};

/**
 * @brief ITI-41 Document Source actor.
 *
 * The instance is bound to one endpoint and one signing credential. To
 * submit to different registries or sign with different certificates,
 * instantiate multiple document_source objects.
 *
 * Thread-safety: a single instance is safe to destroy but submit() is NOT
 * reentrant - callers must serialize submit calls per instance, or create
 * one instance per worker thread. libcurl handles are owned by the impl
 * and may not be shared between threads.
 */
class document_source {
public:
    /**
     * @brief Construct an ITI-41 Document Source bound to an endpoint and
     *        a signing credential.
     *
     * Signing material is copied into the instance; the caller may then
     * zeroize the original buffer. The constructor never throws - any
     * material validation failure is deferred to submit() and surfaces
     * as a typed error_code.
     */
    document_source(http_options opts, signing_material signing);

    document_source(document_source&&) noexcept;
    document_source& operator=(document_source&&) noexcept;

    document_source(const document_source&) = delete;
    document_source& operator=(const document_source&) = delete;

    ~document_source();

    /**
     * @brief Submit an XDS.b submission set.
     *
     * Sequence: validate metadata -> build ebRIM SubmitObjectsRequest SOAP
     * envelope -> WS-Security sign Timestamp+Body -> wrap body parts in
     * multipart/related MTOM payload -> POST to configured endpoint over
     * HTTPS -> parse RegistryResponse.
     *
     * All failure modes surface as typed error_code values; the function
     * never throws across the API boundary. On success, the returned
     * submit_response echoes the registry's Success status and the
     * submission set unique id.
     */
    kcenon::common::Result<submit_response> submit(const submission_set& s);

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

}  // namespace kcenon::pacs::ihe::xds
