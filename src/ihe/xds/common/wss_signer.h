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

#include <string>
#include <string_view>

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
 * @brief Verify the WS-Security signature of a previously signed envelope.
 *
 * Reparses @p signed_xml, locates the ds:Signature in wsse:Security,
 * decodes the BinarySecurityToken into an X.509 public key, re-digests
 * Body and Timestamp with the same pugixml format_raw serializer used for
 * signing, and calls EVP_DigestVerify against the SignedInfo bytes.
 *
 * Honest-URI policy: the CanonicalizationMethod and every Transform
 * Algorithm URI inside SignedInfo MUST equal
 * "urn:kcenon:xds:c14n:pugixml-format-raw-v1". If any Algorithm advertises
 * exc-c14n or any other URI, verification is refused with
 * consumer_signature_verification_failed rather than silently digesting a
 * mismatched byte stream. This mirrors the signer's behavior.
 *
 * Returns consumer_signature_missing if the envelope carries no
 * ds:Signature element; consumer_signature_verification_failed on any
 * other mismatch (digest, signature, algorithm, KeyInfo shape).
 */
kcenon::common::Result<bool> verify_envelope(std::string_view signed_xml);

}  // namespace kcenon::pacs::ihe::xds::detail
