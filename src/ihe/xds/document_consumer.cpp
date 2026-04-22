// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file document_consumer.cpp
 * @brief IHE XDS.b Document Consumer actor (ITI-43).
 */

#include "kcenon/pacs/ihe/xds/document_consumer.h"

#include "common/http_client.h"
#include "common/mtom_packager.h"
#include "common/soap_envelope.h"
#include "common/wss_signer.h"
#include "consumer/retrieve_envelope.h"
#include "consumer/retrieve_response_parser.h"

#include <string>
#include <utility>

namespace kcenon::pacs::ihe::xds {

namespace {

// ITI-43 Retrieve Document Set-b SOAP action URI. The default
// http_options::soap_action is the ITI-41 action, which would make the
// registry reject the POST on the first line of WS-Addressing routing. The
// Consumer overwrites it on a copy of the caller's options so the caller's
// struct is preserved.
constexpr const char* kIti43SoapAction = "urn:ihe:iti:2007:RetrieveDocumentSet";

// Content-Type of a plain SOAP 1.2 request with no MTOM attachments. ITI-43
// requests carry only the envelope; MTOM only appears on the response.
constexpr const char* kSoapContentType =
    "application/soap+xml; charset=UTF-8; "
    "action=\"urn:ihe:iti:2007:RetrieveDocumentSet\"";

}  // namespace

class document_consumer::impl {
public:
    impl(http_options opts, signing_material signing)
        : opts_(std::move(opts)), signing_(std::move(signing)) {
        opts_.soap_action = kIti43SoapAction;
    }

    kcenon::common::Result<document_response> retrieve(
        const std::string& document_unique_id,
        const std::string& repository_unique_id) {
        if (document_unique_id.empty()) {
            return kcenon::common::make_error<document_response>(
                static_cast<int>(
                    error_code::consumer_retrieve_missing_document_id),
                "retrieve: document_unique_id is empty",
                std::string(error_source));
        }
        if (repository_unique_id.empty()) {
            return kcenon::common::make_error<document_response>(
                static_cast<int>(
                    error_code::consumer_retrieve_missing_repository_id),
                "retrieve: repository_unique_id is empty",
                std::string(error_source));
        }

        retrieve_request req;
        req.document_unique_id = document_unique_id;
        req.repository_unique_id = repository_unique_id;

        auto env_result = detail::build_iti43_envelope(req);
        if (env_result.is_err()) {
            return kcenon::common::make_error<document_response>(
                env_result.error());
        }
        auto env = env_result.value();

        auto sign_result = detail::sign_envelope(
            env, signing_.certificate_pem, signing_.private_key_pem,
            signing_.private_key_password);
        if (sign_result.is_err()) {
            return kcenon::common::make_error<document_response>(
                sign_result.error());
        }

        auto http_result =
            detail::http_post(opts_, kSoapContentType, env.xml);
        if (http_result.is_err()) {
            return kcenon::common::make_error<document_response>(
                http_result.error());
        }
        auto response = http_result.value();

        auto parsed = detail::parse_mtom_response(response.body);
        if (parsed.is_err()) {
            return kcenon::common::make_error<document_response>(
                parsed.error());
        }
        auto mtom = parsed.value();

        auto verify = detail::verify_envelope(mtom.root_xml);
        if (verify.is_err()) {
            return kcenon::common::make_error<document_response>(
                verify.error());
        }

        return detail::parse_retrieve_response(mtom.root_xml, mtom.parts,
                                               req);
    }

private:
    http_options opts_;
    signing_material signing_;
};

document_consumer::document_consumer(http_options opts,
                                     signing_material signing)
    : impl_(std::make_unique<impl>(std::move(opts), std::move(signing))) {}

document_consumer::document_consumer(document_consumer&&) noexcept = default;
document_consumer& document_consumer::operator=(
    document_consumer&&) noexcept = default;

document_consumer::~document_consumer() = default;

kcenon::common::Result<document_response> document_consumer::retrieve(
    const std::string& document_unique_id,
    const std::string& repository_unique_id) {
    return impl_->retrieve(document_unique_id, repository_unique_id);
}

}  // namespace kcenon::pacs::ihe::xds
