// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file http_options.h
 * @brief Transport configuration for IHE XDS.b actors.
 *
 * @see Issue #1128 - IHE XDS.b Document Source (ITI-41)
 */

#pragma once

#include <chrono>
#include <string>

namespace kcenon::pacs::ihe::xds {

/**
 * @brief HTTPS transport options for the Document Source actor.
 *
 * The endpoint is the full URL of the Document Registry's Register Document Set-b
 * service (ITI-42 target — the Source POSTs ITI-41 to the Registry here).
 *
 * ca_bundle_path optionally overrides libcurl's default CA bundle. When empty,
 * the system trust store is used. For pinned-PKI deployments set this to a PEM
 * file containing only the trusted registry root(s).
 *
 * client_certificate_path and client_private_key_path are optional mTLS
 * credentials. When set, libcurl presents them during the TLS handshake. They
 * are independent from the WS-Security signing certificate carried in the
 * document_source constructor.
 */
struct http_options {
    std::string endpoint;
    std::string soap_action{
        "urn:ihe:iti:2007:RegisterDocumentSet-b"};

    std::chrono::milliseconds connect_timeout{std::chrono::seconds{10}};
    std::chrono::milliseconds request_timeout{std::chrono::seconds{30}};

    bool verify_peer{true};
    bool verify_host{true};
    std::string ca_bundle_path;

    std::string client_certificate_path;
    std::string client_private_key_path;

    std::string user_agent{"kcenon-pacs-xds/0.1"};
};

}  // namespace kcenon::pacs::ihe::xds
