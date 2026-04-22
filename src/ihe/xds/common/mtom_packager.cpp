// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file mtom_packager.cpp
 * @brief multipart/related + XOP/MTOM body assembly.
 */

#include "mtom_packager.h"

#include <chrono>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>

namespace kcenon::pacs::ihe::xds::detail {

namespace {

constexpr const char* kCrLf = "\r\n";
constexpr const char* kSoapStartCid = "root.message@cxf.apache.org";

std::string make_boundary() {
    static thread_local std::mt19937_64 rng(
        static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<std::uint64_t> dist;
    std::ostringstream oss;
    oss << "----=_Part_kcenon_" << std::hex << dist(rng) << dist(rng);
    return oss.str();
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

bool bytes_contain(const std::vector<std::uint8_t>& haystack,
                   const std::string& needle) {
    if (haystack.size() < needle.size()) return false;
    const auto n = needle.size();
    for (std::size_t i = 0; i + n <= haystack.size(); ++i) {
        if (std::equal(needle.begin(), needle.end(),
                       haystack.begin() + static_cast<std::ptrdiff_t>(i))) {
            return true;
        }
    }
    return false;
}

}  // namespace

kcenon::common::Result<packaged_mtom> package_mtom(
    const std::string& envelope_xml, const std::vector<mtom_part>& parts) {
    if (envelope_xml.empty()) {
        return kcenon::common::make_error<packaged_mtom>(
            static_cast<int>(error_code::mtom_empty_payload),
            "envelope XML is empty", std::string(error_source));
    }

    std::string boundary;
    bool ok = false;
    for (int attempt = 0; attempt < 8; ++attempt) {
        boundary = make_boundary();
        if (contains(envelope_xml, boundary)) continue;
        bool collides = false;
        for (const auto& p : parts) {
            if (bytes_contain(p.content, boundary)) {
                collides = true;
                break;
            }
        }
        if (!collides) {
            ok = true;
            break;
        }
    }
    if (!ok) {
        return kcenon::common::make_error<packaged_mtom>(
            static_cast<int>(error_code::mtom_boundary_generation_failed),
            "failed to generate a collision-free MIME boundary",
            std::string(error_source));
    }

    packaged_mtom out;
    out.content_type =
        std::string("multipart/related; boundary=\"") + boundary +
        std::string("\"; type=\"application/xop+xml\"; start=\"<") +
        kSoapStartCid +
        std::string(">\"; start-info=\"application/soap+xml; action=\\\"urn:ihe:iti:2007:RegisterDocumentSet-b\\\"\"");

    std::string body;
    body.reserve(envelope_xml.size() + 4096);

    body += "--";
    body += boundary;
    body += kCrLf;
    body += "Content-Type: application/xop+xml; charset=UTF-8; type=\"application/soap+xml\"";
    body += kCrLf;
    body += "Content-Transfer-Encoding: 8bit";
    body += kCrLf;
    body += "Content-ID: <";
    body += kSoapStartCid;
    body += ">";
    body += kCrLf;
    body += kCrLf;
    body += envelope_xml;
    body += kCrLf;

    for (const auto& p : parts) {
        if (p.content_id.empty()) {
            return kcenon::common::make_error<packaged_mtom>(
                static_cast<int>(error_code::mtom_empty_payload),
                "part has empty content-id", std::string(error_source));
        }
        if (p.content.empty()) {
            return kcenon::common::make_error<packaged_mtom>(
                static_cast<int>(error_code::mtom_empty_payload),
                "part '" + p.content_id + "' has empty content",
                std::string(error_source));
        }

        body += "--";
        body += boundary;
        body += kCrLf;
        body += "Content-Type: ";
        body += (p.mime_type.empty() ? "application/octet-stream" : p.mime_type);
        body += kCrLf;
        body += "Content-Transfer-Encoding: binary";
        body += kCrLf;
        body += "Content-ID: <";
        body += p.content_id;
        body += ">";
        body += kCrLf;
        body += kCrLf;
        body.append(reinterpret_cast<const char*>(p.content.data()),
                    p.content.size());
        body += kCrLf;
    }

    body += "--";
    body += boundary;
    body += "--";
    body += kCrLf;

    out.body = std::move(body);
    return out;
}

}  // namespace kcenon::pacs::ihe::xds::detail
