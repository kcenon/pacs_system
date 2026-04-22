// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file mtom_packager.cpp
 * @brief multipart/related + XOP/MTOM body assembly.
 */

#include "mtom_packager.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

namespace {

// Locate the MIME boundary in a body. The first "--" followed by at least
// one non-whitespace byte and terminated by CR/LF (or the end of the part
// separator "--") is the boundary. Returns the boundary *without* leading
// "--".
std::string detect_boundary(const std::string& body) {
    const auto pos = body.find("--");
    if (pos == std::string::npos) return {};
    const std::size_t start = pos + 2;
    std::size_t end = start;
    while (end < body.size()) {
        const char c = body[end];
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t' || c == ';' ||
            c == '"') {
            break;
        }
        ++end;
    }
    if (end == start) return {};
    return body.substr(start, end - start);
}

// Case-insensitive prefix check on the ASCII subset of the MIME headers
// we care about.
bool iequals_prefix(std::string_view line, std::string_view prefix) {
    if (line.size() < prefix.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        const char a = line[i];
        const char b = prefix[i];
        const char la = (a >= 'A' && a <= 'Z') ? static_cast<char>(a + 32) : a;
        const char lb = (b >= 'A' && b <= 'Z') ? static_cast<char>(b + 32) : b;
        if (la != lb) return false;
    }
    return true;
}

std::string trim(std::string_view s) {
    std::size_t start = 0;
    std::size_t end = s.size();
    while (start < end && (s[start] == ' ' || s[start] == '\t')) ++start;
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' ||
                           s[end - 1] == '\r' || s[end - 1] == '\n')) {
        --end;
    }
    return std::string(s.substr(start, end - start));
}

// Strip angle brackets around a content-id value, so we can match it
// against xop:Include href="cid:..." references that have no brackets.
std::string strip_cid_brackets(std::string_view raw) {
    std::string out = trim(raw);
    if (out.size() >= 2 && out.front() == '<' && out.back() == '>') {
        out = out.substr(1, out.size() - 2);
    }
    return out;
}

}  // namespace

kcenon::common::Result<parsed_mtom> parse_mtom_response(
    const std::string& body) {
    parsed_mtom out;

    if (body.empty()) {
        return kcenon::common::make_error<parsed_mtom>(
            static_cast<int>(error_code::consumer_response_mtom_malformed),
            "response body is empty", std::string(error_source));
    }

    // Plain SOAP (no multipart framing) fallback: if the body starts with
    // "<?xml" or "<soap" it is the envelope itself and we surface it as a
    // root-only parsed_mtom. This keeps the caller's pipeline uniform.
    {
        std::size_t leading = 0;
        while (leading < body.size() &&
               (body[leading] == ' ' || body[leading] == '\t' ||
                body[leading] == '\r' || body[leading] == '\n')) {
            ++leading;
        }
        if (body.compare(leading, 5, "<?xml") == 0 ||
            body.compare(leading, 5, "<soap") == 0 ||
            body.compare(leading, 14, "<soap:Envelope") == 0) {
            out.root_xml = body;
            return out;
        }
    }

    const std::string boundary = detect_boundary(body);
    if (boundary.empty()) {
        return kcenon::common::make_error<parsed_mtom>(
            static_cast<int>(error_code::consumer_response_mtom_malformed),
            "no MIME boundary detected in response body",
            std::string(error_source));
    }

    const std::string delim = "--" + boundary;

    // Split body on every occurrence of "--<boundary>". The last segment
    // before "--<boundary>--" is the final part; anything after the closer
    // is the epilogue (ignored).
    std::vector<std::string> segments;
    std::size_t cursor = 0;
    while (cursor < body.size()) {
        const auto next = body.find(delim, cursor);
        if (next == std::string::npos) break;
        if (next > cursor) {
            segments.push_back(body.substr(cursor, next - cursor));
        }
        cursor = next + delim.size();
        // Close marker: "--<boundary>--"
        if (cursor + 2 <= body.size() && body[cursor] == '-' &&
            body[cursor + 1] == '-') {
            cursor += 2;
            break;
        }
    }
    if (segments.empty()) {
        // The preamble before the first boundary is allowed by RFC 2046,
        // so an "empty" segments list only means the body had no proper
        // first-part marker.
        return kcenon::common::make_error<parsed_mtom>(
            static_cast<int>(error_code::consumer_response_mtom_malformed),
            "multipart body yielded zero parts",
            std::string(error_source));
    }

    // Discard any preamble-only first segment that isn't actually a part
    // (i.e. the bytes between the MIME body start and the first "--B").
    // Our parser starts cursor at 0, so the first segment is whatever
    // preceded the first boundary - usually empty after CRLFs. A real part
    // always has a header-body split we can detect.
    bool first_part_consumed = false;
    for (auto& seg : segments) {
        // A valid MIME part starts with CRLF, then headers, then CRLF CRLF,
        // then content, then CRLF. Strip the leading CRLF if present.
        std::size_t start = 0;
        while (start < seg.size() &&
               (seg[start] == '\r' || seg[start] == '\n')) {
            ++start;
        }
        if (start >= seg.size()) continue;

        const auto header_end = seg.find("\r\n\r\n", start);
        const std::size_t content_start =
            (header_end == std::string::npos) ? seg.size() : header_end + 4;
        if (content_start > seg.size()) continue;

        // Parse headers.
        const std::string_view header_block(
            seg.data() + start,
            (header_end == std::string::npos ? seg.size() - start
                                             : header_end - start));
        std::string mime_type;
        std::string content_id;
        std::size_t pos = 0;
        while (pos < header_block.size()) {
            const auto eol = header_block.find("\r\n", pos);
            const std::size_t line_end =
                (eol == std::string::npos) ? header_block.size() : eol;
            const std::string_view line = header_block.substr(pos, line_end - pos);
            if (iequals_prefix(line, "Content-Type:")) {
                mime_type = trim(line.substr(std::strlen("Content-Type:")));
                const auto semi = mime_type.find(';');
                if (semi != std::string::npos) {
                    mime_type = trim(std::string_view(mime_type).substr(0, semi));
                }
            } else if (iequals_prefix(line, "Content-ID:")) {
                content_id = strip_cid_brackets(
                    line.substr(std::strlen("Content-ID:")));
            }
            if (eol == std::string::npos) break;
            pos = eol + 2;
        }

        // Extract content, trimming the trailing CRLF that precedes the
        // next boundary delimiter.
        std::size_t content_end = seg.size();
        while (content_end > content_start &&
               (seg[content_end - 1] == '\r' ||
                seg[content_end - 1] == '\n')) {
            --content_end;
        }
        if (content_end < content_start) content_end = content_start;
        const std::size_t content_len = content_end - content_start;

        if (!first_part_consumed) {
            out.root_xml.assign(seg, content_start, content_len);
            first_part_consumed = true;
            continue;
        }
        mtom_part mp;
        mp.content_id = std::move(content_id);
        mp.mime_type = std::move(mime_type);
        mp.content.assign(
            reinterpret_cast<const std::uint8_t*>(seg.data() + content_start),
            reinterpret_cast<const std::uint8_t*>(seg.data() + content_end));
        out.parts.push_back(std::move(mp));
    }

    if (out.root_xml.empty()) {
        return kcenon::common::make_error<parsed_mtom>(
            static_cast<int>(error_code::consumer_response_missing_mtom_root),
            "multipart body had no root part with XML payload",
            std::string(error_source));
    }

    return out;
}

}  // namespace kcenon::pacs::ihe::xds::detail
