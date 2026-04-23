// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file errors.h
 * @brief Typed error codes for IHE XDS.b actors (ITI-41 and siblings)
 *
 * Every public API in kcenon::pacs::ihe::xds returns
 * kcenon::common::Result<T>. Error codes here are the numeric tag passed to
 * make_error so callers can branch on failure class without parsing strings.
 *
 * @see Issue #1128 - IHE XDS.b Document Source (ITI-41)
 */

#pragma once

#include <string_view>

namespace kcenon::pacs::ihe::xds {

/**
 * @brief Stable numeric error codes for XDS.b actor failures.
 *
 * Values are contiguous and intended for persistence/logging. They are not
 * HTTP status codes; HTTP-layer failures are reported via
 * transport_http_error with the status embedded in the error message.
 */
enum class error_code : int {
    // --- input validation (50-59) ---
    metadata_missing_patient_id = 50,
    metadata_missing_source_id = 51,
    metadata_missing_submission_set_uid = 52,
    metadata_no_documents = 53,
    metadata_document_empty = 54,
    metadata_document_missing_unique_id = 55,
    metadata_document_missing_mime_type = 56,

    // --- envelope construction (60-69) ---
    envelope_build_failed = 60,
    envelope_serialization_failed = 61,

    // --- WS-Security signing (70-79) ---
    signer_invalid_certificate = 70,
    signer_invalid_private_key = 71,
    signer_digest_failed = 72,
    signer_sign_failed = 73,
    signer_openssl_unavailable = 74,

    // --- MTOM packaging (80-89) ---
    mtom_empty_payload = 80,
    mtom_boundary_generation_failed = 81,

    // --- transport (90-99) ---
    transport_init_failed = 90,
    transport_not_configured = 91,
    transport_curl_failed = 92,
    transport_tls_error = 93,
    transport_http_error = 94,
    transport_timeout = 95,
    transport_invalid_response = 96,
    transport_response_too_large = 97,

    // --- registry response (100-109) ---
    registry_failure_response = 100,
    registry_partial_success = 101,

    // --- Consumer / ITI-43 (110-119) ---
    consumer_retrieve_missing_document_id = 110,
    consumer_retrieve_missing_repository_id = 111,
    consumer_response_missing_mtom_root = 112,
    consumer_response_mtom_malformed = 113,
    consumer_response_document_not_found = 114,
    consumer_signature_verification_failed = 115,
    consumer_signature_missing = 116,

    // --- Registry Query / ITI-18 (120-129) ---
    consumer_query_missing_patient_id = 120,
    consumer_query_empty_uuid_list = 121,
    consumer_query_response_parse_failed = 122,
};

/**
 * @brief Short identifier used as the "source" field of make_error.
 *
 * Kept as a single constexpr view so callers grepping logs see one tag.
 * Historical value was "ihe.xds.document_source" (Issue #1128). Generalized
 * to "ihe.xds" in Issue #1129 when the Document Consumer landed; both actors
 * share the tag so logs filter cleanly on a single prefix.
 */
inline constexpr std::string_view error_source = "ihe.xds";

}  // namespace kcenon::pacs::ihe::xds
