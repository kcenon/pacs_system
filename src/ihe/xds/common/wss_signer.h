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

}  // namespace kcenon::pacs::ihe::xds::detail
