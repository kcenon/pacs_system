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

namespace kcenon::pacs::ihe::xds {

namespace {

constexpr const char* kIti18SoapAction =
    "urn:ihe:iti:2007:RegistryStoredQuery";

// Plain SOAP 1.2 content-type; ITI-18 carries no MTOM attachments on
// either direction.
constexpr const char* kSoapContentType =
    "application/soap+xml; charset=UTF-8; "
    "action=\"urn:ihe:iti:2007:RegistryStoredQuery\"";

kcenon::common::Result<registry_query_result> send_and_parse(
    const http_options& opts, const signing_material& signing,
    detail::built_envelope env) {
    auto sign_result = detail::sign_envelope(
        env, signing.certificate_pem, signing.private_key_pem,
        signing.private_key_password);
    if (sign_result.is_err()) {
        return kcenon::common::make_error<registry_query_result>(
            sign_result.error());
    }

    // Per-call copy: the ITI-18 action URI is specific to this
    // transaction and must not bleed into the caller's shared options.
    http_options call_opts = opts;
    call_opts.soap_action = kIti18SoapAction;

    auto http_result =
        detail::http_post(call_opts, kSoapContentType, env.xml);
    if (http_result.is_err()) {
        return kcenon::common::make_error<registry_query_result>(
            http_result.error());
    }

    return detail::parse_query_response(http_result.value().body);
}

}  // namespace

class registry_query::impl {
public:
    impl(http_options opts, signing_material signing)
        : opts_(std::move(opts)), signing_(std::move(signing)) {}

    kcenon::common::Result<registry_query_result> find_documents(
        const std::string& patient_id,
        const std::vector<std::string>& statuses) {
        auto env_result = detail::build_iti18_find_documents_envelope(
            patient_id, statuses);
        if (env_result.is_err()) {
            return kcenon::common::make_error<registry_query_result>(
                env_result.error());
        }
        return send_and_parse(opts_, signing_, env_result.value());
    }

    kcenon::common::Result<registry_query_result> get_documents(
        const std::vector<std::string>& uuids) {
        auto env_result =
            detail::build_iti18_get_documents_envelope(uuids);
        if (env_result.is_err()) {
            return kcenon::common::make_error<registry_query_result>(
                env_result.error());
        }
        return send_and_parse(opts_, signing_, env_result.value());
    }

private:
    http_options opts_;
    signing_material signing_;
};

registry_query::registry_query(http_options opts, signing_material signing)
    : impl_(std::make_unique<impl>(std::move(opts), std::move(signing))) {}

registry_query::registry_query(registry_query&&) noexcept = default;
registry_query& registry_query::operator=(registry_query&&) noexcept = default;

registry_query::~registry_query() = default;

kcenon::common::Result<registry_query_result> registry_query::find_documents(
    const std::string& patient_id,
    const std::vector<std::string>& statuses) {
    return impl_->find_documents(patient_id, statuses);
}

kcenon::common::Result<registry_query_result> registry_query::get_documents(
    const std::vector<std::string>& uuids) {
    return impl_->get_documents(uuids);
}

}  // namespace kcenon::pacs::ihe::xds
