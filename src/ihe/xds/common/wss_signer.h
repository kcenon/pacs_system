// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file wss_signer.h
 * @brief WS-Security XML-DSig signer backed by OpenSSL EVP_DigestSign.
 *
 * Signs the <soap:Body> and <wsu:Timestamp> of a previously constructed
 * envelope and injects <ds:Signature> into the <wsse:Security> header. The
 * resulting XML is ready for transmission.
 *
 * Canonicalization: exc-c14n (http://www.w3.org/2001/10/xml-exc-c14n#).
 * Signature method: RSA-SHA256. KeyInfo points to the BinarySecurityToken
 * that already exists in the envelope header.
 *
 * This is an internal header.
 */

#pragma once

#include <kcenon/common/patterns/result.h>
#include <kcenon/pacs/ihe/xds/errors.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kcenon::pacs::ihe::xds::detail {

struct built_envelope;

/**
 * @brief Sign the body + timestamp of @p env in place.
 *
 * @p cert_pem is the PEM-encoded X.509 signer certificate. @p key_pem is
 * the matching PEM RSA or EC private key; @p key_password decrypts it when
 * the key is encrypted and is ignored otherwise.
 *
 * The envelope's BinarySecurityToken is populated with the DER-encoded
 * certificate (Base64). A ds:Signature element is appended to the
 * wsse:Security header covering the body and timestamp by wsu:Id reference.
 *
 * On success, @p env.xml is replaced by the signed envelope and the
 * Result<bool> value is true (the bool is a sentinel; the signature itself
 * is inside env.xml).
 */
kcenon::common::Result<bool> sign_envelope(built_envelope& env,
                                           std::string_view cert_pem,
                                           std::string_view key_pem,
                                           std::string_view key_password);

/**
 * @brief Verify the *structural integrity* of a previously signed
 *        envelope against the certificate embedded in its own
 *        BinarySecurityToken.
 *
 * Reparses @p signed_xml, locates the ds:Signature in wsse:Security,
 * decodes the BinarySecurityToken into an X.509 public key, re-digests
 * Body and Timestamp with the same pugixml format_raw serializer used for
 * signing, and calls EVP_DigestVerify against the SignedInfo bytes.
 *
 * SECURITY NOTE - SELF-AUTHENTICATING CHECK, NOT TRUST VALIDATION:
 * this function verifies that the ds:Signature binds the payload to the
 * certificate embedded in the response's BST. It does NOT validate that
 * certificate against a trust anchor, CRL, OCSP responder, or peer TLS
 * identity. A man-in-the-middle who can serve any valid X.509
 * certificate can produce a response that passes this check - the
 * embedded key, the embedded signature, and the embedded payload are
 * all self-consistent by construction. Full trust-chain validation
 * will land with the Gazelle conformance integration tracked in #1131.
 * Until that lands, callers relying solely on this function for
 * authenticity guarantees are trusting whatever party the TLS peer is.
 *
 * The rename from verify_envelope to verify_envelope_integrity in the
 * #1129 review cycle is deliberate: the new name closes the "what does
 * this actually guarantee" gap at the call site.
 *
 * Honest-URI policy: the CanonicalizationMethod and every Transform
 * Algorithm URI inside SignedInfo MUST equal
 * "urn:kcenon:xds:c14n:pugixml-format-raw-v1". If any Algorithm advertises
 * exc-c14n or any other URI, verification is refused with
 * consumer_signature_verification_failed rather than silently digesting a
 * mismatched byte stream. This mirrors the signer's behavior.
 *
 * Canonicalization caveat: verification is only reproducible for
 * envelopes produced by this project's sign_envelope. Real exc-c14n
 * interop is a follow-up issue that must land before production use
 * against a third-party IHE XDS.b repository.
 *
 * Returns consumer_signature_missing if the envelope carries no
 * ds:Signature element; consumer_signature_verification_failed on any
 * other mismatch (digest, signature, algorithm, KeyInfo shape).
 */
kcenon::common::Result<bool> verify_envelope_integrity(
    std::string_view signed_xml);

/**
 * @brief Base64-decode @p b64 using the same BIO-backed decoder the
 *        signer and verifier use internally.
 *
 * Exported so callers outside wss_signer.cpp (in particular the retrieve
 * response parser, which needs to decode inline Document payloads on
 * non-MTOM responses) do not have to re-implement the OpenSSL BIO dance.
 * Returns an empty vector on decode failure; callers must check.
 */
std::vector<std::uint8_t> base64_decode_bytes(std::string_view b64);

}  // namespace kcenon::pacs::ihe::xds::detail
