// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file registry_query.cpp
 * @brief IHE XDS.b Registry Query actor (ITI-18).
 */

#include "kcenon/pacs/ihe/xds/registry_query.h"

#include "common/http_client.h"
#include "common/soap_envelope.h"
#include "common/wss_signer.h"
#include "registry_query/query_envelope.h"
#include "registry_query/query_response_parser.h"

#include <string>
#include <utility>
#include <vector>

namespace kcenon::pacs::ihe::xds {

namespace {

// ITI-18 RegistryStoredQuery SOAP action URI. The default
// http_options::soap_action is the ITI-41 action, which would make the
// registry reject the POST on the first line of WS-Addressing routing.
// Registry Query overwrites it on a copy of the caller's options so the
// caller's struct is preserved across calls.
constexpr const char* kIti18SoapAction =
    "urn:ihe:iti:2007:RegistryStoredQuery";

// Content-Type of a plain SOAP 1.2 request. ITI-18 carries no attachments
// in either direction, so the wire is a single application/soap+xml part.
// Composed at compile time from kIti18SoapAction so the action URI is not
// duplicated (review m5).
constexpr const char* kSoapContentType =
    "application/soap+xml; charset=UTF-8; "
    "action=\"urn:ihe:iti:2007:RegistryStoredQuery\"";

// Public-API input-validation predicate: rejects ebRIM StoredQuery
// slot-literal metacharacters. Kept at the public boundary so callers
// see the invalid-input error without a transport or envelope call,
// and so the rejection is visible to auditors tracing the public
// surface (review M1). The envelope builder keeps its own copy of this
// check as defense in depth.
bool contains_slot_literal_metachar(std::string_view v) {
    for (char c : v) {
        if (c == '\'' || c == '(' || c == ')' || c == ',') return true;
        if (static_cast<unsigned char>(c) < 0x20) return true;
    }
    return false;
}

// Map the generic envelope signer/verifier failure codes to the
// query-specific namespace so callers can branch on
// registry_query_signature_* without pulling the Consumer vocabulary.
int map_signature_error(int raw) {
    if (raw ==
        static_cast<int>(error_code::consumer_signature_verification_failed)) {
        return static_cast<int>(
            error_code::registry_query_signature_verification_failed);
    }
    if (raw == static_cast<int>(error_code::consumer_signature_missing)) {
        return static_cast<int>(error_code::registry_query_signature_missing);
    }
    return raw;
}

}  // namespace

class registry_query::impl {
public:
    impl(http_options opts, signing_material signing)
        : opts_(std::move(opts)), signing_(std::move(signing)) {}

    kcenon::common::Result<registry_query_result> find_documents(
        const std::string& patient_id,
        const registry_query_options& options) {
        if (patient_id.empty()) {
            return kcenon::common::make_error<registry_query_result>(
                static_cast<int>(
                    error_code::registry_query_missing_patient_id),
                "find_documents: patient_id is empty",
                std::string(error_source));
        }
        if (contains_slot_literal_metachar(patient_id)) {
            return kcenon::common::make_error<registry_query_result>(
                static_cast<int>(
                    error_code::registry_query_invalid_patient_id),
                "find_documents: patient_id contains a character that is "
                "not legal inside an ebRIM StoredQuery slot literal "
                "('(),\\' or control character)",
                std::string(error_source));
        }
        for (const auto& s : options.status_values) {
            if (contains_slot_literal_metachar(s)) {
                return kcenon::common::make_error<registry_query_result>(
                    static_cast<int>(
                        error_code::registry_query_invalid_patient_id),
                    "find_documents: options.status_values contains a "
                    "slot-literal metacharacter",
                    std::string(error_source));
            }
        }
        if (contains_slot_literal_metachar(options.creation_time_from) ||
            contains_slot_literal_metachar(options.creation_time_to)) {
            return kcenon::common::make_error<registry_query_result>(
                static_cast<int>(
                    error_code::registry_query_invalid_patient_id),
                "find_documents: options.creation_time_* contains a "
                "slot-literal metacharacter",
                std::string(error_source));
        }

        registry_query_request req;
        req.patient_id = patient_id;
        req.options = options;

        return execute(req, detail::stored_query_kind::find_documents);
    }

    kcenon::common::Result<registry_query_result> get_documents(
        const std::vector<std::string>& uuids) {
        if (uuids.empty()) {
            return kcenon::common::make_error<registry_query_result>(
                static_cast<int>(
                    error_code::registry_query_empty_uuid_list),
                "get_documents: uuids is empty",
                std::string(error_source));
        }
        for (const auto& u : uuids) {
            if (u.empty()) {
                return kcenon::common::make_error<registry_query_result>(
                    static_cast<int>(
                        error_code::registry_query_missing_document_uuid),
                    "get_documents: uuids contains an empty element",
                    std::string(error_source));
            }
            if (contains_slot_literal_metachar(u)) {
                return kcenon::common::make_error<registry_query_result>(
                    static_cast<int>(
                        error_code::registry_query_missing_document_uuid),
                    "get_documents: uuids contains an element with a "
                    "slot-literal metacharacter",
                    std::string(error_source));
            }
        }

        registry_query_request req;
        req.document_uuids = uuids;

        return execute(req, detail::stored_query_kind::get_documents);
    }

private:
    kcenon::common::Result<registry_query_result> execute(
        const registry_query_request& req,
        detail::stored_query_kind kind) {
        auto env_result = detail::build_iti18_envelope(req, kind);
        if (env_result.is_err()) {
            return kcenon::common::make_error<registry_query_result>(
                env_result.error());
        }
        auto env = env_result.value();

        auto sign_result = detail::sign_envelope(
            env, signing_.certificate_pem, signing_.private_key_pem,
            signing_.private_key_password);
        if (sign_result.is_err()) {
            return kcenon::common::make_error<registry_query_result>(
                sign_result.error());
        }

        // Per-call copy: the ITI-18 action is specific to this transaction
        // and must not mutate the shared opts_ member, so other transaction
        // types sharing the same http_options template do not pick up a
        // stale action from a previous call.
        http_options call_opts = opts_;
        call_opts.soap_action = kIti18SoapAction;

        auto http_result =
            detail::http_post(call_opts, kSoapContentType, env.xml);
        if (http_result.is_err()) {
            return kcenon::common::make_error<registry_query_result>(
                http_result.error());
        }
        auto response = http_result.value();

        // Verify the signature on the response envelope before trusting
        // its status attribute or any entry it carries. An unsigned
        // "Failure" response could otherwise be used to deny service or
        // mask real successful queries; this check short-circuits before
        // parse_query_response sees the body.
        auto verify = detail::verify_envelope_integrity(response.body);
        if (verify.is_err()) {
            auto err = verify.error();
            err.code = map_signature_error(err.code);
            return kcenon::common::make_error<registry_query_result>(err);
        }

        return detail::parse_query_response(response.body);
    }

    http_options opts_;
    signing_material signing_;
};

registry_query::registry_query(http_options opts, signing_material signing)
    : impl_(std::make_unique<impl>(std::move(opts), std::move(signing))) {}

registry_query::registry_query(registry_query&&) noexcept = default;
registry_query& registry_query::operator=(registry_query&&) noexcept = default;

registry_query::~registry_query() = default;

kcenon::common::Result<registry_query_result> registry_query::find_documents(
    const std::string& patient_id, const registry_query_options& options) {
    return impl_->find_documents(patient_id, options);
}

kcenon::common::Result<registry_query_result> registry_query::get_documents(
    const std::vector<std::string>& uuids) {
    return impl_->get_documents(uuids);
}

}  // namespace kcenon::pacs::ihe::xds
