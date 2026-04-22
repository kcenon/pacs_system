// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file document_source.cpp
 * @brief IHE XDS.b Document Source actor (ITI-41).
 */

#include "kcenon/pacs/ihe/xds/document_source.h"

#include "common/http_client.h"
#include "common/mtom_packager.h"
#include "common/soap_envelope.h"
#include "common/wss_signer.h"

#include <pugixml.hpp>

#include <chrono>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <utility>

namespace kcenon::pacs::ihe::xds {

namespace {

std::string make_cid(std::size_t index) {
    static thread_local std::mt19937_64 rng(
        static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<std::uint64_t> dist;
    std::ostringstream oss;
    oss << "doc-" << index << '-' << std::hex << dist(rng)
        << "@kcenon.pacs";
    return oss.str();
}

}  // namespace

class document_source::impl {
public:
    impl(http_options opts, signing_material signing)
        : opts_(std::move(opts)), signing_(std::move(signing)) {}

    kcenon::common::Result<submit_response> submit(const submission_set& s) {
        std::vector<std::string> cids;
        cids.reserve(s.documents.size());
        for (std::size_t i = 0; i < s.documents.size(); ++i) {
            cids.push_back(make_cid(i));
        }

        auto env_result = detail::build_iti41_envelope(s, cids);
        if (env_result.is_err()) {
            return kcenon::common::make_error<submit_response>(
                env_result.error());
        }
        auto env = env_result.value();

        auto sign_result = detail::sign_envelope(
            env, signing_.certificate_pem, signing_.private_key_pem,
            signing_.private_key_password);
        if (sign_result.is_err()) {
            return kcenon::common::make_error<submit_response>(
                sign_result.error());
        }

        std::vector<detail::mtom_part> parts;
        parts.reserve(s.documents.size());
        for (std::size_t i = 0; i < s.documents.size(); ++i) {
            detail::mtom_part p;
            p.content_id = cids[i];
            p.mime_type = s.documents[i].mime_type;
            p.content = s.documents[i].content;
            parts.push_back(std::move(p));
        }

        auto mtom_result = detail::package_mtom(env.xml, parts);
        if (mtom_result.is_err()) {
            return kcenon::common::make_error<submit_response>(
                mtom_result.error());
        }
        auto mtom = mtom_result.value();

        auto http_result =
            detail::http_post(opts_, mtom.content_type, mtom.body);
        if (http_result.is_err()) {
            return kcenon::common::make_error<submit_response>(
                http_result.error());
        }
        auto response = http_result.value();

        return parse_registry_response(response.body,
                                       s.submission_set_unique_id);
    }

private:
    // Belt-and-suspenders cap. The HTTP layer already enforces 8 MiB via
    // the write callback, so any response reaching us here is already
    // bounded. This defensive check survives future refactors that might
    // add a non-libcurl transport path without a size cap.
    static constexpr std::size_t kMaxRegistryResponseBytes =
        8 * 1024 * 1024;

    kcenon::common::Result<submit_response> parse_registry_response(
        const std::string& body, const std::string& submission_set_uid) {
        if (body.size() > kMaxRegistryResponseBytes) {
            return kcenon::common::make_error<submit_response>(
                static_cast<int>(error_code::transport_response_too_large),
                "RegistryResponse exceeds 8 MiB cap (" +
                    std::to_string(body.size()) + " bytes) - rejected to "
                    "bound pugixml memory footprint",
                std::string(error_source));
        }
        pugi::xml_document doc;
        auto parse = doc.load_buffer(body.data(), body.size(),
                                     pugi::parse_default, pugi::encoding_utf8);
        if (!parse) {
            return kcenon::common::make_error<submit_response>(
                static_cast<int>(error_code::transport_invalid_response),
                std::string("failed to parse RegistryResponse: ") +
                    parse.description(),
                std::string(error_source));
        }

        auto registry_response =
            doc.select_node(
                   "//*[local-name()='RegistryResponse' or "
                   "local-name()='RegistryResponseType']")
                .node();
        if (!registry_response) {
            return kcenon::common::make_error<submit_response>(
                static_cast<int>(error_code::transport_invalid_response),
                "response contains no RegistryResponse element",
                std::string(error_source));
        }

        const std::string status =
            registry_response.attribute("status").as_string();
        if (status.empty()) {
            return kcenon::common::make_error<submit_response>(
                static_cast<int>(error_code::transport_invalid_response),
                "RegistryResponse is missing status attribute",
                std::string(error_source));
        }

        static constexpr std::string_view success_status =
            "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success";
        static constexpr std::string_view partial_success_status =
            "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:PartialSuccess";

        submit_response out;
        out.registry_response_status = status;
        out.submission_set_unique_id = submission_set_uid;

        if (status == success_status) {
            return out;
        }
        if (status == partial_success_status) {
            return kcenon::common::make_error<submit_response>(
                static_cast<int>(error_code::registry_partial_success),
                "registry reported PartialSuccess: " + status,
                std::string(error_source));
        }
        return kcenon::common::make_error<submit_response>(
            static_cast<int>(error_code::registry_failure_response),
            "registry reported non-success status: " + status,
            std::string(error_source));
    }

    http_options opts_;
    signing_material signing_;
};

document_source::document_source(http_options opts, signing_material signing)
    : impl_(std::make_unique<impl>(std::move(opts), std::move(signing))) {}

document_source::document_source(document_source&&) noexcept = default;
document_source& document_source::operator=(document_source&&) noexcept = default;

document_source::~document_source() = default;

kcenon::common::Result<submit_response> document_source::submit(
    const submission_set& s) {
    return impl_->submit(s);
}

}  // namespace kcenon::pacs::ihe::xds
