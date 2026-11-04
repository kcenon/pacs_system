// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file dicomweb_endpoints.cpp
 * @brief DICOMweb (WADO-RS) API endpoints implementation
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

// IMPORTANT: Include Crow FIRST before any PACS headers to avoid forward
// declaration conflicts
#include "crow.h"

#include "../rest_app.h"

// Workaround for Windows: DELETE is defined as a macro in <winnt.h>
// which conflicts with crow::HTTPMethod::DELETE
#ifdef DELETE
#undef DELETE
#endif

#include "kcenon/pacs/core/dicom_dataset.h"
#include "kcenon/pacs/core/dicom_dictionary.h"
#include "kcenon/pacs/core/dicom_element.h"
#include "kcenon/pacs/core/dicom_file.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/core/utf8_text.h"
#include "kcenon/pacs/encoding/compression/codec_factory.h"
#include "kcenon/pacs/encoding/compression/htj2k_codec.h"
#include "kcenon/pacs/encoding/compression/jpeg_baseline_codec.h"
#include "kcenon/pacs/encoding/transfer_syntax.h"
#include "kcenon/pacs/encoding/vr_type.h"
#include "kcenon/pacs/storage/file_storage.h"
#include "kcenon/pacs/services/qr_index.h"
#include "kcenon/pacs/storage/index_database.h"
#include "kcenon/pacs/storage/instance_archive.h"
#include "kcenon/pacs/storage/instance_record.h"
#include "kcenon/pacs/storage/patient_record.h"
#include "kcenon/pacs/storage/series_record.h"
#include "kcenon/pacs/storage/study_record.h"
#include "kcenon/pacs/web/auth/oauth2_middleware.h"
#include "kcenon/pacs/web/endpoints/dicomweb_endpoints.h"
#include "kcenon/pacs/web/endpoints/system_endpoints.h"
#include "kcenon/pacs/web/rest_config.h"
#include "kcenon/pacs/web/rest_types.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include <system_error>
#include <type_traits>

namespace kcenon::pacs::web::dicomweb {

namespace {

/**
 * @brief Trim whitespace from string
 */
std::string trim(std::string_view s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return "";
    }
    auto end = s.find_last_not_of(" \t\r\n");
    return std::string(s.substr(start, end - start + 1));
}

/**
 * @brief Split string by delimiter
 */
std::vector<std::string> split(std::string_view s, char delimiter) {
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = s.find(delimiter);

    while (end != std::string_view::npos) {
        result.push_back(std::string(s.substr(start, end - start)));
        start = end + 1;
        end = s.find(delimiter, start);
    }
    result.push_back(std::string(s.substr(start)));
    return result;
}

/**
 * @brief Convert VR enum to string representation
 */
std::string vr_enum_to_string(kcenon::pacs::encoding::vr_type vr) {
    using vr_type = kcenon::pacs::encoding::vr_type;
    switch (vr) {
    case vr_type::AE: return "AE";
    case vr_type::AS: return "AS";
    case vr_type::AT: return "AT";
    case vr_type::CS: return "CS";
    case vr_type::DA: return "DA";
    case vr_type::DS: return "DS";
    case vr_type::DT: return "DT";
    case vr_type::FD: return "FD";
    case vr_type::FL: return "FL";
    case vr_type::IS: return "IS";
    case vr_type::LO: return "LO";
    case vr_type::LT: return "LT";
    case vr_type::OB: return "OB";
    case vr_type::OD: return "OD";
    case vr_type::OF: return "OF";
    case vr_type::OL: return "OL";
    case vr_type::OW: return "OW";
    case vr_type::PN: return "PN";
    case vr_type::SH: return "SH";
    case vr_type::SL: return "SL";
    case vr_type::SQ: return "SQ";
    case vr_type::SS: return "SS";
    case vr_type::ST: return "ST";
    case vr_type::TM: return "TM";
    case vr_type::UC: return "UC";
    case vr_type::UI: return "UI";
    case vr_type::UL: return "UL";
    case vr_type::UN: return "UN";
    case vr_type::UR: return "UR";
    case vr_type::US: return "US";
    case vr_type::UT: return "UT";
    default: return "UN";
    }
}

} // anonymous namespace

// ============================================================================
// Accept Header Parsing
// ============================================================================

auto parse_accept_header(std::string_view accept_header)
    -> std::vector<accept_info> {
    std::vector<accept_info> result;

    if (accept_header.empty()) {
        // Default to application/dicom
        result.push_back({std::string(media_type::dicom), "", 1.0f});
        return result;
    }

    auto parts = split(accept_header, ',');
    for (const auto& part : parts) {
        accept_info info;
        auto params = split(part, ';');

        if (params.empty()) {
            continue;
        }

        info.media_type = trim(params[0]);
        std::transform(info.media_type.begin(), info.media_type.end(), info.media_type.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        for (size_t i = 1; i < params.size(); ++i) {
            auto param = trim(params[i]);
            if (param.starts_with("type=")) {
                info.type = param.substr(5);
                if (info.type.size() >= 2 && info.type.front() == '"' && info.type.back() == '"') {
                    info.type = info.type.substr(1, info.type.size() - 2);
                }
                std::transform(info.type.begin(), info.type.end(), info.type.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            } else if (param.starts_with("q=")) {
                try {
                    info.quality = std::stof(param.substr(2));
                } catch (...) {
                    info.quality = 1.0f;
                }
            } else if (param.starts_with("transfer-syntax=")) {
                info.transfer_syntax = param.substr(16);
                // Remove quotes if present
                if (!info.transfer_syntax.empty() &&
                    info.transfer_syntax.front() == '"') {
                    info.transfer_syntax = info.transfer_syntax.substr(
                        1, info.transfer_syntax.size() - 2);
                }
            }
        }

        result.push_back(std::move(info));
    }

    // Sort by quality (descending), keeping the client's order among equals
    std::stable_sort(result.begin(), result.end(),
              [](const accept_info& a, const accept_info& b) {
                  return a.quality > b.quality;
              });

    return result;
}

auto is_acceptable(const std::vector<accept_info>& accept_infos,
                   std::string_view media_type) -> bool {
    if (accept_infos.empty()) {
        return true; // Accept all if no Accept header
    }

    for (const auto& info : accept_infos) {
        if (info.media_type == "*/*" ||
            info.media_type == media_type ||
            (info.media_type.ends_with("/*") &&
             media_type.starts_with(
                 info.media_type.substr(0, info.media_type.size() - 1)))) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Media Type Selection
// ============================================================================

namespace {

/// Compressed Bulk Data media types and the Transfer Syntaxes each carries (PS3.18 8.7.3.3.2).
constexpr std::array<std::pair<std::string_view, std::string_view>, 14> compressed_media_types{{
    {"image/jpeg", "1.2.840.10008.1.2.4.50"},
    {"image/jpeg", "1.2.840.10008.1.2.4.51"},
    {"image/jpeg", "1.2.840.10008.1.2.4.57"},
    {"image/jpeg", "1.2.840.10008.1.2.4.70"},
    {"image/jls", "1.2.840.10008.1.2.4.80"},
    {"image/jls", "1.2.840.10008.1.2.4.81"},
    {"image/jp2", "1.2.840.10008.1.2.4.90"},
    {"image/jp2", "1.2.840.10008.1.2.4.91"},
    {"image/jphc", "1.2.840.10008.1.2.4.201"},
    {"image/jphc", "1.2.840.10008.1.2.4.202"},
    {"image/jphc", "1.2.840.10008.1.2.4.203"},
    {"image/jxl", "1.2.840.10008.1.2.4.110"},
    {"image/jxl", "1.2.840.10008.1.2.4.112"},
    {"image/dicom-rle", "1.2.840.10008.1.2.5"},
}};

[[nodiscard]] bool is_any(std::string_view media) {
    return media == "*/*";
}

[[nodiscard]] bool is_multipart_of(const accept_info& info, std::string_view part_type) {
    if (info.media_type != "multipart/related" && info.media_type != "multipart/*") {
        return false;
    }
    return info.type == part_type;
}

[[nodiscard]] bool is_untyped_multipart(const accept_info& info) {
    return (info.media_type == "multipart/related" || info.media_type == "multipart/*") && info.type.empty();
}

/// A rendered type by the family it belongs to; image/ types with a Transfer Syntax are Bulk Data instead.
[[nodiscard]] bool is_rendered_family(const accept_info& info) {
    return (info.media_type.starts_with("image/") || info.media_type.starts_with("video/")) && info.transfer_syntax.empty();
}

[[nodiscard]] bool is_dicom_family(const accept_info& info) {
    const std::string_view named = info.media_type == "multipart/related" && !info.type.empty() ? std::string_view(info.type)
                                                                                                 : std::string_view(info.media_type);
    return named.starts_with("application/dicom") || named == "application/octet-stream";
}

[[nodiscard]] media_choice accepted(std::string_view media, bool multipart, const accept_info& info) {
    media_choice choice;
    choice.status = 200;
    choice.media = std::string(media);
    choice.multipart = multipart;
    choice.transfer_syntax = info.transfer_syntax;
    return choice;
}

/// The media type a compressed frame goes out as, when the Accept entry allows it for what is held.
[[nodiscard]] std::optional<std::string_view> compressed_type_for(std::string_view asked, std::string_view syntax_asked,
                                                                  std::string_view held) {
    if (!syntax_asked.empty() && syntax_asked != "*" && syntax_asked != held) {
        return std::nullopt;
    }
    for (const auto& [media, syntax] : compressed_media_types) {
        if (syntax == held && (asked == media || asked == "image/*")) {
            return media;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<media_choice> pixel_choice(const accept_info& info, std::size_t items, std::string_view held) {
    const bool octet_syntax = info.transfer_syntax.empty() || info.transfer_syntax == "*" ||
                              info.transfer_syntax == encoding::transfer_syntax::explicit_vr_little_endian.uid();
    if (octet_syntax && (is_any(info.media_type) || is_untyped_multipart(info) || is_multipart_of(info, media_type::octet_stream))) {
        return accepted(media_type::octet_stream, true, info);
    }
    if (octet_syntax && items == 1 && (info.media_type == media_type::octet_stream || info.media_type == "application/*")) {
        return accepted(media_type::octet_stream, false, info);
    }
    if (info.media_type == "multipart/related" || info.media_type == "multipart/*") {
        if (const auto media = compressed_type_for(info.type, info.transfer_syntax, held)) {
            return accepted(*media, true, info);
        }
    } else if (items == 1) {
        if (const auto media = compressed_type_for(info.media_type, info.transfer_syntax, held)) {
            return accepted(*media, false, info);
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<media_choice> entry_choice(const accept_info& info, wado_resource resource, std::size_t items,
                                                       std::string_view held) {
    switch (resource) {
        case wado_resource::instance:
            if (info.media_type == media_type::dicom || info.media_type == "application/*") {
                return accepted(media_type::dicom, false, info);
            }
            [[fallthrough]];
        case wado_resource::collection:
            if (is_any(info.media_type) || is_untyped_multipart(info) || is_multipart_of(info, media_type::dicom) ||
                is_multipart_of(info, "application/*")) {
                return accepted(media_type::dicom, true, info);
            }
            if (is_multipart_of(info, media_type::octet_stream)) {
                return accepted(media_type::octet_stream, true, info);
            }
            return std::nullopt;
        case wado_resource::metadata:
        case wado_resource::search:
        case wado_resource::store_response:
            if (is_any(info.media_type) || info.media_type == "application/*" || info.media_type == media_type::dicom_json) {
                return accepted(media_type::dicom_json, false, info);
            }
            if (resource == wado_resource::store_response && info.media_type == media_type::dicom_xml) {
                return accepted(media_type::dicom_xml, false, info);
            }
            if (resource != wado_resource::store_response && is_multipart_of(info, media_type::dicom_xml)) {
                return accepted(media_type::dicom_xml, true, info);
            }
            return std::nullopt;
        case wado_resource::frames:
            return pixel_choice(info, items, held);
        case wado_resource::bulk_data:
            return pixel_choice(info, 1, {});
        case wado_resource::rendered:
            if (is_any(info.media_type) || info.media_type == "image/*" || info.media_type == media_type::jpeg) {
                return accepted(media_type::jpeg, false, info);
            }
            if (info.media_type == media_type::png || info.media_type == media_type::jphc) {
                return accepted(info.media_type, false, info);
            }
            return std::nullopt;
    }
    return std::nullopt;
}

}  // namespace

auto choose_media_type(std::string_view accept_header, wado_resource resource, std::size_t items, std::string_view held_syntax)
    -> media_choice {
    media_choice choice;
    if (trim(accept_header).empty()) {
        choice.status = 406;
        choice.detail = "A request without an Accept header is not acceptable (PS3.18 8.7.5)";
        return choice;
    }
    const auto entries = parse_accept_header(accept_header);
    const bool dicom = std::any_of(entries.begin(), entries.end(), is_dicom_family);
    const bool rendered = std::any_of(entries.begin(), entries.end(), is_rendered_family);
    if (dicom && rendered) {
        choice.status = 400;
        choice.detail = "Acceptable media types shall not mix DICOM and rendered types (PS3.18 8.7.5)";
        return choice;
    }
    for (const auto& entry : entries) {
        if (entry.quality <= 0.0f) {
            continue;  // q=0 says "not this one" (RFC 9110 12.4.2)
        }
        if (!entry.transfer_syntax.empty() && entry.transfer_syntax != "*" && !storage::is_valid_uid(entry.transfer_syntax)) {
            choice.status = 400;
            choice.detail = "Not a Transfer Syntax UID: "+entry.transfer_syntax;
            return choice;
        }
        if (auto selected = entry_choice(entry, resource, items, held_syntax)) {
            return *selected;
        }
    }
    choice.status = 406;
    choice.detail = "None of the acceptable media types can represent this resource";
    return choice;
}

// ============================================================================
// Multipart Builder Implementation
// ============================================================================

multipart_builder::multipart_builder(std::string_view content_type)
    : boundary_(generate_boundary()),
      default_content_type_(content_type) {}

void multipart_builder::add_part(
    std::vector<uint8_t> data,
    std::optional<std::string_view> content_type) {
    part p;
    p.data = std::move(data);
    p.content_type = content_type.has_value()
                         ? std::string(*content_type)
                         : default_content_type_;
    parts_.push_back(std::move(p));
}

void multipart_builder::add_part_with_location(
    std::vector<uint8_t> data,
    std::string_view location,
    std::optional<std::string_view> content_type) {
    part p;
    p.data = std::move(data);
    p.content_type = content_type.has_value()
                         ? std::string(*content_type)
                         : default_content_type_;
    p.location = std::string(location);
    parts_.push_back(std::move(p));
}

auto multipart_builder::build() const -> std::string {
    std::ostringstream oss;

    for (const auto& p : parts_) {
        oss << "--" << boundary_ << "\r\n";
        oss << "Content-Type: " << p.content_type << "\r\n";
        if (!p.location.empty()) {
            oss << "Content-Location: " << p.location << "\r\n";
        }
        oss << "\r\n";
        oss.write(reinterpret_cast<const char*>(p.data.data()),
                  static_cast<std::streamsize>(p.data.size()));
        oss << "\r\n";
    }

    if (!parts_.empty()) {
        oss << "--" << boundary_ << "--\r\n";
    }

    return oss.str();
}

auto multipart_builder::content_type_header() const -> std::string {
    std::ostringstream oss;
    oss << "multipart/related; type=\"" << default_content_type_
        << "\"; boundary=\"" << boundary_ << "\"";
    return oss.str();
}

auto multipart_builder::boundary() const -> std::string_view {
    return boundary_;
}

auto multipart_builder::empty() const noexcept -> bool {
    return parts_.empty();
}

auto multipart_builder::size() const noexcept -> size_t {
    return parts_.size();
}

auto multipart_builder::generate_boundary() -> std::string {
    // Generate a unique boundary using timestamp and random number
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                         now.time_since_epoch())
                         .count();

    std::random_device rd;
    std::mt19937_64 gen((static_cast<uint64_t>(rd()) << 32) | rd());

    std::ostringstream oss;
    oss << "----=_Part_" << timestamp << "_" << std::hex << gen() << gen();
    return oss.str();
}

// ============================================================================
// DicomJSON Conversion
// ============================================================================

auto is_bulk_data_tag(uint32_t tag) -> bool {
    // Pixel Data and related bulk data tags
    uint16_t group = static_cast<uint16_t>(tag >> 16);

    // Audio Sample Data is in group 0x50xx (curve data)
    if (group >= 0x5000 && group <= 0x50FF) {
        uint16_t element = static_cast<uint16_t>(tag & 0xFFFF);
        if (element == 0x3000) {
            return true;  // Audio Sample Data
        }
    }

    return tag == 0x7FE00010 ||  // Pixel Data
           tag == 0x7FE00008 ||  // Float Pixel Data
           tag == 0x7FE00009 ||  // Double Float Pixel Data
           tag == 0x00420011 ||  // Encapsulated Document
           tag == 0x00660023 ||  // Triangle Point Index List
           tag == 0x00660024 ||  // Edge Point Index List
           tag == 0x00660025 ||  // Vertex Point Index List
           tag == 0x00660026 ||  // Triangle Strip Sequence
           tag == 0x00660027 ||  // Triangle Fan Sequence
           tag == 0x00660028 ||  // Line Sequence
           tag == 0x00660029;    // Primitive Point Index List
}

auto vr_to_string(uint16_t vr_code) -> std::string {
    // Convert 2-byte VR code to string
    char vr[3] = {static_cast<char>(vr_code & 0xFF),
                  static_cast<char>((vr_code >> 8) & 0xFF),
                  '\0'};
    return std::string(vr);
}

// ============================================================================
// Multipart Parser Implementation (STOW-RS)
// ============================================================================

auto multipart_parser::extract_boundary(std::string_view content_type)
    -> std::optional<std::string> {
    // Find boundary parameter in Content-Type header
    // Example: multipart/related; type="application/dicom"; boundary=----=_Part_123
    auto boundary_pos = content_type.find("boundary=");
    if (boundary_pos == std::string_view::npos) {
        return std::nullopt;
    }

    auto value_start = boundary_pos + 9; // length of "boundary="
    if (value_start >= content_type.size()) {
        return std::nullopt;
    }

    // Check if boundary is quoted
    if (content_type[value_start] == '"') {
        auto end_quote = content_type.find('"', value_start + 1);
        if (end_quote == std::string_view::npos) {
            return std::nullopt;
        }
        return std::string(content_type.substr(value_start + 1,
                                               end_quote - value_start - 1));
    }

    // Unquoted boundary - find end (semicolon, space, or end of string)
    auto end_pos = content_type.find_first_of("; \t", value_start);
    if (end_pos == std::string_view::npos) {
        end_pos = content_type.size();
    }
    return std::string(content_type.substr(value_start, end_pos - value_start));
}

auto multipart_parser::extract_type(std::string_view content_type)
    -> std::optional<std::string> {
    // Find type parameter in Content-Type header
    // Example: multipart/related; type="application/dicom"
    auto type_pos = content_type.find("type=");
    if (type_pos == std::string_view::npos) {
        return std::nullopt;
    }

    auto value_start = type_pos + 5; // length of "type="
    if (value_start >= content_type.size()) {
        return std::nullopt;
    }

    // Check if type is quoted
    if (content_type[value_start] == '"') {
        auto end_quote = content_type.find('"', value_start + 1);
        if (end_quote == std::string_view::npos) {
            return std::nullopt;
        }
        return std::string(content_type.substr(value_start + 1,
                                               end_quote - value_start - 1));
    }

    // Unquoted type
    auto end_pos = content_type.find_first_of("; \t", value_start);
    if (end_pos == std::string_view::npos) {
        end_pos = content_type.size();
    }
    return std::string(content_type.substr(value_start, end_pos - value_start));
}

auto multipart_parser::parse_part_headers(std::string_view header_section)
    -> std::vector<std::pair<std::string, std::string>> {
    std::vector<std::pair<std::string, std::string>> headers;

    size_t pos = 0;
    while (pos < header_section.size()) {
        // Find end of line
        auto line_end = header_section.find("\r\n", pos);
        if (line_end == std::string_view::npos) {
            line_end = header_section.size();
        }

        auto line = header_section.substr(pos, line_end - pos);
        if (line.empty()) {
            break;
        }

        // Parse header: name: value
        auto colon_pos = line.find(':');
        if (colon_pos != std::string_view::npos) {
            auto name = trim(line.substr(0, colon_pos));
            auto value = trim(line.substr(colon_pos + 1));

            // Convert header name to lowercase for case-insensitive matching
            std::string name_lower;
            name_lower.reserve(name.size());
            for (char c : name) {
                name_lower += static_cast<char>(std::tolower(
                    static_cast<unsigned char>(c)));
            }

            headers.emplace_back(std::move(name_lower), std::string(value));
        }

        pos = line_end + 2; // Skip \r\n
    }

    return headers;
}

auto multipart_parser::parse(std::string_view content_type,
                             std::string_view body) -> parse_result {
    parse_result result;

    // Extract boundary from Content-Type
    auto boundary_opt = extract_boundary(content_type);
    if (!boundary_opt || boundary_opt->empty()) {
        result.error = parse_error{
            "INVALID_BOUNDARY",
            "Missing or invalid boundary in Content-Type header"};
        return result;
    }

    // RFC 2046 5.1.1: a delimiter is a whole line. Pixel data may hold the boundary's bytes
    // anywhere else, and a part is only over when the next delimiter line is found.
    const std::string delimiter = "--" + *boundary_opt;
    const auto delimiter_at = [&](std::size_t from) -> std::size_t {
        for (auto at = body.find(delimiter, from); at != std::string_view::npos; at = body.find(delimiter, at + 1)) {
            const bool starts_line = at == 0 || (at >= 2 && body.substr(at - 2, 2) == "\r\n");
            const auto after = at + delimiter.size();
            const auto rest = body.substr(after);
            const auto padding_end = rest.find_first_not_of(" \t");
            const auto tail = padding_end == std::string_view::npos ? std::string_view{} : rest.substr(padding_end);
            const bool ends_line = rest.starts_with("--") || tail.starts_with("\r\n");
            if (starts_line && ends_line) {
                return at;
            }
        }
        return std::string_view::npos;
    };

    auto at = delimiter_at(0);
    if (at == std::string_view::npos) {
        result.error = parse_error{
            "NO_PARTS",
            "No parts found in multipart body"};
        return result;
    }

    for (;;) {
        const auto after = at + delimiter.size();
        if (body.substr(after).starts_with("--")) {
            break;
        }
        const auto line_end = body.find("\r\n", after);
        const auto next = delimiter_at(line_end + 2);
        if (next == std::string_view::npos) {
            result.parts.clear();
            result.error = parse_error{
                "TRUNCATED",
                "The multipart body ends without its closing delimiter"};
            return result;
        }

        // The CRLF before the delimiter belongs to the delimiter
        auto section = body.substr(line_end + 2, next - (line_end + 2));
        if (section.size() >= 2 && section.substr(section.size() - 2) == "\r\n") {
            section = section.substr(0, section.size() - 2);
        } else if (next == line_end + 2) {
            section = {};
        }

        std::string_view header_section;
        std::string_view body_section;
        if (section.starts_with("\r\n")) {
            body_section = section.substr(2);
        } else {
            const auto header_end = section.find("\r\n\r\n");
            if (header_end == std::string_view::npos) {
                result.parts.clear();
                result.error = parse_error{
                    "MALFORMED_PART",
                    "A part has no blank line between its headers and its body"};
                return result;
            }
            header_section = section.substr(0, header_end);
            body_section = section.substr(header_end + 4);
        }

        multipart_part part;
        for (const auto& [name, value] : parse_part_headers(header_section)) {
            if (name == "content-type") {
                part.content_type = value;
            } else if (name == "content-location") {
                part.content_location = value;
            } else if (name == "content-id") {
                part.content_id = value;
            }
        }

        // Default content type if not specified
        if (part.content_type.empty()) {
            part.content_type = std::string(media_type::dicom);
        }

        part.data.assign(
            reinterpret_cast<const uint8_t*>(body_section.data()),
            reinterpret_cast<const uint8_t*>(body_section.data() +
                                             body_section.size()));
        result.parts.push_back(std::move(part));
        at = next;
    }

    if (result.parts.empty()) {
        result.error = parse_error{
            "NO_VALID_PARTS",
            "No valid parts found in multipart body"};
    }

    return result;
}

// ============================================================================
// STOW-RS Validation
// ============================================================================

auto validate_instance(
    const core::dicom_dataset& dataset,
    std::optional<std::string_view> target_study_uid) -> validation_result {

    // Check required DICOM tags
    auto sop_class = dataset.get(core::tags::sop_class_uid);
    if (!sop_class || sop_class->as_string().unwrap_or("").empty()) {
        return validation_result::error(
            "MISSING_SOP_CLASS",
            "SOP Class UID (0008,0016) is required");
    }

    auto sop_instance = dataset.get(core::tags::sop_instance_uid);
    if (!sop_instance || sop_instance->as_string().unwrap_or("").empty()) {
        return validation_result::error(
            "MISSING_SOP_INSTANCE",
            "SOP Instance UID (0008,0018) is required");
    }

    auto study_uid = dataset.get(core::tags::study_instance_uid);
    if (!study_uid || study_uid->as_string().unwrap_or("").empty()) {
        return validation_result::error(
            "MISSING_STUDY_UID",
            "Study Instance UID (0020,000D) is required");
    }

    auto series_uid = dataset.get(core::tags::series_instance_uid);
    if (!series_uid || series_uid->as_string().unwrap_or("").empty()) {
        return validation_result::error(
            "MISSING_SERIES_UID",
            "Series Instance UID (0020,000E) is required");
    }

    // These values name files and appear in URLs: they are only ever digits and dots (PS3.5 9.1).
    for (const auto* uid : {sop_class, sop_instance, study_uid, series_uid}) {
        if (!storage::is_valid_uid(uid->as_string().unwrap_or(""))) {
            return validation_result::error(
                "INVALID_UID",
                "Not a valid UID: " + uid->as_string().unwrap_or(""));
        }
    }

    // Validate study UID matches target if specified
    if (target_study_uid.has_value()) {
        auto instance_study_uid = study_uid->as_string().unwrap_or("");
        if (instance_study_uid != *target_study_uid) {
            return validation_result::error(
                "STUDY_UID_MISMATCH",
                "Instance Study UID does not match target study");
        }
    }

    return validation_result::ok();
}

// ============================================================================
// STOW-RS Response Building
// ============================================================================

namespace {

/// PS3.18 I.2.2: what each kind of refusal is called in Failure Reason (0008,1197).
[[nodiscard]] uint16_t failure_reason_of(const store_instance_result& inst) {
    constexpr uint16_t processing_failure = 0x0110;
    constexpr uint16_t duplicate_sop_instance = 0x0111;
    constexpr uint16_t out_of_resources = 0xA700;
    constexpr uint16_t does_not_match_sop_class = 0xA900;
    constexpr uint16_t cannot_understand = 0xC000;
    if (!inst.error_code) {
        return processing_failure;
    }
    const std::string_view code = *inst.error_code;
    if (code == "DUPLICATE") {
        return duplicate_sop_instance;
    }
    if (code == "STORAGE_ERROR") {
        return out_of_resources;
    }
    if (code == "INVALID_DATA" || code == "UNSUPPORTED_PART") {
        return cannot_understand;
    }
    if (code.starts_with("MISSING_") || code == "INVALID_UID" || code == "STUDY_UID_MISMATCH") {
        return does_not_match_sop_class;
    }
    return processing_failure;
}

void write_uid_json(std::ostringstream& oss, std::string_view tag, const std::string& uid) {
    oss << "\"" << tag << "\":{\"vr\":\"UI\",\"Value\":[\"" << json_escape(uid) << "\"]},";
}

/// The attributes of the Store Instances Response Module (PS3.18 Annex I), as a dataset.
[[nodiscard]] core::dicom_dataset store_response_dataset(const store_response& response, std::string_view base_url) {
    const auto referenced = [](const store_instance_result& inst) {
        core::dicom_dataset item;
        if (!inst.sop_class_uid.empty()) {
            item.set_string(core::tags::referenced_sop_class_uid, encoding::vr_type::UI, inst.sop_class_uid);
        }
        if (!inst.sop_instance_uid.empty()) {
            item.set_string(core::tags::referenced_sop_instance_uid, encoding::vr_type::UI, inst.sop_instance_uid);
        }
        return item;
    };
    core::dicom_dataset dataset;
    if (!response.referenced_instances.empty()) {
        core::dicom_element sequence(core::dicom_tag{0x0008, 0x1199}, encoding::vr_type::SQ);
        for (const auto& inst : response.referenced_instances) {
            auto item = referenced(inst);
            item.set_string(core::dicom_tag{0x0008, 0x1190}, encoding::vr_type::UR, std::string(base_url)+inst.retrieve_url);
            sequence.sequence_items().push_back(std::move(item));
        }
        dataset.insert(std::move(sequence));
    }
    if (!response.failed_instances.empty()) {
        core::dicom_element sequence(core::dicom_tag{0x0008, 0x1198}, encoding::vr_type::SQ);
        for (const auto& inst : response.failed_instances) {
            auto item = referenced(inst);
            item.set_numeric<uint16_t>(core::dicom_tag{0x0008, 0x1197}, encoding::vr_type::US, failure_reason_of(inst));
            sequence.sequence_items().push_back(std::move(item));
        }
        dataset.insert(std::move(sequence));
    }
    return dataset;
}

}  // namespace

auto build_store_response_json(
    const store_response& response,
    std::string_view base_url) -> std::string {
    std::ostringstream oss;
    oss << "{";

    // Referenced SOP Sequence (0008,1199) - successfully stored instances
    if (!response.referenced_instances.empty()) {
        oss << "\"00081199\":{\"vr\":\"SQ\",\"Value\":[";
        bool first = true;
        for (const auto& inst : response.referenced_instances) {
            if (!first) oss << ",";
            first = false;

            oss << "{";
            write_uid_json(oss, "00081150", inst.sop_class_uid);
            write_uid_json(oss, "00081155", inst.sop_instance_uid);
            // Retrieve URL (0008,1190)
            oss << "\"00081190\":{\"vr\":\"UR\",\"Value\":[\""
                << json_escape(std::string(base_url) + inst.retrieve_url) << "\"]}";
            oss << "}";
        }
        oss << "]}";
    }

    // Failed SOP Sequence (0008,1198) - failed instances
    if (!response.failed_instances.empty()) {
        if (!response.referenced_instances.empty()) {
            oss << ",";
        }
        oss << "\"00081198\":{\"vr\":\"SQ\",\"Value\":[";
        bool first = true;
        for (const auto& inst : response.failed_instances) {
            if (!first) oss << ",";
            first = false;

            oss << "{";
            // An object that could not be read has no UIDs to name.
            if (!inst.sop_class_uid.empty()) {
                write_uid_json(oss, "00081150", inst.sop_class_uid);
            }
            if (!inst.sop_instance_uid.empty()) {
                write_uid_json(oss, "00081155", inst.sop_instance_uid);
            }
            // Failure Reason (0008,1197)
            oss << "\"00081197\":{\"vr\":\"US\",\"Value\":["
                << failure_reason_of(inst) << "]}";
            oss << "}";
        }
        oss << "]}";
    }

    oss << "}";
    return oss.str();
}

auto build_store_response_xml(
    const store_response& response,
    std::string_view base_url) -> std::string {
    return dataset_to_native_xml(store_response_dataset(response, base_url));
}

namespace {

constexpr std::string_view base64_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

[[nodiscard]] std::string base64_encode(std::span<const uint8_t> data) {
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < data.size(); i += 3) {
        const uint32_t a = data[i];
        const uint32_t b = i + 1 < data.size() ? data[i + 1] : 0;
        const uint32_t c = i + 2 < data.size() ? data[i + 2] : 0;
        const uint32_t triple = (a << 16) | (b << 8) | c;
        out += base64_alphabet[(triple >> 18) & 63];
        out += base64_alphabet[(triple >> 12) & 63];
        out += i + 1 < data.size() ? base64_alphabet[(triple >> 6) & 63] : '=';
        out += i + 2 < data.size() ? base64_alphabet[triple & 63] : '=';
    }
    return out;
}

[[nodiscard]] std::string tag_text(uint32_t tag) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << tag;
    return oss.str();
}

/// VRs whose values are held in binary form and go out as base64 or a Bulk Data URI (PS3.18 Table F.2.3-1).
[[nodiscard]] bool holds_binary_values(encoding::vr_type vr) {
    using vr_type = encoding::vr_type;
    return vr == vr_type::OB || vr == vr_type::OD || vr == vr_type::OF || vr == vr_type::OL || vr == vr_type::OV ||
           vr == vr_type::OW || vr == vr_type::UN;
}

/// LT, ST, UT and UR hold one value, and a backslash in them is just a character (PS3.5 6.2).
[[nodiscard]] bool is_single_valued_text(encoding::vr_type vr) {
    using vr_type = encoding::vr_type;
    return vr == vr_type::LT || vr == vr_type::ST || vr == vr_type::UT || vr == vr_type::UR;
}

[[nodiscard]] std::vector<std::string> values_of(const core::dicom_element& element) {
    auto text = element.as_string();
    if (text.is_err() || text.value().empty()) {
        return {};
    }
    if (is_single_valued_text(element.vr())) {
        return {text.value()};
    }
    return split(text.value(), '\\');
}

/// A DS value as a JSON number: "+1.50", "1." and ".5" are DICOM but not JSON. Empty when it is neither.
[[nodiscard]] std::string json_number(std::string_view text) {
    const auto value = trim(text);
    if (value.empty()) {
        return {};
    }
    char* end = nullptr;
    const double parsed = std::strtod(value.c_str(), &end);
    if (end == value.c_str() || *end != '\0' || !std::isfinite(parsed)) {
        return {};
    }
    std::array<char, 40> digits{};
    const auto written = std::to_chars(digits.data(), digits.data() + digits.size(), parsed);
    return std::string(digits.data(), written.ptr);
}

template <typename T>
void write_numbers(std::ostringstream& oss, const core::dicom_element& element) {
    auto values = element.as_numeric_list<T>();
    if (values.is_err() || values.value().empty()) {
        return;
    }
    oss << ",\"Value\":[";
    bool first = true;
    for (const auto value : values.value()) {
        if (!first) oss << ",";
        first = false;
        if constexpr (std::is_floating_point_v<T>) {
            if (std::isfinite(value)) {
                std::array<char, 40> digits{};
                const auto written = std::to_chars(digits.data(), digits.data() + digits.size(), value);
                oss << std::string_view(digits.data(), static_cast<std::size_t>(written.ptr - digits.data()));
            } else {
                // JSON has no number for these (PS3.18 F.2.3).
                oss << (std::isnan(value) ? "\"NaN\"" : value > 0 ? "\"Infinity\"" : "\"-Infinity\"");
            }
        } else if constexpr (sizeof(T) == 8) {
            // Beyond 2^53 a JSON reader cannot keep the value as a number (PS3.18 Table F.2.3-1 note).
            const bool exact = value <= static_cast<T>(9007199254740991LL) &&
                               (std::is_unsigned_v<T> || value >= static_cast<T>(-9007199254740991LL));
            if (exact) {
                oss << value;
            } else {
                oss << "\"" << value << "\"";
            }
        } else {
            oss << +value;
        }
    }
    oss << "]";
}

[[nodiscard]] std::vector<std::string> attribute_tags_of(const core::dicom_element& element) {
    std::vector<std::string> tags;
    const auto data = element.raw_data();
    for (std::size_t i = 0; i + 4 <= data.size(); i += 4) {
        const uint32_t group = static_cast<uint32_t>(data[i]) | (static_cast<uint32_t>(data[i + 1]) << 8);
        const uint32_t member = static_cast<uint32_t>(data[i + 2]) | (static_cast<uint32_t>(data[i + 3]) << 8);
        tags.push_back(tag_text((group << 16) | member));
    }
    return tags;
}

/// One PN value's component groups: Alphabetic, Ideographic and Phonetic, by the = between them.
[[nodiscard]] std::array<std::string, 3> name_groups(const std::string& value) {
    std::array<std::string, 3> groups;
    std::size_t index = 0;
    for (const auto& group : split(value, '=')) {
        if (index < groups.size()) {
            groups[index++] = group;
        }
    }
    return groups;
}

void write_element_json(std::ostringstream& oss, const core::dicom_element& element, uint32_t tag, bool include_bulk_data,
                        std::string_view bulk_data_uri_prefix);

void write_dataset_json(std::ostringstream& oss, const core::dicom_dataset& dataset, bool include_bulk_data,
                        std::string_view bulk_data_uri_prefix) {
    oss << "{";
    bool first = true;
    for (const auto& [tag_key, element] : dataset) {
        // Group lengths are not attributes (PS3.5 7.2), and file meta belongs to a file.
        if (tag_key.element() == 0x0000 || tag_key.group() == 0x0002) {
            continue;
        }
        if (!first) {
            oss << ",";
        }
        first = false;
        write_element_json(oss, element, tag_key.combined(), include_bulk_data, bulk_data_uri_prefix);
    }
    oss << "}";
}

void write_element_json(std::ostringstream& oss, const core::dicom_element& element, uint32_t tag, bool include_bulk_data,
                        std::string_view bulk_data_uri_prefix) {
    using vr_type = encoding::vr_type;
    const auto vr = element.vr();
    oss << "\"" << tag_text(tag) << "\":{\"vr\":\"" << encoding::to_string(vr) << "\"";

    if (element.is_sequence()) {
        if (!element.sequence_items().empty()) {
            oss << ",\"Value\":[";
            bool first = true;
            for (const auto& item : element.sequence_items()) {
                if (!first) oss << ",";
                first = false;
                // The Bulk Data route names top level attributes, so anything nested goes inline or is lost.
                write_dataset_json(oss, item, true, {});
            }
            oss << "]";
        }
    } else if (is_bulk_data_tag(tag) && !include_bulk_data) {
        if (!bulk_data_uri_prefix.empty()) {
            oss << ",\"BulkDataURI\":\"" << json_escape(std::string(bulk_data_uri_prefix)) << tag_text(tag) << "\"";
        }
    } else if (holds_binary_values(vr)) {
        if (!element.raw_data().empty()) {
            oss << ",\"InlineBinary\":\"" << base64_encode(element.raw_data()) << "\"";
        }
    } else {
        switch (vr) {
            case vr_type::US: write_numbers<uint16_t>(oss, element); break;
            case vr_type::SS: write_numbers<int16_t>(oss, element); break;
            case vr_type::UL: write_numbers<uint32_t>(oss, element); break;
            case vr_type::SL: write_numbers<int32_t>(oss, element); break;
            case vr_type::UV: write_numbers<uint64_t>(oss, element); break;
            case vr_type::SV: write_numbers<int64_t>(oss, element); break;
            case vr_type::FL: write_numbers<float>(oss, element); break;
            case vr_type::FD: write_numbers<double>(oss, element); break;
            default: {
                const auto values = vr == vr_type::AT ? attribute_tags_of(element) : values_of(element);
                if (values.empty()) {
                    break;
                }
                oss << ",\"Value\":[";
                bool first = true;
                for (const auto& value : values) {
                    if (!first) oss << ",";
                    first = false;
                    if (vr == vr_type::PN) {
                        const auto groups = name_groups(value);
                        oss << "{";
                        bool first_group = true;
                        for (std::size_t g = 0; g < groups.size(); ++g) {
                            if (groups[g].empty()) {
                                continue;
                            }
                            static constexpr std::array<std::string_view, 3> names{"Alphabetic", "Ideographic", "Phonetic"};
                            if (!first_group) oss << ",";
                            first_group = false;
                            oss << "\"" << names[g] << "\":\"" << json_escape(groups[g]) << "\"";
                        }
                        oss << "}";
                    } else if (vr == vr_type::DS || vr == vr_type::IS) {
                        // A number that is not one still has to survive as text (PS3.18 Table F.2.3-1 note).
                        const auto number = json_number(value);
                        if (number.empty()) {
                            oss << (trim(value).empty() ? std::string("null") : "\"" + json_escape(value) + "\"");
                        } else {
                            oss << number;
                        }
                    } else {
                        oss << "\"" << json_escape(value) << "\"";
                    }
                }
                oss << "]";
            }
        }
    }
    oss << "}";
}

[[nodiscard]] std::string xml_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c;
        }
    }
    return out;
}

void write_dataset_xml(std::ostringstream& oss, const core::dicom_dataset& dataset, bool include_bulk_data,
                       std::string_view bulk_data_uri_prefix);

void write_name_xml(std::ostringstream& oss, const std::string& value) {
    static constexpr std::array<std::string_view, 3> groups{"Alphabetic", "Ideographic", "Phonetic"};
    static constexpr std::array<std::string_view, 5> parts{"FamilyName", "GivenName", "MiddleName", "NamePrefix", "NameSuffix"};
    const auto values = name_groups(value);
    for (std::size_t g = 0; g < values.size(); ++g) {
        if (values[g].empty()) {
            continue;
        }
        oss << "<" << groups[g] << ">";
        std::size_t index = 0;
        for (const auto& component : split(values[g], '^')) {
            if (index < parts.size() && !component.empty()) {
                oss << "<" << parts[index] << ">" << xml_escape(component) << "</" << parts[index] << ">";
            }
            ++index;
        }
        oss << "</" << groups[g] << ">";
    }
}

template <typename T>
void write_numbers_xml(std::ostringstream& oss, const core::dicom_element& element) {
    auto values = element.as_numeric_list<T>();
    if (values.is_err()) {
        return;
    }
    std::size_t number = 0;
    for (const auto value : values.value()) {
        oss << "<Value number=\"" << ++number << "\">" << +value << "</Value>";
    }
}

void write_element_xml(std::ostringstream& oss, const core::dicom_element& element, uint32_t tag, bool include_bulk_data,
                       std::string_view bulk_data_uri_prefix) {
    using vr_type = encoding::vr_type;
    const auto vr = element.vr();
    oss << "<DicomAttribute tag=\"" << tag_text(tag) << "\" vr=\"" << encoding::to_string(vr) << "\"";
    if (const auto info = core::dicom_dictionary::instance().find(core::dicom_tag{tag})) {
        oss << " keyword=\"" << info->keyword << "\"";
    }
    oss << ">";
    if (element.is_sequence()) {
        std::size_t number = 0;
        for (const auto& item : element.sequence_items()) {
            oss << "<Item number=\"" << ++number << "\">";
            write_dataset_xml(oss, item, true, {});
            oss << "</Item>";
        }
    } else if (is_bulk_data_tag(tag) && !include_bulk_data) {
        if (!bulk_data_uri_prefix.empty()) {
            oss << "<BulkData uri=\"" << xml_escape(bulk_data_uri_prefix) << tag_text(tag) << "\"/>";
        }
    } else if (holds_binary_values(vr)) {
        if (!element.raw_data().empty()) {
            oss << "<InlineBinary>" << base64_encode(element.raw_data()) << "</InlineBinary>";
        }
    } else {
        switch (vr) {
            case vr_type::US: write_numbers_xml<uint16_t>(oss, element); break;
            case vr_type::SS: write_numbers_xml<int16_t>(oss, element); break;
            case vr_type::UL: write_numbers_xml<uint32_t>(oss, element); break;
            case vr_type::SL: write_numbers_xml<int32_t>(oss, element); break;
            case vr_type::UV: write_numbers_xml<uint64_t>(oss, element); break;
            case vr_type::SV: write_numbers_xml<int64_t>(oss, element); break;
            case vr_type::FL: write_numbers_xml<float>(oss, element); break;
            case vr_type::FD: write_numbers_xml<double>(oss, element); break;
            default: {
                const auto values = vr == vr_type::AT ? attribute_tags_of(element) : values_of(element);
                std::size_t number = 0;
                for (const auto& value : values) {
                    if (vr == vr_type::PN) {
                        oss << "<PersonName number=\"" << ++number << "\">";
                        write_name_xml(oss, value);
                        oss << "</PersonName>";
                    } else {
                        oss << "<Value number=\"" << ++number << "\">" << xml_escape(value) << "</Value>";
                    }
                }
            }
        }
    }
    oss << "</DicomAttribute>";
}

void write_dataset_xml(std::ostringstream& oss, const core::dicom_dataset& dataset, bool include_bulk_data,
                       std::string_view bulk_data_uri_prefix) {
    for (const auto& [tag_key, element] : dataset) {
        if (tag_key.element() == 0x0000 || tag_key.group() == 0x0002) {
            continue;
        }
        write_element_xml(oss, element, tag_key.combined(), include_bulk_data, bulk_data_uri_prefix);
    }
}

}  // namespace

namespace {

/// Bytes an object's character set does not explain, as U+FFFD: one bad name must not make a whole response unreadable.
[[nodiscard]] std::string valid_utf8(std::string text) {
    std::string repaired;
    bool any_bad = false;
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i]);
        const std::size_t length = lead < 0x80 ? 1 : (lead >= 0xC2 && lead <= 0xDF) ? 2 : (lead & 0xF0) == 0xE0 ? 3 : (lead >= 0xF0 && lead <= 0xF4) ? 4 : 0;
        bool good = length != 0 && i + length <= text.size();
        for (std::size_t j = 1; good && j < length; ++j) {
            good = (static_cast<unsigned char>(text[i + j]) & 0xC0) == 0x80;
        }
        if (good && length >= 3) {
            const auto second = static_cast<unsigned char>(text[i + 1]);
            good = !(lead == 0xE0 && second < 0xA0) && !(lead == 0xED && second > 0x9F) && !(lead == 0xF0 && second < 0x90) && !(lead == 0xF4 && second > 0x8F);
        }
        if (good) {
            repaired.append(text, i, length);
            i += length;
        } else {
            any_bad = true;
            repaired += "\xEF\xBF\xBD";
            ++i;
        }
    }
    return any_bad ? repaired : text;
}

}  // namespace

auto dataset_to_dicom_json(
    const core::dicom_dataset& dataset,
    bool include_bulk_data,
    std::string_view bulk_data_uri_prefix) -> std::string {
    std::ostringstream oss;
    write_dataset_json(oss, core::utf8_text_copy(dataset), include_bulk_data, bulk_data_uri_prefix);
    return valid_utf8(oss.str());
}

auto dataset_to_native_xml(
    const core::dicom_dataset& dataset,
    bool include_bulk_data,
    std::string_view bulk_data_uri_prefix) -> std::string {
    std::ostringstream oss;
    oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<NativeDicomModel xmlns=\"http://dicom.nema.org/PS3.19/models/NativeDICOM\">";
    write_dataset_xml(oss, core::utf8_text_copy(dataset), include_bulk_data, bulk_data_uri_prefix);
    oss << "</NativeDicomModel>\n";
    return valid_utf8(oss.str());
}

// ============================================================================
// QIDO-RS Response Building
// ============================================================================

auto study_record_to_dicom_json(
    const storage::study_record& record,
    std::string_view patient_id,
    std::string_view patient_name) -> std::string {
    std::ostringstream oss;
    oss << "{";

    // Study Instance UID (0020,000D)
    oss << "\"0020000D\":{\"vr\":\"UI\",\"Value\":[\""
        << json_escape(record.study_uid) << "\"]}";

    // Study Date (0008,0020)
    if (!record.study_date.empty()) {
        oss << ",\"00080020\":{\"vr\":\"DA\",\"Value\":[\""
            << json_escape(record.study_date) << "\"]}";
    }

    // Study Time (0008,0030)
    if (!record.study_time.empty()) {
        oss << ",\"00080030\":{\"vr\":\"TM\",\"Value\":[\""
            << json_escape(record.study_time) << "\"]}";
    }

    // Accession Number (0008,0050)
    if (!record.accession_number.empty()) {
        oss << ",\"00080050\":{\"vr\":\"SH\",\"Value\":[\""
            << json_escape(record.accession_number) << "\"]}";
    }

    // Modalities in Study (0008,0061)
    if (!record.modalities_in_study.empty()) {
        oss << ",\"00080061\":{\"vr\":\"CS\",\"Value\":[";
        auto modalities = split(record.modalities_in_study, '\\');
        bool first = true;
        for (const auto& mod : modalities) {
            if (!first) oss << ",";
            first = false;
            oss << "\"" << json_escape(mod) << "\"";
        }
        oss << "]}";
    }

    // Referring Physician's Name (0008,0090)
    if (!record.referring_physician.empty()) {
        oss << ",\"00080090\":{\"vr\":\"PN\",\"Value\":[{\"Alphabetic\":\""
            << json_escape(record.referring_physician) << "\"}]}";
    }

    // Patient's Name (0010,0010)
    if (!patient_name.empty()) {
        oss << ",\"00100010\":{\"vr\":\"PN\",\"Value\":[{\"Alphabetic\":\""
            << json_escape(std::string(patient_name)) << "\"}]}";
    }

    // Patient ID (0010,0020)
    if (!patient_id.empty()) {
        oss << ",\"00100020\":{\"vr\":\"LO\",\"Value\":[\""
            << json_escape(std::string(patient_id)) << "\"]}";
    }

    // Study ID (0020,0010)
    if (!record.study_id.empty()) {
        oss << ",\"00200010\":{\"vr\":\"SH\",\"Value\":[\""
            << json_escape(record.study_id) << "\"]}";
    }

    // Study Description (0008,1030)
    if (!record.study_description.empty()) {
        oss << ",\"00081030\":{\"vr\":\"LO\",\"Value\":[\""
            << json_escape(record.study_description) << "\"]}";
    }

    // Number of Study Related Series (0020,1206)
    oss << ",\"00201206\":{\"vr\":\"IS\",\"Value\":[" << record.num_series << "]}";

    // Number of Study Related Instances (0020,1208)
    oss << ",\"00201208\":{\"vr\":\"IS\",\"Value\":[" << record.num_instances << "]}";

    oss << "}";
    return oss.str();
}

auto series_record_to_dicom_json(
    const storage::series_record& record,
    std::string_view study_uid) -> std::string {
    std::ostringstream oss;
    oss << "{";

    // Series Instance UID (0020,000E)
    oss << "\"0020000E\":{\"vr\":\"UI\",\"Value\":[\""
        << json_escape(record.series_uid) << "\"]}";

    // Study Instance UID (0020,000D)
    if (!study_uid.empty()) {
        oss << ",\"0020000D\":{\"vr\":\"UI\",\"Value\":[\""
            << json_escape(std::string(study_uid)) << "\"]}";
    }

    // Modality (0008,0060)
    if (!record.modality.empty()) {
        oss << ",\"00080060\":{\"vr\":\"CS\",\"Value\":[\""
            << json_escape(record.modality) << "\"]}";
    }

    // Series Number (0020,0011)
    if (record.series_number.has_value()) {
        oss << ",\"00200011\":{\"vr\":\"IS\",\"Value\":["
            << *record.series_number << "]}";
    }

    // Series Description (0008,103E)
    if (!record.series_description.empty()) {
        oss << ",\"0008103E\":{\"vr\":\"LO\",\"Value\":[\""
            << json_escape(record.series_description) << "\"]}";
    }

    // Body Part Examined (0018,0015)
    if (!record.body_part_examined.empty()) {
        oss << ",\"00180015\":{\"vr\":\"CS\",\"Value\":[\""
            << json_escape(record.body_part_examined) << "\"]}";
    }

    // Number of Series Related Instances (0020,1209)
    oss << ",\"00201209\":{\"vr\":\"IS\",\"Value\":[" << record.num_instances << "]}";

    oss << "}";
    return oss.str();
}

auto instance_record_to_dicom_json(
    const storage::instance_record& record,
    std::string_view series_uid,
    std::string_view study_uid) -> std::string {
    std::ostringstream oss;
    oss << "{";

    // SOP Class UID (0008,0016)
    if (!record.sop_class_uid.empty()) {
        oss << "\"00080016\":{\"vr\":\"UI\",\"Value\":[\""
            << json_escape(record.sop_class_uid) << "\"]}";
    }

    // SOP Instance UID (0008,0018)
    oss << ",\"00080018\":{\"vr\":\"UI\",\"Value\":[\""
        << json_escape(record.sop_uid) << "\"]}";

    // Study Instance UID (0020,000D)
    if (!study_uid.empty()) {
        oss << ",\"0020000D\":{\"vr\":\"UI\",\"Value\":[\""
            << json_escape(std::string(study_uid)) << "\"]}";
    }

    // Series Instance UID (0020,000E)
    if (!series_uid.empty()) {
        oss << ",\"0020000E\":{\"vr\":\"UI\",\"Value\":[\""
            << json_escape(std::string(series_uid)) << "\"]}";
    }

    // Instance Number (0020,0013)
    if (record.instance_number.has_value()) {
        oss << ",\"00200013\":{\"vr\":\"IS\",\"Value\":["
            << *record.instance_number << "]}";
    }

    // Rows (0028,0010)
    if (record.rows.has_value()) {
        oss << ",\"00280010\":{\"vr\":\"US\",\"Value\":["
            << *record.rows << "]}";
    }

    // Columns (0028,0011)
    if (record.columns.has_value()) {
        oss << ",\"00280011\":{\"vr\":\"US\",\"Value\":["
            << *record.columns << "]}";
    }

    // Number of Frames (0028,0008)
    if (record.number_of_frames.has_value()) {
        oss << ",\"00280008\":{\"vr\":\"IS\",\"Value\":["
            << *record.number_of_frames << "]}";
    }

    oss << "}";
    return oss.str();
}

// ============================================================================
// QIDO-RS Query Parameter Parsing
// ============================================================================

namespace {

/**
 * @brief URL decode a string
 */
std::string url_decode(std::string_view str) {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size() && std::isxdigit(static_cast<unsigned char>(str[i + 1])) != 0 &&
            std::isxdigit(static_cast<unsigned char>(str[i + 2])) != 0) {
            int value = 0;
            (void)std::from_chars(str.data() + i + 1, str.data() + i + 3, value, 16);
            result += static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

/**
 * @brief Parse query string into key-value pairs
 */
std::vector<std::pair<std::string, std::string>> parse_query_string(
    const std::string& query_string) {
    std::vector<std::pair<std::string, std::string>> result;

    if (query_string.empty()) {
        return result;
    }

    // Callers pass a whole URL or only its query; what precedes '?' is not a parameter.
    std::string_view qs = query_string;
    if (const auto question = qs.find('?'); question != std::string_view::npos) {
        qs = qs.substr(question + 1);
    } else if (!qs.empty() && qs[0] == '/') {
        return result;
    }

    auto params = split(qs, '&');
    for (const auto& param : params) {
        auto eq_pos = param.find('=');
        if (eq_pos != std::string::npos) {
            auto key = url_decode(param.substr(0, eq_pos));
            auto value = url_decode(param.substr(eq_pos + 1));
            result.emplace_back(std::move(key), std::move(value));
        }
    }

    return result;
}

} // anonymous namespace

auto parse_study_query_params(const std::string& url_params) -> storage::study_query {
    storage::study_query query;

    auto params = parse_query_string(url_params);
    for (const auto& [key, value] : params) {
        if (key == "PatientID" || key == "00100020") {
            query.patient_id = value;
        } else if (key == "PatientName" || key == "00100010") {
            query.patient_name = value;
        } else if (key == "StudyInstanceUID" || key == "0020000D") {
            query.study_uid = value;
        } else if (key == "StudyID" || key == "00200010") {
            query.study_id = value;
        } else if (key == "StudyDate" || key == "00080020") {
            // Handle date range (YYYYMMDD-YYYYMMDD)
            auto dash_pos = value.find('-');
            if (dash_pos != std::string::npos && dash_pos > 0 &&
                dash_pos < value.size() - 1) {
                query.study_date_from = value.substr(0, dash_pos);
                query.study_date_to = value.substr(dash_pos + 1);
            } else if (dash_pos == 0) {
                // -YYYYMMDD (up to date)
                query.study_date_to = value.substr(1);
            } else if (dash_pos == value.size() - 1) {
                // YYYYMMDD- (from date)
                query.study_date_from = value.substr(0, dash_pos);
            } else {
                query.study_date = value;
            }
        } else if (key == "AccessionNumber" || key == "00080050") {
            query.accession_number = value;
        } else if (key == "ModalitiesInStudy" || key == "00080061") {
            query.modality = value;
        } else if (key == "ReferringPhysicianName" || key == "00080090") {
            query.referring_physician = value;
        } else if (key == "StudyDescription" || key == "00081030") {
            query.study_description = value;
        } else if (key == "limit") {
            try {
                query.limit = std::stoull(value);
            } catch (...) {}
        } else if (key == "offset") {
            try {
                query.offset = std::stoull(value);
            } catch (...) {}
        }
    }

    return query;
}

auto parse_series_query_params(const std::string& url_params) -> storage::series_query {
    storage::series_query query;

    auto params = parse_query_string(url_params);
    for (const auto& [key, value] : params) {
        if (key == "StudyInstanceUID" || key == "0020000D") {
            query.study_uid = value;
        } else if (key == "SeriesInstanceUID" || key == "0020000E") {
            query.series_uid = value;
        } else if (key == "Modality" || key == "00080060") {
            query.modality = value;
        } else if (key == "SeriesNumber" || key == "00200011") {
            try {
                query.series_number = std::stoi(value);
            } catch (...) {}
        } else if (key == "SeriesDescription" || key == "0008103E") {
            query.series_description = value;
        } else if (key == "BodyPartExamined" || key == "00180015") {
            query.body_part_examined = value;
        } else if (key == "limit") {
            try {
                query.limit = std::stoull(value);
            } catch (...) {}
        } else if (key == "offset") {
            try {
                query.offset = std::stoull(value);
            } catch (...) {}
        }
    }

    return query;
}

auto parse_instance_query_params(const std::string& url_params) -> storage::instance_query {
    storage::instance_query query;

    auto params = parse_query_string(url_params);
    for (const auto& [key, value] : params) {
        if (key == "SeriesInstanceUID" || key == "0020000E") {
            query.series_uid = value;
        } else if (key == "SOPInstanceUID" || key == "00080018") {
            query.sop_uid = value;
        } else if (key == "SOPClassUID" || key == "00080016") {
            query.sop_class_uid = value;
        } else if (key == "InstanceNumber" || key == "00200013") {
            try {
                query.instance_number = std::stoi(value);
            } catch (...) {}
        } else if (key == "limit") {
            try {
                query.limit = std::stoull(value);
            } catch (...) {}
        } else if (key == "offset") {
            try {
                query.offset = std::stoull(value);
            } catch (...) {}
        }
    }

    return query;
}

// ============================================================================
// QIDO-RS Parameters as PS3.18 8.3.4 Defines Them
// ============================================================================

namespace {

[[nodiscard]] std::optional<std::size_t> unsigned_value(std::string_view text) {
    std::size_t value = 0;
    const auto* end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, value);
    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != end) {
        return std::nullopt;
    }
    return value;
}

/// An Attribute by its Keyword or eight hexadecimal digits (8.3.4.1).
[[nodiscard]] std::optional<std::pair<core::dicom_tag, encoding::vr_type>> attribute_named(std::string_view name) {
    const auto& dictionary = core::dicom_dictionary::instance();
    if (const auto info = dictionary.find_by_keyword(name)) {
        return std::make_pair(info->tag, static_cast<encoding::vr_type>(info->vr));
    }
    if (name.size() == 8 && std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isxdigit(c) != 0; })) {
        uint32_t combined = 0;
        (void)std::from_chars(name.data(), name.data() + name.size(), combined, 16);
        const core::dicom_tag tag{combined};
        const auto info = dictionary.find(tag);
        return std::make_pair(tag, info ? static_cast<encoding::vr_type>(info->vr) : encoding::vr_type::UN);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<bool> boolean_value(std::string_view text) {
    if (text == "true") {
        return true;
    }
    if (text == "false") {
        return false;
    }
    return std::nullopt;
}

}  // namespace

auto parse_search_parameters(std::string_view url) -> search_parameters {
    search_parameters parsed;
    std::vector<uint32_t> seen;
    const auto refuse = [&parsed](std::string reason) {
        parsed.error = std::move(reason);
        return parsed;
    };
    for (const auto& [name, value] : parse_query_string(std::string(url))) {
        if (name == "limit" || name == "offset") {
            const auto number = unsigned_value(value);
            if (!number) {
                return refuse(name + " is not an unsigned integer: " + value);
            }
            (name == "limit" ? parsed.limit : parsed.offset) = *number;
        } else if (name == "fuzzymatching" || name == "emptyvaluematching" || name == "multiplevaluematching") {
            const auto flag = boolean_value(value);
            if (!flag) {
                return refuse(name + " is neither true nor false: " + value);
            }
            if (*flag) {
                parsed.unsupported_flags.push_back(name);
            }
        } else if (name == "includefield") {
            for (const auto& field : split(value, ',')) {
                if (field == "all") {
                    parsed.include_all = true;
                } else if (const auto attribute = attribute_named(field)) {
                    parsed.include_fields.push_back(attribute->first);
                } else {
                    return refuse("includefield names no attribute: " + field);
                }
            }
        } else if (name == "accept" || name == "charset") {
            continue;  // 8.3.3: they select a representation, not results
        } else if (name.find('.') != std::string::npos) {
            // A key inside a sequence is valid syntax; the index holds no sequences to match it against.
            parsed.keys_not_used.push_back(name);
        } else {
            const auto attribute = attribute_named(name);
            if (!attribute) {
                return refuse("Not a DICOM Keyword or Tag: " + name);
            }
            const auto combined = attribute->first.combined();
            if (std::find(seen.begin(), seen.end(), combined) != seen.end()) {
                return refuse("An attribute shall not be repeated (8.3.4.1): " + name);
            }
            seen.push_back(combined);
            auto text = value;
            // Only UIDs may be listed with commas (8.3.4.1); C-FIND lists them with backslashes.
            if (attribute->second == encoding::vr_type::UI) {
                std::replace(text.begin(), text.end(), ',', '\\');
            }
            parsed.keys.push_back({attribute->first, attribute->second, std::move(text), name});
        }
    }
    return parsed;
}

// ============================================================================
// Frame Retrieval
// ============================================================================

auto parse_frame_numbers(std::string_view frame_list) -> std::vector<uint32_t> {
    std::vector<uint32_t> frames;

    if (frame_list.empty()) {
        return frames;
    }

    // Split by comma
    auto parts = split(frame_list, ',');
    for (const auto& part : parts) {
        auto trimmed = trim(part);
        if (trimmed.empty()) {
            continue;
        }

        // Check for range (e.g., "1-5")
        auto dash_pos = trimmed.find('-');
        if (dash_pos != std::string::npos && dash_pos > 0 &&
            dash_pos < trimmed.size() - 1) {
            try {
                uint32_t start = std::stoul(trimmed.substr(0, dash_pos));
                uint32_t end = std::stoul(trimmed.substr(dash_pos + 1));
                if (start > 0 && end >= start) {
                    for (uint32_t i = start; i <= end; ++i) {
                        frames.push_back(i);
                    }
                }
            } catch (...) {
                // Invalid range, skip
            }
        } else {
            // Single frame number
            try {
                uint32_t frame = std::stoul(trimmed);
                if (frame > 0) {
                    frames.push_back(frame);
                }
            } catch (...) {
                // Invalid number, skip
            }
        }
    }

    // Remove duplicates while preserving order
    std::vector<uint32_t> unique_frames;
    for (auto f : frames) {
        if (std::find(unique_frames.begin(), unique_frames.end(), f) ==
            unique_frames.end()) {
            unique_frames.push_back(f);
        }
    }

    return unique_frames;
}

auto extract_frame(
    std::span<const uint8_t> pixel_data,
    uint32_t frame_number,
    size_t frame_size) -> std::vector<uint8_t> {

    if (frame_number == 0 || frame_size == 0) {
        return {};
    }

    // Frame numbers are 1-based
    size_t offset = (frame_number - 1) * frame_size;

    if (offset + frame_size > pixel_data.size()) {
        return {};  // Frame doesn't exist
    }

    return std::vector<uint8_t>(
        pixel_data.begin() + static_cast<ptrdiff_t>(offset),
        pixel_data.begin() + static_cast<ptrdiff_t>(offset + frame_size));
}

// ============================================================================
// Frame Lists
// ============================================================================

auto parse_frame_list(std::string_view frame_list) -> std::optional<std::vector<uint32_t>> {
    // Bounds what a range can ask for before anything knows how many frames there are.
    constexpr std::size_t most_frames = 1U << 20;
    std::vector<uint32_t> frames;
    const auto add = [&frames](uint32_t frame) {
        // PS3.18 10.1: "a comma-separated list of Frame numbers, in ascending order".
        if (frame == 0 || (!frames.empty() && frame <= frames.back()) || frames.size() >= most_frames) {
            return false;
        }
        frames.push_back(frame);
        return true;
    };
    const auto number = [](std::string_view text) -> std::optional<uint32_t> {
        uint32_t value = 0;
        const auto* end = text.data() + text.size();
        const auto parsed = std::from_chars(text.data(), end, value);
        if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != end) {
            return std::nullopt;
        }
        return value;
    };
    for (const auto& item : split(frame_list, ',')) {
        const auto dash = item.find('-');
        if (dash == std::string::npos) {
            const auto frame = number(item);
            if (!frame || !add(*frame)) {
                return std::nullopt;
            }
            continue;
        }
        const auto first = number(std::string_view(item).substr(0, dash));
        const auto last = number(std::string_view(item).substr(dash + 1));
        if (!first || !last || *last < *first || *last - *first >= most_frames) {
            return std::nullopt;
        }
        for (uint32_t frame = *first; ; ++frame) {
            if (!add(frame)) {
                return std::nullopt;
            }
            if (frame == *last) {
                break;
            }
        }
    }
    return frames;
}

// ============================================================================
// Pixel Access
// ============================================================================

namespace {

namespace compression = encoding::compression;

constexpr core::dicom_tag number_of_frames_tag{0x0028, 0x0008};
constexpr core::dicom_tag planar_configuration_tag{0x0028, 0x0006};

/// What the image attributes say about the pixels of one frame.
struct frame_layout {
    uint16_t rows{0};
    uint16_t columns{0};
    uint16_t bits_allocated{8};
    uint16_t bits_stored{8};
    uint16_t high_bit{7};
    uint16_t samples{1};
    uint16_t planar{0};
    uint16_t pixel_representation{0};
    uint32_t frames{1};
    compression::photometric_interpretation photometric{compression::photometric_interpretation::monochrome2};

    [[nodiscard]] std::size_t frame_bytes() const {
        return static_cast<std::size_t>(rows) * columns * samples * (bits_allocated / 8);
    }
};

[[nodiscard]] std::optional<frame_layout> layout_of(const core::dicom_dataset& dataset) {
    if (!dataset.contains(core::tags::pixel_data)) {
        return std::nullopt;
    }
    const auto rows = dataset.get_numeric<uint16_t>(core::tags::rows);
    const auto columns = dataset.get_numeric<uint16_t>(core::tags::columns);
    if (!rows || !columns || *rows == 0 || *columns == 0) {
        return std::nullopt;
    }
    frame_layout layout;
    layout.rows = *rows;
    layout.columns = *columns;
    layout.bits_allocated = dataset.get_numeric<uint16_t>(core::tags::bits_allocated).value_or(8);
    layout.bits_stored = dataset.get_numeric<uint16_t>(core::tags::bits_stored).value_or(layout.bits_allocated);
    layout.high_bit = dataset.get_numeric<uint16_t>(core::tags::high_bit).value_or(static_cast<uint16_t>(layout.bits_stored - 1));
    layout.samples = dataset.get_numeric<uint16_t>(core::tags::samples_per_pixel).value_or(1);
    layout.planar = dataset.get_numeric<uint16_t>(planar_configuration_tag).value_or(0);
    layout.pixel_representation = dataset.get_numeric<uint16_t>(core::tags::pixel_representation).value_or(0);
    layout.photometric = compression::parse_photometric_interpretation(dataset.get_string(core::tags::photometric_interpretation));
    const auto frames = dataset.get_string(number_of_frames_tag);
    if (!frames.empty()) {
        const auto text = trim(frames);
        uint32_t count = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), count);
        if (parsed.ec != std::errc{} || count == 0) {
            return std::nullopt;
        }
        layout.frames = count;
    }
    return layout;
}

/// The frames asked for as they are held: native bytes, or one encoded frame without fragment delimiters.
[[nodiscard]] kcenon::pacs::Result<std::vector<std::vector<uint8_t>>> held_frames(const core::dicom_file& file, const frame_layout& layout,
                                                                                 const std::vector<uint32_t>& numbers) {
    using frame_list = std::vector<std::vector<uint8_t>>;
    const auto* pixel = file.dataset().get(core::tags::pixel_data);
    if (pixel == nullptr) {
        return kcenon::pacs::pacs_error<frame_list>(kcenon::pacs::error_codes::invalid_sequence, "The object has no Pixel Data");
    }
    frame_list selected;
    if (file.transfer_syntax().is_encapsulated()) {
        auto all = core::dicom_file::encapsulated_frames(pixel->raw_data(), layout.frames);
        if (all.is_err()) {
            return all.error();
        }
        for (const auto number : numbers) {
            selected.push_back(std::move(all.value()[number - 1]));
        }
        return selected;
    }
    if (layout.bits_allocated % 8 != 0) {
        return kcenon::pacs::pacs_error<frame_list>(kcenon::pacs::error_codes::invalid_sequence,
                                                    "Frames that are not byte aligned cannot be separated");
    }
    const auto data = pixel->raw_data();
    const auto size = layout.frame_bytes();
    for (const auto number : numbers) {
        const auto frame = extract_frame(data, number, size);
        if (frame.empty()) {
            return kcenon::pacs::pacs_error<frame_list>(kcenon::pacs::error_codes::data_size_mismatch,
                                                        "Pixel Data is too short for frame " + std::to_string(number));
        }
        selected.push_back(frame);
    }
    return selected;
}

[[nodiscard]] compression::image_params params_of(const frame_layout& layout) {
    compression::image_params params;
    params.width = layout.columns;
    params.height = layout.rows;
    params.bits_allocated = layout.bits_allocated;
    params.bits_stored = layout.bits_stored;
    params.high_bit = layout.high_bit;
    params.samples_per_pixel = layout.samples;
    params.planar_configuration = layout.planar;
    params.pixel_representation = layout.pixel_representation;
    params.photometric = layout.photometric;
    params.number_of_frames = 1;
    return params;
}

/// The frames asked for as native pixels; `layout` then describes what the codec produced.
[[nodiscard]] kcenon::pacs::Result<std::vector<std::vector<uint8_t>>> native_frames(const core::dicom_file& file, frame_layout& layout,
                                                                                   const std::vector<uint32_t>& numbers) {
    using frame_list = std::vector<std::vector<uint8_t>>;
    auto held = held_frames(file, layout, numbers);
    if (held.is_err() || !file.transfer_syntax().is_encapsulated()) {
        return held;
    }
    const auto codec = compression::codec_factory::create(file.transfer_syntax().uid());
    if (!codec) {
        return kcenon::pacs::pacs_error<frame_list>(kcenon::pacs::error_codes::unsupported_transfer_syntax,
                                                    "No codec for " + std::string(file.transfer_syntax().uid()));
    }
    frame_list decoded;
    for (const auto& frame : held.value()) {
        auto result = codec->decode(frame, params_of(layout));
        if (result.is_err()) {
            return result.error();
        }
        layout.photometric = result.value().output_params.photometric;
        layout.planar = result.value().output_params.planar_configuration;
        decoded.push_back(std::move(result.value().data));
    }
    return decoded;
}

[[nodiscard]] std::vector<uint32_t> every_frame(const frame_layout& layout) {
    std::vector<uint32_t> numbers(layout.frames);
    std::iota(numbers.begin(), numbers.end(), 1U);
    return numbers;
}

// ============================================================================
// PNG (RFC 2083), written without a compressor
// ============================================================================

[[nodiscard]] uint32_t crc32_of(std::span<const uint8_t> data, uint32_t crc = 0xFFFFFFFFU) {
    for (const auto byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return crc;
}

void append_be32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value >> 24));
    out.push_back(static_cast<uint8_t>(value >> 16));
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value));
}

void append_chunk(std::vector<uint8_t>& out, std::string_view kind, std::span<const uint8_t> data) {
    append_be32(out, static_cast<uint32_t>(data.size()));
    const auto start = out.size();
    out.insert(out.end(), kind.begin(), kind.end());
    out.insert(out.end(), data.begin(), data.end());
    append_be32(out, ~crc32_of(std::span<const uint8_t>(out.data() + start, out.size() - start)));
}

}  // namespace

auto encode_png(std::span<const uint8_t> pixels, uint32_t width, uint32_t height, uint16_t channels) -> std::vector<uint8_t> {
    const std::size_t stride = static_cast<std::size_t>(width) * channels;
    if (channels != 1 && channels != 3) {
        return {};
    }
    if (width == 0 || height == 0 || pixels.size() != stride * height) {
        return {};
    }

    // Each row starts with filter type 0, and the zlib stream holds stored deflate blocks.
    std::vector<uint8_t> raw;
    raw.reserve((stride + 1) * height);
    for (uint32_t row = 0; row < height; ++row) {
        raw.push_back(0);
        const auto line = pixels.subspan(row * stride, stride);
        raw.insert(raw.end(), line.begin(), line.end());
    }
    std::vector<uint8_t> compressed{0x78, 0x01};
    constexpr std::size_t largest_block = 65535;
    for (std::size_t offset = 0; offset < raw.size();) {
        const auto size = (std::min)(largest_block, raw.size() - offset);
        compressed.push_back(offset + size == raw.size() ? 1 : 0);
        compressed.push_back(static_cast<uint8_t>(size));
        compressed.push_back(static_cast<uint8_t>(size >> 8));
        compressed.push_back(static_cast<uint8_t>(~size));
        compressed.push_back(static_cast<uint8_t>((~size) >> 8));
        compressed.insert(compressed.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                          raw.begin() + static_cast<std::ptrdiff_t>(offset + size));
        offset += size;
    }
    uint32_t adler_a = 1;
    uint32_t adler_b = 0;
    for (const auto byte : raw) {
        adler_a = (adler_a + byte) % 65521;
        adler_b = (adler_b + adler_a) % 65521;
    }
    append_be32(compressed, (adler_b << 16) | adler_a);

    std::vector<uint8_t> out{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<uint8_t> header;
    append_be32(header, width);
    append_be32(header, height);
    header.push_back(8);
    header.push_back(channels == 1 ? 0 : 2);
    header.push_back(0);
    header.push_back(0);
    header.push_back(0);
    append_chunk(out, "IHDR", header);
    append_chunk(out, "IDAT", compressed);
    append_chunk(out, "IEND", {});
    return out;
}

// ============================================================================
// Rendered Images
// ============================================================================

auto parse_rendered_params(
    std::string_view query_string,
    std::string_view accept_header) -> rendered_params {

    rendered_params params;

    // Determine format from Accept header
    if (accept_header.find("image/jphc") != std::string_view::npos) {
        params.format = rendered_format::jphc;
    } else if (accept_header.find("image/png") != std::string_view::npos) {
        params.format = rendered_format::png;
    } else {
        params.format = rendered_format::jpeg;  // Default to JPEG
    }

    // Parse query parameters
    auto query_params = parse_query_string(std::string(query_string));
    for (const auto& [key, value] : query_params) {
        if (key == "quality") {
            try {
                params.quality = std::stoi(value);
                params.quality = std::clamp(params.quality, 1, 100);
            } catch (...) {}
        } else if (key == "window") {
            // PS3.18 8.3.5.1.4: center, width and function, with the widths PS3.3 C.11.2.1.2 allows each function.
            const auto parts = split(value, ',');
            char* center_end = nullptr;
            char* width_end = nullptr;
            const double center = parts.size() == 3 ? std::strtod(parts[0].c_str(), &center_end) : 0.0;
            const double width = parts.size() == 3 ? std::strtod(parts[1].c_str(), &width_end) : 0.0;
            const bool known = parts.size() == 3 && (parts[2] == "linear" || parts[2] == "linear-exact" || parts[2] == "sigmoid");
            if (!known || center_end == parts[0].c_str() || *center_end != '\0' ||
                width_end == parts[1].c_str() || *width_end != '\0' || width < (parts[2] == "linear" ? 1.0 : (std::numeric_limits<double>::min)())) {
                params.invalid_parameter = "window shall be center,width,function with a function of linear, linear-exact or sigmoid and a width it allows: " + value;
            } else {
                params.window_center = center;
                params.window_width = width;
                params.window_function = parts[2];
            }
        } else if (key == "windowcenter" || key == "window-center") {
            try {
                params.window_center = std::stod(value);
            } catch (...) {}
        } else if (key == "windowwidth" || key == "window-width") {
            try {
                params.window_width = std::stod(value);
            } catch (...) {}
        } else if (key == "viewport") {
            // Format: WxH or W,H. Cropping (PS3.18 8.3.5.1.3 with six values) is refused rather than ignored.
            if (std::count(value.begin(), value.end(), ',') > 1) {
                params.invalid_parameter = "viewport with a source rectangle is not supported: " + value;
            }
            auto sep = value.find_first_of("x,");
            if (sep != std::string::npos) {
                try {
                    params.viewport_width =
                        static_cast<uint16_t>(std::stoul(value.substr(0, sep)));
                    params.viewport_height =
                        static_cast<uint16_t>(std::stoul(value.substr(sep + 1)));
                } catch (...) {}
            }
        } else if (key == "rows") {
            try {
                params.viewport_height = static_cast<uint16_t>(std::stoul(value));
            } catch (...) {}
        } else if (key == "columns") {
            try {
                params.viewport_width = static_cast<uint16_t>(std::stoul(value));
            } catch (...) {}
        } else if (key == "frame") {
            try {
                params.frame = std::stoul(value);
                if (params.frame == 0) params.frame = 1;
            } catch (...) {}
        } else if (key == "annotation") {
            params.burn_annotations = (value == "true" || value == "1");
        }
    }

    return params;
}

namespace {

/// PS3.3 C.11.2.1.3 and C.11.2.1.3.2, onto 0 to 255.
[[nodiscard]] uint8_t windowed_by(std::string_view function, double value, double center, double width) {
    if (function == "sigmoid") {
        return static_cast<uint8_t>(std::clamp(std::lround(255.0 / (1.0 + std::exp(-4.0 * (value - center) / width))), 0L, 255L));
    }
    if (value <= center - width / 2.0) {
        return 0;
    }
    if (value > center + width / 2.0) {
        return 255;
    }
    return static_cast<uint8_t>(std::clamp(std::lround(((value - center) / width + 0.5) * 255.0), 0L, 255L));
}

/// PS3.3 C.11.2.1.2, the linear function, onto 0 to 255.
[[nodiscard]] uint8_t windowed(double value, double center, double width) {
    if (width <= 1.0) {
        return value <= center - 0.5 ? 0 : 255;
    }
    const double half = (width - 1.0) / 2.0;
    if (value <= center - 0.5 - half) {
        return 0;
    }
    if (value > center - 0.5 + half) {
        return 255;
    }
    const double scaled = ((value - (center - 0.5)) / (width - 1.0) + 0.5) * 255.0;
    return static_cast<uint8_t>(std::clamp(std::lround(scaled), 0L, 255L));
}

/// One sample as a number: High Bit places it in its container and Pixel Representation gives its sign.
[[nodiscard]] double stored_value(std::span<const uint8_t> frame, std::size_t index, const frame_layout& layout) {
    uint32_t container = 0;
    if (layout.bits_allocated == 8) {
        container = frame[index];
    } else {
        container = static_cast<uint32_t>(frame[index * 2]) | (static_cast<uint32_t>(frame[index * 2 + 1]) << 8);
    }
    const int shift = layout.high_bit + 1 - layout.bits_stored;
    const uint32_t mask = layout.bits_stored >= 32 ? 0xFFFFFFFFU : (1U << layout.bits_stored) - 1U;
    uint32_t raw = (container >> (shift > 0 ? shift : 0)) & mask;
    if (layout.pixel_representation == 1 && layout.bits_stored < 32 && (raw & (1U << (layout.bits_stored - 1))) != 0) {
        return static_cast<double>(static_cast<int64_t>(raw) - (int64_t{1} << layout.bits_stored));
    }
    return static_cast<double>(raw);
}

[[nodiscard]] std::optional<double> first_number(const core::dicom_dataset& dataset, core::dicom_tag tag) {
    const auto values = split(dataset.get_string(tag), '\\');
    if (values.empty() || trim(values[0]).empty()) {
        return std::nullopt;
    }
    const auto text = trim(values[0]);
    char* end = nullptr;
    const double parsed = std::strtod(text.c_str(), &end);
    return end != text.c_str() && *end == '\0' ? std::optional<double>{parsed} : std::nullopt;
}

/// A grayscale frame on 8 bits: Modality LUT, then the asked or stored window, or else the frame's own range.
[[nodiscard]] std::vector<uint8_t> render_gray(std::span<const uint8_t> frame, const frame_layout& layout,
                                               const core::dicom_dataset& dataset, const rendered_params& params) {
    const std::size_t count = static_cast<std::size_t>(layout.rows) * layout.columns;
    const double slope = first_number(dataset, core::tags::rescale_slope).value_or(1.0);
    const double intercept = first_number(dataset, core::tags::rescale_intercept).value_or(0.0);
    std::vector<double> values(count);
    for (std::size_t i = 0; i < count; ++i) {
        values[i] = stored_value(frame, i, layout) * slope + intercept;
    }
    auto center = params.window_center ? params.window_center : first_number(dataset, core::tags::window_center);
    auto width = params.window_width ? params.window_width : first_number(dataset, core::tags::window_width);
    if (!center || !width || *width < 1.0) {
        const auto [lowest, highest] = std::minmax_element(values.begin(), values.end());
        width = (std::max)(*highest - *lowest + 1.0, 2.0);
        center = (*highest + *lowest + 1.0) / 2.0;
    }
    std::vector<uint8_t> output(count);
    const bool inverted = layout.photometric == compression::photometric_interpretation::monochrome1;
    for (std::size_t i = 0; i < count; ++i) {
        const auto level = params.window_function == "linear" ? windowed(values[i], *center, *width)
                                                              : windowed_by(params.window_function, values[i], *center, *width);
        output[i] = inverted ? static_cast<uint8_t>(255 - level) : level;
    }
    return output;
}

/// An image scaled to fit the viewport, keeping its proportions, by repeating or dropping pixels.
struct scaled_image {
    std::vector<uint8_t> samples;
    uint32_t width;
    uint32_t height;
};

[[nodiscard]] scaled_image fit_viewport(std::vector<uint8_t> samples, uint32_t width, uint32_t height, uint16_t channels,
                                        uint16_t viewport_width, uint16_t viewport_height) {
    if (viewport_width == 0 && viewport_height == 0) {
        return {std::move(samples), width, height};
    }
    const double across = viewport_width != 0 ? static_cast<double>(viewport_width) / width : static_cast<double>(viewport_height) / height;
    const double down = viewport_height != 0 ? static_cast<double>(viewport_height) / height : across;
    const double scale = (std::min)(across, down);
    const auto out_width = std::max<uint32_t>(1, static_cast<uint32_t>(std::lround(width * scale)));
    const auto out_height = std::max<uint32_t>(1, static_cast<uint32_t>(std::lround(height * scale)));
    std::vector<uint8_t> scaled(static_cast<std::size_t>(out_width) * out_height * channels);
    for (uint32_t row = 0; row < out_height; ++row) {
        const uint32_t from_row = (std::min)(height - 1, static_cast<uint32_t>(row / scale));
        for (uint32_t column = 0; column < out_width; ++column) {
            const uint32_t from_column = (std::min)(width - 1, static_cast<uint32_t>(column / scale));
            for (uint16_t channel = 0; channel < channels; ++channel) {
                scaled[(static_cast<std::size_t>(row) * out_width + column) * channels + channel] =
                    samples[(static_cast<std::size_t>(from_row) * width + from_column) * channels + channel];
            }
        }
    }
    return {std::move(scaled), out_width, out_height};
}

/// A color frame as interleaved 8 bit RGB. Empty when the photometric interpretation is not one of these.
[[nodiscard]] std::vector<uint8_t> render_color(std::span<const uint8_t> frame, const frame_layout& layout) {
    const std::size_t count = static_cast<std::size_t>(layout.rows) * layout.columns;
    std::vector<uint8_t> output(count * 3);
    const auto sample = [&](std::size_t pixel, std::size_t channel) -> uint8_t {
        const std::size_t index = layout.planar == 1 ? channel * count + pixel : pixel * 3 + channel;
        if (layout.bits_allocated == 8) {
            return frame[index];
        }
        const auto wide = static_cast<uint32_t>(frame[index * 2]) | (static_cast<uint32_t>(frame[index * 2 + 1]) << 8);
        return static_cast<uint8_t>(wide >> (layout.bits_stored > 8 ? layout.bits_stored - 8 : 0));
    };
    const bool ybr = layout.photometric == compression::photometric_interpretation::ycbcr_full;
    if (!ybr && layout.photometric != compression::photometric_interpretation::rgb) {
        return {};
    }
    const auto to_byte = [](double value) { return static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 255L)); };
    for (std::size_t pixel = 0; pixel < count; ++pixel) {
        const auto first = sample(pixel, 0);
        const auto second = sample(pixel, 1);
        const auto third = sample(pixel, 2);
        if (ybr) {
            output[pixel * 3] = to_byte(first + 1.402 * (third - 128.0));
            output[pixel * 3 + 1] = to_byte(first - 0.344136 * (second - 128.0) - 0.714136 * (third - 128.0));
            output[pixel * 3 + 2] = to_byte(first + 1.772 * (second - 128.0));
        } else {
            output[pixel * 3] = first;
            output[pixel * 3 + 1] = second;
            output[pixel * 3 + 2] = third;
        }
    }
    return output;
}

}  // namespace

auto apply_window_level(
    std::span<const uint8_t> pixel_data,
    uint16_t width,
    uint16_t height,
    uint16_t bits_stored,
    bool is_signed,
    double window_center,
    double window_width,
    double rescale_slope,
    double rescale_intercept) -> std::vector<uint8_t> {

    std::vector<uint8_t> output(static_cast<size_t>(width) * height);

    size_t pixel_count = static_cast<size_t>(width) * height;
    bool is_16bit = (bits_stored > 8);

    for (size_t i = 0; i < pixel_count; ++i) {
        double value;

        if (is_16bit) {
            // 16-bit pixel data
            size_t byte_offset = i * 2;
            if (byte_offset + 1 >= pixel_data.size()) break;

            if (is_signed) {
                int16_t raw_value = static_cast<int16_t>(
                    pixel_data[byte_offset] |
                    (pixel_data[byte_offset + 1] << 8));
                value = raw_value * rescale_slope + rescale_intercept;
            } else {
                uint16_t raw_value = static_cast<uint16_t>(
                    pixel_data[byte_offset] |
                    (pixel_data[byte_offset + 1] << 8));
                value = raw_value * rescale_slope + rescale_intercept;
            }
        } else {
            // 8-bit pixel data
            if (i >= pixel_data.size()) break;

            if (is_signed) {
                value = static_cast<int8_t>(pixel_data[i]) * rescale_slope +
                        rescale_intercept;
            } else {
                value = pixel_data[i] * rescale_slope + rescale_intercept;
            }
        }

        output[i] = windowed(value, window_center, window_width);
    }

    return output;
}

auto render_dicom_image(
    std::string_view file_path,
    const rendered_params& params) -> rendered_result {

    if (!params.invalid_parameter.empty()) {
        return rendered_result::error(params.invalid_parameter);
    }

    // Load DICOM file
    std::ifstream file(std::string(file_path), std::ios::binary);
    if (!file) {
        return rendered_result::error("Failed to open DICOM file");
    }

    std::vector<uint8_t> file_data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();

    auto dicom_result = core::dicom_file::from_bytes(
        std::span<const uint8_t>(file_data.data(), file_data.size()));

    if (dicom_result.is_err()) {
        return rendered_result::error("Failed to parse DICOM file");
    }

    const auto& dataset = dicom_result.value().dataset();
    auto layout = layout_of(dataset);
    if (!layout) {
        return rendered_result::error("Missing required image attributes");
    }
    if (params.frame == 0 || params.frame > layout->frames) {
        return rendered_result::not_found("Requested frame does not exist");
    }

    if ((layout->bits_allocated != 8 && layout->bits_allocated != 16) || layout->bits_stored == 0 || layout->bits_stored > layout->bits_allocated) {
        return rendered_result::error("Cannot render " + std::to_string(layout->bits_allocated) + " bits allocated");
    }

    auto frames = native_frames(dicom_result.value(), *layout, {params.frame});
    if (frames.is_err()) {
        return rendered_result::error("Cannot read the frame: " + frames.error().message);
    }
    const auto& frame = frames.value().front();
    // What a codec returned or a file held decides how far the renderers read, not what the header says.
    if (frame.size() < layout->frame_bytes()) {
        return rendered_result::error("The frame is shorter than its image attributes say");
    }

    // Window applied to grayscale; color goes out as it is
    const uint16_t channels = layout->samples == 1 ? 1 : 3;
    std::vector<uint8_t> output_pixels;
    if (layout->samples == 1) {
        if (layout->photometric != compression::photometric_interpretation::monochrome1 &&
            layout->photometric != compression::photometric_interpretation::monochrome2) {
            return rendered_result::error("Cannot render this photometric interpretation");
        }
        output_pixels = render_gray(frame, *layout, dataset, params);
    } else if (layout->samples == 3) {
        output_pixels = render_color(frame, *layout);
        if (output_pixels.empty()) {
            return rendered_result::error("Cannot render this photometric interpretation");
        }
    } else {
        return rendered_result::error("Cannot render this number of samples per pixel");
    }

    constexpr uint64_t largest_viewport_area = 1ULL << 26;
    if (static_cast<uint64_t>(params.viewport_width) * params.viewport_height > largest_viewport_area) {
        return rendered_result::error("The viewport is larger than this server renders");
    }
    auto image = fit_viewport(std::move(output_pixels), layout->columns, layout->rows, channels, params.viewport_width, params.viewport_height);

    // Encode to requested format
    encoding::compression::image_params img_params;
    img_params.width = static_cast<uint16_t>(image.width);
    img_params.height = static_cast<uint16_t>(image.height);
    img_params.bits_allocated = 8;
    img_params.bits_stored = 8;
    img_params.high_bit = 7;
    img_params.samples_per_pixel = channels;
    img_params.photometric =
        (channels == 1) ?
        encoding::compression::photometric_interpretation::monochrome2 :
        encoding::compression::photometric_interpretation::rgb;

    if (params.format == rendered_format::jphc) {
        encoding::compression::htj2k_codec codec(
            /*lossless=*/false, /*use_rpcl=*/false);

        auto result = codec.encode(image.samples, img_params);
        if (result.is_err()) {
            return rendered_result::error("HTJ2K encoding failed: " +
                                          result.error().message);
        }

        return rendered_result::ok(std::move(result.value().data), media_type::jphc);
    } else if (params.format == rendered_format::jpeg) {
        encoding::compression::jpeg_baseline_codec codec;

        encoding::compression::compression_options opts;
        opts.quality = params.quality;

        auto result = codec.encode(image.samples, img_params, opts);
        if (result.is_err()) {
            return rendered_result::error("JPEG encoding failed: " +
                                          result.error().message);
        }

        return rendered_result::ok(std::move(result.value().data), media_type::jpeg);
    }
    return rendered_result::ok(encode_png(image.samples, image.width, image.height, channels), media_type::png);
}

} // namespace kcenon::pacs::web::dicomweb

namespace kcenon::pacs::web::endpoints {

namespace {

namespace tags = core::tags;
using encoding::vr_type;

const std::vector<std::string> read_scope{"dicomweb.read"};
const std::vector<std::string> search_scopes{"dicomweb.read", "dicomweb.search"};
const std::vector<std::string> write_scope{"dicomweb.write"};

/// What a search returns when it asks for no limit, and the most one response holds (PS3.18 8.3.4.4).
constexpr std::size_t default_search_limit = 100;
constexpr std::size_t largest_search_page = 1000;

/**
 * @brief Authenticate a DICOMweb request with OAuth 2.0
 *
 * With OAuth 2.0 enabled, validates the Bearer token and checks the required scope.
 * Without it, DICOMweb answers everyone who can reach the port.
 */
bool check_dicomweb_auth(const std::shared_ptr<rest_server_context>& ctx,
                          const crow::request& req,
                          crow::response& res,
                          const std::vector<std::string>& required_scopes) {
    if (ctx->oauth2 && ctx->oauth2->enabled()) {
        auto auth = ctx->oauth2->authenticate(req, res);
        if (!auth) return false;

        if (!required_scopes.empty()) {
            return ctx->oauth2->require_any_scope(
                auth->claims, res, required_scopes);
        }
        return true;
    }
    return true;
}

/**
 * @brief Add CORS headers to response
 */
void add_cors_headers(crow::response& res, const rest_server_context& ctx) {
    if (ctx.config && !ctx.config->cors_allowed_origins.empty()) {
        res.add_header("Access-Control-Allow-Origin",
                       ctx.config->cors_allowed_origins);
    }
}

/**
 * @brief Read file contents into byte vector
 */
std::vector<uint8_t> read_file_bytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return {};
    }

    return buffer;
}

[[nodiscard]] crow::response failure(crow::response res, int status, std::string_view code, std::string_view message) {
    res.code = status;
    res.set_header("Content-Type", "application/json");
    res.body = make_error_json(code, message);
    return res;
}

[[nodiscard]] crow::response refusal(crow::response res, const dicomweb::media_choice& choice) {
    return failure(std::move(res), choice.status, choice.status == 400 ? "BAD_ACCEPT" : "NOT_ACCEPTABLE", choice.detail);
}

/// Every DICOMweb route starts here, so none can be reached without its scope.
template <typename Handler>
[[nodiscard]] crow::response guarded(const std::shared_ptr<rest_server_context>& ctx, const crow::request& req,
                                     const std::vector<std::string>& scopes, Handler&& handler) {
    crow::response res;
    add_cors_headers(res, *ctx);
    if (!check_dicomweb_auth(ctx, req, res, scopes)) {
        return res;
    }
    if (!ctx->database) {
        return failure(std::move(res), 503, "DATABASE_UNAVAILABLE", "Database not configured");
    }
    return handler(std::move(res));
}

/// The Accept Query Parameter stands in for the header when present (PS3.18 8.3.3.1).
[[nodiscard]] std::string acceptable_types(const crow::request& req) {
    if (const char* value = req.url_params.get("accept")) {
        return value;
    }
    return req.get_header_value("Accept");
}

/// The scheme and authority this request came in on, for URLs in responses. Anything else in Host is left out.
[[nodiscard]] std::string base_url(const crow::request& req, const rest_server_context& ctx) {
    const auto host = req.get_header_value("Host");
    const auto allowed = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-:[]";
    if (host.empty() || host.find_first_not_of(allowed) != std::string::npos) {
        return {};
    }
    return std::string(ctx.config && ctx.config->enable_tls ? "https://" : "http://") + host;
}

[[nodiscard]] std::string instance_path(std::string_view study, std::string_view series, std::string_view sop) {
    return "/dicomweb/studies/" + std::string(study) + "/series/" + std::string(series) + "/instances/" + std::string(sop);
}

/// A series that is where the URL says it is.
[[nodiscard]] std::optional<storage::series_record> series_under(const storage::index_database& database, const std::string& study_uid,
                                                                 const std::string& series_uid) {
    const auto study = database.find_study(study_uid);
    auto series = database.find_series(series_uid);
    if (!study || !series || series->study_pk != study->pk) {
        return std::nullopt;
    }
    return series;
}

/// An instance that is where the URL says it is.
[[nodiscard]] std::optional<storage::instance_record> instance_under(const storage::index_database& database, const std::string& study_uid,
                                                                     const std::string& series_uid, const std::string& sop_uid) {
    const auto series = series_under(database, study_uid, series_uid);
    auto instance = database.find_instance(sop_uid);
    if (!series || !instance || instance->series_pk != series->pk) {
        return std::nullopt;
    }
    return instance;
}

/// One object for a response, or the status that replaces the whole response.
struct object_bytes {
    int status{200};
    std::string message;
    std::vector<uint8_t> data;
    std::string syntax;
};

[[nodiscard]] object_bytes refused_object(int status, std::string message) {
    object_bytes refused;
    refused.status = status;
    refused.message = std::move(message);
    return refused;
}

/**
 * @brief The object in the Transfer Syntax the request selects (PS3.18 8.7.3)
 *
 * No parameter means Explicit VR Little Endian unless only lossy pixels are held; "*" means as held;
 * Implicit VR and Big Endian are never returned. There is no encoder here for other syntaxes.
 */
[[nodiscard]] object_bytes object_in_syntax(const std::string& path, std::string_view wanted) {
    const auto& explicit_le = encoding::transfer_syntax::explicit_vr_little_endian;
    auto bytes = read_file_bytes(path);
    if (bytes.empty()) {
        return refused_object(500, "An indexed object cannot be read");
    }
    auto file = core::dicom_file::from_bytes(std::span<const uint8_t>(bytes.data(), bytes.size()));
    if (file.is_err()) {
        return refused_object(500, "An indexed object cannot be parsed");
    }
    const auto held = file.value().transfer_syntax();
    const bool never_returned = held == encoding::transfer_syntax::implicit_vr_little_endian ||
                                held == encoding::transfer_syntax::explicit_vr_big_endian;

    bool keep_as_held = false;
    if (wanted.empty()) {
        const auto codec = held.is_encapsulated() ? encoding::compression::codec_factory::create(held.uid()) : nullptr;
        keep_as_held = held == explicit_le || (held.is_encapsulated() && (!codec || codec->is_lossy()));
    } else if (wanted == "*") {
        keep_as_held = !never_returned;
    } else if (wanted == held.uid() && !never_returned) {
        keep_as_held = true;
    } else if (wanted != explicit_le.uid()) {
        return refused_object(406, "Transfer Syntax " + std::string(wanted) + " is not held and cannot be produced");
    }
    if (keep_as_held) {
        object_bytes kept;
        kept.data = std::move(bytes);
        kept.syntax = std::string(held.uid());
        return kept;
    }

    auto dataset = file.value().dataset();
    if (held.is_encapsulated()) {
        auto layout = dicomweb::layout_of(dataset);
        if (!layout) {
            return refused_object(500, "Encapsulated pixel data without image attributes");
        }
        auto frames = dicomweb::native_frames(file.value(), *layout, dicomweb::every_frame(*layout));
        if (frames.is_err()) {
            return refused_object(406, "Cannot decode to Explicit VR Little Endian: " + frames.error().message);
        }
        std::vector<uint8_t> pixels;
        for (const auto& frame : frames.value()) {
            pixels.insert(pixels.end(), frame.begin(), frame.end());
        }
        dataset.insert(core::dicom_element(tags::pixel_data, layout->bits_allocated > 8 ? vr_type::OW : vr_type::OB, pixels));
        dataset.set_string(tags::photometric_interpretation, vr_type::CS, encoding::compression::to_string(layout->photometric));
        if (layout->samples > 1) {
            dataset.set_numeric<uint16_t>(dicomweb::planar_configuration_tag, vr_type::US, layout->planar);
        }
    }
    auto encoded = core::dicom_file::create(std::move(dataset), explicit_le).to_bytes();
    if (encoded.empty()) {
        return refused_object(500, "The object cannot be encoded in Explicit VR Little Endian");
    }
    object_bytes converted;
    converted.data = std::move(encoded);
    converted.syntax = std::string(explicit_le.uid());
    return converted;
}

/// The Pixel Data of one object as Bulk Data: native, whatever is held, or refused with a status.
[[nodiscard]] object_bytes native_pixels_of(const std::string& path) {
    auto bytes = read_file_bytes(path);
    if (bytes.empty()) {
        return refused_object(500, "An indexed object cannot be read");
    }
    auto file = core::dicom_file::from_bytes(std::span<const uint8_t>(bytes.data(), bytes.size()));
    if (file.is_err()) {
        return refused_object(500, "An indexed object cannot be parsed");
    }
    auto layout = dicomweb::layout_of(file.value().dataset());
    if (!layout) {
        return refused_object(404, "The object has no pixels");
    }
    auto frames = dicomweb::native_frames(file.value(), *layout, dicomweb::every_frame(*layout));
    if (frames.is_err()) {
        return refused_object(406, frames.error().message);
    }
    object_bytes pixels;
    for (const auto& frame : frames.value()) {
        pixels.data.insert(pixels.data.end(), frame.begin(), frame.end());
    }
    return pixels;
}

/// Bulk Data resources are where metadata sends a reader for what it leaves out; one URL per instance.
[[nodiscard]] std::string bulk_prefix_of(const core::dicom_dataset& dataset, const std::string& base) {
    return base + instance_path(dataset.get_string(tags::study_instance_uid), dataset.get_string(tags::series_instance_uid),
                                 dataset.get_string(tags::sop_instance_uid)) + "/bulkdata/";
}

/// Everything a retrieve of these files holds, or the status of the failure. 200 means all of it (PS3.18 10.4.3.1).
[[nodiscard]] crow::response instances_response(crow::response res, const std::vector<std::string>& files, const dicomweb::media_choice& choice,
                                                const std::string& base) {
    if (files.empty()) {
        return failure(std::move(res), 404, "NOT_FOUND", "No instances found");
    }
    if (choice.media == dicomweb::media_type::octet_stream) {
        dicomweb::multipart_builder builder(dicomweb::media_type::octet_stream);
        for (const auto& path : files) {
            auto pixels = native_pixels_of(path);
            if (pixels.status == 404) {
                continue;
            }
            if (pixels.status != 200) {
                return failure(std::move(res), pixels.status, "RETRIEVE_FAILED", pixels.message);
            }
            const auto parsed = core::dicom_file::open(path);
            if (parsed.is_err()) {
                return failure(std::move(res), 500, "RETRIEVE_FAILED", "An indexed object cannot be parsed");
            }
            builder.add_part_with_location(std::move(pixels.data), bulk_prefix_of(parsed.value().dataset(), base) + "7FE00010");
        }
        if (builder.empty()) {
            return failure(std::move(res), 404, "NOT_FOUND", "These instances hold no Bulk Data");
        }
        res.code = 200;
        res.set_header("Content-Type", builder.content_type_header());
        res.body = builder.build();
        return res;
    }

    dicomweb::multipart_builder builder(dicomweb::media_type::dicom);
    for (const auto& path : files) {
        auto object = object_in_syntax(path, choice.transfer_syntax);
        if (object.status != 200) {
            return failure(std::move(res), object.status, "RETRIEVE_FAILED", object.message);
        }
        if (!choice.multipart) {
            res.code = 200;
            res.set_header("Content-Type", std::string(dicomweb::media_type::dicom) + "; transfer-syntax=" + object.syntax);
            res.body.assign(reinterpret_cast<const char*>(object.data.data()), object.data.size());
            return res;
        }
        const auto part_type = std::string(dicomweb::media_type::dicom) + "; transfer-syntax=" + object.syntax;
        builder.add_part(std::move(object.data), std::string_view(part_type));
    }
    res.code = 200;
    res.set_header("Content-Type", builder.content_type_header());
    res.body = builder.build();
    return res;
}

/// The metadata of these files, as one JSON array or one XML part each (PS3.18 10.4.3.3.2).
[[nodiscard]] crow::response metadata_response(crow::response res, const std::vector<std::string>& files, const dicomweb::media_choice& choice,
                                               const std::string& base) {
    if (files.empty()) {
        return failure(std::move(res), 404, "NOT_FOUND", "No instances found");
    }
    std::string json = "[";
    dicomweb::multipart_builder xml(dicomweb::media_type::dicom_xml);
    bool first = true;
    for (const auto& path : files) {
        auto file = core::dicom_file::open(path);
        if (file.is_err()) {
            return failure(std::move(res), 500, "RETRIEVE_FAILED", "An indexed object cannot be read");
        }
        const auto& dataset = file.value().dataset();
        const auto prefix = bulk_prefix_of(dataset, base);
        if (choice.multipart) {
            const auto text = dicomweb::dataset_to_native_xml(dataset, false, prefix);
            xml.add_part(std::vector<uint8_t>(text.begin(), text.end()));
        } else {
            if (!first) {
                json += ",";
            }
            first = false;
            json += dicomweb::dataset_to_dicom_json(dataset, false, prefix);
        }
    }
    res.code = 200;
    if (choice.multipart) {
        res.set_header("Content-Type", xml.content_type_header());
        res.body = xml.build();
    } else {
        res.set_header("Content-Type", std::string(dicomweb::media_type::dicom_json));
        res.body = json + "]";
    }
    return res;
}

/// The frame list of a path, checked before anything is looked up.
[[nodiscard]] std::optional<std::vector<uint32_t>> requested_frames(const std::string& frame_list) {
    auto frames = dicomweb::parse_frame_list(frame_list);
    if (!frames || frames->empty()) {
        return std::nullopt;
    }
    return frames;
}

// ---- STOW-RS -----------------------------------------------------------------------------------------

[[nodiscard]] std::string lowercase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

[[nodiscard]] std::string media_type_of(const std::string& content_type) {
    const auto end = content_type.find(';');
    auto main = content_type.substr(0, end);
    const auto first = main.find_first_not_of(" \t");
    const auto last = main.find_last_not_of(" \t");
    return first == std::string::npos ? std::string{} : lowercase(main.substr(first, last - first + 1));
}

void store_part(const std::shared_ptr<rest_server_context>& ctx, const dicomweb::multipart_part& part,
                const std::optional<std::string>& target_study, dicomweb::store_response& response) {
    dicomweb::store_instance_result result;
    const auto failed = [&](std::string code, std::string message) {
        result.success = false;
        result.error_code = std::move(code);
        result.error_message = std::move(message);
        response.failed_instances.push_back(std::move(result));
    };

    // A part nobody can store is a failure to report, or the sender would count it as kept.
    if (media_type_of(part.content_type) != dicomweb::media_type::dicom) {
        failed("UNSUPPORTED_PART", "Only application/dicom parts are stored");
        return;
    }
    auto file = core::dicom_file::from_bytes(std::span<const uint8_t>(part.data.data(), part.data.size()));
    if (file.is_err()) {
        failed("INVALID_DATA", "Failed to parse DICOM data");
        return;
    }
    const auto& dataset = file.value().dataset();
    result.sop_class_uid = dataset.get_string(tags::sop_class_uid);
    result.sop_instance_uid = dataset.get_string(tags::sop_instance_uid);

    const auto validation = target_study ? dicomweb::validate_instance(dataset, std::string_view(*target_study))
                                         : dicomweb::validate_instance(dataset);
    if (!validation) {
        failed(validation.error_code, validation.error_message);
        return;
    }

    const auto archived = storage::archive_instance(*ctx->file_storage, *ctx->database, dataset, file.value().transfer_syntax());
    switch (archived.status) {
        case storage::archive_status::stored:
        case storage::archive_status::duplicate_ignored:
            break;
        case storage::archive_status::duplicate_refused:
            failed("DUPLICATE", archived.message);
            return;
        case storage::archive_status::rejected:
            failed("MISSING_ATTRIBUTE", archived.message);
            return;
        case storage::archive_status::failed:
            failed("STORAGE_ERROR", archived.message);
            return;
    }
    result.success = true;
    result.retrieve_url = instance_path(dataset.get_string(tags::study_instance_uid), dataset.get_string(tags::series_instance_uid),
                                        result.sop_instance_uid);
    response.referenced_instances.push_back(std::move(result));
}

/// Both Store routes: PS3.18 Table 10.5.3-1.
[[nodiscard]] crow::response store_instances(const std::shared_ptr<rest_server_context>& ctx, const crow::request& req,
                                             const std::optional<std::string>& target_study) {
    return guarded(ctx, req, write_scope, [&](crow::response res) {
        if (!ctx->file_storage) {
            return failure(std::move(res), 503, "SERVICE_UNAVAILABLE", "File storage not configured");
        }
        if (ctx->config && req.body.size() > ctx->config->max_body_size) {
            return failure(std::move(res), 413, "PAYLOAD_TOO_LARGE",
                           "The request exceeds the configured limit of " + std::to_string(ctx->config->max_body_size) + " bytes");
        }
        const auto choice = dicomweb::choose_media_type(acceptable_types(req), dicomweb::wado_resource::store_response);
        if (!choice.ok()) {
            return refusal(std::move(res), choice);
        }
        const auto content_type = req.get_header_value("Content-Type");
        const auto declared_type = dicomweb::multipart_parser::extract_type(content_type);
        if (media_type_of(content_type) != dicomweb::media_type::multipart_related ||
            (declared_type && lowercase(*declared_type) != dicomweb::media_type::dicom)) {
            return failure(std::move(res), 415, "UNSUPPORTED_MEDIA_TYPE",
                           "Content-Type must be multipart/related with parts of type application/dicom");
        }
        const auto parsed = dicomweb::multipart_parser::parse(content_type, req.body);
        if (!parsed) {
            return failure(std::move(res), 400, parsed.error->code, parsed.error->message);
        }

        dicomweb::store_response response;
        for (const auto& part : parsed.parts) {
            store_part(ctx, part, target_study, response);
        }

        const auto base = base_url(req, *ctx);
        res.code = response.all_failed() ? 409 : response.partial_success() ? 202 : 200;
        res.set_header("Content-Type", choice.media);
        res.body = choice.media == dicomweb::media_type::dicom_xml ? dicomweb::build_store_response_xml(response, base)
                                                                   : dicomweb::build_store_response_json(response, base);
        return res;
    });
}

// ---- QIDO-RS -----------------------------------------------------------------------------------------

/// Which of the routes of PS3.18 Table 10.6.1-1 was asked, and so what each row carries (10.6.3.3).
struct search_route {
    services::query_level level;
    std::optional<std::string> study_uid;
    std::optional<std::string> series_uid;
};

struct search_match {
    std::string study_uid;
    std::string series_uid;
    std::string sop_uid;
};

[[nodiscard]] bool is_key_of(const std::vector<core::dicom_tag>& keys, core::dicom_tag tag) {
    return std::find(keys.begin(), keys.end(), tag) != keys.end();
}

// The keys the index can match, by level: what qr_index answers for Study Root.
const std::vector<core::dicom_tag> study_level_keys{
    tags::patient_id, tags::patient_name, tags::patient_birth_date, tags::patient_sex, tags::study_instance_uid, tags::study_date,
    tags::study_time, tags::accession_number, tags::study_id, tags::referring_physician_name, tags::study_description, tags::modalities_in_study};
const std::vector<core::dicom_tag> series_level_keys{tags::series_instance_uid, tags::modality, tags::series_number, tags::series_description};
const std::vector<core::dicom_tag> image_level_keys{tags::sop_instance_uid, tags::instance_number, tags::sop_class_uid};

class index_search {
public:
    index_search(storage::index_database& database, const dicomweb::search_parameters& parameters, const search_route& route)
        : database_(database), route_(route) {
        const auto start = [](core::dicom_dataset& keys) { keys.set_string(tags::specific_character_set, vr_type::CS, "ISO_IR 192"); };
        start(study_);
        start(series_);
        start(image_);
        study_.set_string(tags::study_instance_uid, vr_type::UI, route.study_uid.value_or(""));
        series_.set_string(tags::series_instance_uid, vr_type::UI, route.series_uid.value_or(""));
        image_.set_string(tags::sop_instance_uid, vr_type::UI, "");
        for (const auto& key : parameters.keys) {
            auto* level_keys = is_key_of(study_level_keys, key.tag) ? &study_
                             : is_key_of(series_level_keys, key.tag) && route.level != services::query_level::study ? &series_
                             : is_key_of(image_level_keys, key.tag) && route.level == services::query_level::image ? &image_ : nullptr;
            if (level_keys == nullptr) {
                not_used_.push_back(key.name);
                continue;
            }
            // The path names the resource; a key cannot move the search elsewhere.
            if ((key.tag == tags::study_instance_uid && route.study_uid) || (key.tag == tags::series_instance_uid && route.series_uid)) {
                if (key.value != (key.tag == tags::study_instance_uid ? *route.study_uid : *route.series_uid)) {
                    contradicted_ = true;
                }
                continue;
            }
            level_keys->set_string(key.tag, key.vr, key.value);
        }
    }

    [[nodiscard]] const std::vector<std::string>& keys_not_used() const noexcept { return not_used_; }

    /// Hierarchical Search Method (8.3.4.1.1): the same evaluation C-FIND uses, one level at a time.
    [[nodiscard]] std::optional<std::vector<search_match>> run() const {
        std::vector<search_match> matches;
        if (contradicted_) {
            return matches;
        }
        const auto studies = find(services::query_level::study, study_);
        if (!studies) {
            return std::nullopt;
        }
        for (const auto& study : *studies) {
            const auto study_uid = study.get_string(tags::study_instance_uid);
            if (route_.level == services::query_level::study) {
                matches.push_back({study_uid, {}, {}});
                continue;
            }
            auto series_keys = series_;
            series_keys.set_string(tags::study_instance_uid, vr_type::UI, study_uid);
            const auto series = find(services::query_level::series, series_keys);
            if (!series) {
                return std::nullopt;
            }
            for (const auto& one_series : *series) {
                const auto series_uid = one_series.get_string(tags::series_instance_uid);
                if (route_.level == services::query_level::series) {
                    matches.push_back({study_uid, series_uid, {}});
                    continue;
                }
                auto image_keys = image_;
                image_keys.set_string(tags::study_instance_uid, vr_type::UI, study_uid);
                image_keys.set_string(tags::series_instance_uid, vr_type::UI, series_uid);
                const auto images = find(services::query_level::image, image_keys);
                if (!images) {
                    return std::nullopt;
                }
                for (const auto& image : *images) {
                    matches.push_back({study_uid, series_uid, image.get_string(tags::sop_instance_uid)});
                }
            }
        }
        return matches;
    }

private:
    [[nodiscard]] std::optional<std::vector<core::dicom_dataset>> find(services::query_level level, const core::dicom_dataset& keys) const {
        const services::find_request request{services::qr_model::study_root, level, keys, {}, {}};
        auto outcome = services::find_in_index(database_, request);
        if (outcome.failure != services::qr_failure::none) {
            return std::nullopt;
        }
        return std::move(outcome.matches);
    }

    storage::index_database& database_;
    const search_route& route_;
    core::dicom_dataset study_;
    core::dicom_dataset series_;
    core::dicom_dataset image_;
    std::vector<std::string> not_used_;
    bool contradicted_{false};
};

void set_number(core::dicom_dataset& row, core::dicom_tag tag, vr_type vr, std::optional<int> value) {
    row.set_string(tag, vr, value ? std::to_string(*value) : std::string{});
}

/// Table 10.6.3-3
void add_study_attributes(core::dicom_dataset& row, const storage::study_record& study, const std::optional<storage::patient_record>& patient) {
    row.set_string(tags::study_instance_uid, vr_type::UI, study.study_uid);
    row.set_string(tags::study_date, vr_type::DA, study.study_date);
    row.set_string(tags::study_time, vr_type::TM, study.study_time);
    row.set_string(tags::accession_number, vr_type::SH, study.accession_number);
    row.set_string(tags::modalities_in_study, vr_type::CS, study.modalities_in_study);
    row.set_string(tags::referring_physician_name, vr_type::PN, study.referring_physician);
    row.set_string(tags::study_description, vr_type::LO, study.study_description);
    row.set_string(tags::study_id, vr_type::SH, study.study_id);
    row.set_string(tags::patient_name, vr_type::PN, patient ? patient->patient_name : std::string{});
    row.set_string(tags::patient_id, vr_type::LO, patient ? patient->patient_id : std::string{});
    row.set_string(tags::patient_birth_date, vr_type::DA, patient ? patient->birth_date : std::string{});
    row.set_string(tags::patient_sex, vr_type::CS, patient ? patient->sex : std::string{});
    set_number(row, tags::number_of_study_related_series, vr_type::IS, study.num_series);
    set_number(row, tags::number_of_study_related_instances, vr_type::IS, study.num_instances);
}

/// Table 10.6.3-4
void add_series_attributes(core::dicom_dataset& row, const storage::series_record& series) {
    row.set_string(tags::series_instance_uid, vr_type::UI, series.series_uid);
    row.set_string(tags::modality, vr_type::CS, series.modality);
    row.set_string(tags::series_description, vr_type::LO, series.series_description);
    set_number(row, tags::series_number, vr_type::IS, series.series_number);
    set_number(row, tags::number_of_series_related_instances, vr_type::IS, series.num_instances);
}

/// Table 10.6.3-5
void add_instance_attributes(core::dicom_dataset& row, const storage::instance_record& instance) {
    row.set_string(tags::sop_class_uid, vr_type::UI, instance.sop_class_uid);
    row.set_string(tags::sop_instance_uid, vr_type::UI, instance.sop_uid);
    set_number(row, tags::instance_number, vr_type::IS, instance.instance_number);
    if (instance.rows) {
        row.set_numeric<uint16_t>(tags::rows, vr_type::US, static_cast<uint16_t>(*instance.rows));
    }
    if (instance.columns) {
        row.set_numeric<uint16_t>(tags::columns, vr_type::US, static_cast<uint16_t>(*instance.columns));
    }
    if (instance.bits_allocated) {
        row.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, static_cast<uint16_t>(*instance.bits_allocated));
    }
    if (instance.number_of_frames) {
        set_number(row, dicomweb::number_of_frames_tag, vr_type::IS, instance.number_of_frames);
    }
}

/// The attributes a response carries beyond the table, from the object they came from (8.3.4.3).
void add_included_fields(core::dicom_dataset& row, const dicomweb::search_parameters& parameters, const search_match& match,
                         const storage::index_database& database) {
    if (parameters.include_fields.empty() && !(parameters.include_all && !match.sop_uid.empty())) {
        return;
    }
    std::string path;
    if (!match.sop_uid.empty()) {
        if (auto file = database.get_file_path(match.sop_uid); file.is_ok() && file.value()) {
            path = *file.value();
        }
    } else {
        auto files = match.series_uid.empty() ? database.get_study_files(match.study_uid) : database.get_series_files(match.series_uid);
        if (files.is_ok() && !files.value().empty()) {
            path = files.value().front();
        }
    }
    if (path.empty()) {
        return;
    }
    auto file = core::dicom_file::open(path);
    if (file.is_err()) {
        return;
    }
    const auto text = core::utf8_text_copy(file.value().dataset());
    if (parameters.include_all && !match.sop_uid.empty()) {
        for (const auto& [tag, element] : text) {
            if (!row.contains(tag) && !dicomweb::is_bulk_data_tag(tag.combined()) && tag.group() != 0x0002) {
                row.insert(element);
            }
        }
    }
    for (const auto tag : parameters.include_fields) {
        if (const auto* element = text.get(tag); element != nullptr && !row.contains(tag)) {
            row.insert(*element);
        }
    }
}

[[nodiscard]] crow::response search_response(const std::shared_ptr<rest_server_context>& ctx, const crow::request& req, const search_route& route) {
    return guarded(ctx, req, search_scopes, [&](crow::response res) {
        const auto choice = dicomweb::choose_media_type(acceptable_types(req), dicomweb::wado_resource::search);
        if (!choice.ok()) {
            return refusal(std::move(res), choice);
        }
        const auto parameters = dicomweb::parse_search_parameters(req.raw_url);
        if (parameters.error) {
            return failure(std::move(res), 400, "BAD_QUERY", *parameters.error);
        }
        if (route.study_uid && !ctx->database->find_study(*route.study_uid)) {
            return failure(std::move(res), 404, "NOT_FOUND", "Study not found");
        }
        if (route.series_uid && !series_under(*ctx->database, *route.study_uid, *route.series_uid)) {
            return failure(std::move(res), 404, "NOT_FOUND", "Series not found");
        }

        const index_search search(*ctx->database, parameters, route);
        const auto matches = search.run();
        if (!matches) {
            return failure(std::move(res), 500, "QUERY_ERROR", "The index could not answer");
        }

        // 8.3.4.4: min(max(0, matches - offset), the largest page, limit).
        const auto offset = parameters.offset.value_or(0);
        const auto limit = (std::min)(parameters.limit.value_or(default_search_limit), largest_search_page);
        const auto first = (std::min)(offset, matches->size());
        const auto count = (std::min)(matches->size() - first, limit);
        const auto remaining = matches->size() - first - count;

        const auto base = base_url(req, *ctx);
        const bool with_study = !route.study_uid;
        const bool with_series = !route.series_uid && route.level == services::query_level::image;
        std::string json = "[";
        dicomweb::multipart_builder xml(dicomweb::media_type::dicom_xml);
        for (std::size_t index = first; index < first + count; ++index) {
            const auto& match = (*matches)[index];
            core::dicom_dataset row;
            if (route.level == services::query_level::study || with_study) {
                const auto study = ctx->database->find_study(match.study_uid);
                if (!study) {
                    return failure(std::move(res), 500, "QUERY_ERROR", "A match has no study row");
                }
                add_study_attributes(row, *study, ctx->database->find_patient_by_pk(study->patient_pk));
            }
            std::string url = base + "/dicomweb/studies/" + match.study_uid;
            if (route.level == services::query_level::series || with_series) {
                const auto series = ctx->database->find_series(match.series_uid);
                if (!series) {
                    return failure(std::move(res), 500, "QUERY_ERROR", "A match has no series row");
                }
                add_series_attributes(row, *series);
            }
            if (!match.series_uid.empty()) {
                url += "/series/" + match.series_uid;
            }
            if (route.level == services::query_level::image) {
                const auto instance = ctx->database->find_instance(match.sop_uid);
                if (!instance) {
                    return failure(std::move(res), 500, "QUERY_ERROR", "A match has no instance row");
                }
                add_instance_attributes(row, *instance);
                url += "/instances/" + match.sop_uid;
            }
            row.set_string(core::dicom_tag{0x0008, 0x1190}, vr_type::UR, url);
            add_included_fields(row, parameters, match, *ctx->database);
            if (choice.multipart) {
                const auto text = dicomweb::dataset_to_native_xml(row);
                xml.add_part(std::vector<uint8_t>(text.begin(), text.end()));
            } else {
                if (index != first) {
                    json += ",";
                }
                json += dicomweb::dataset_to_dicom_json(row);
            }
        }

        const auto service = base + "/dicomweb";
        for (const auto& flag : parameters.unsupported_flags) {
            res.add_header("Warning", "299 " + service + ": The " + flag + " parameter is not supported. Only literal matching has been performed.");
        }
        auto not_used = search.keys_not_used();
        not_used.insert(not_used.end(), parameters.keys_not_used.begin(), parameters.keys_not_used.end());
        if (!not_used.empty()) {
            std::string names;
            for (const auto& name : not_used) {
                names += (names.empty() ? "" : ",") + name;
            }
            res.add_header("Warning", "299 " + service + ": The following attributes were not used for matching: " + names);
        }
        if (remaining > 0) {
            res.add_header("Warning", "299 " + service + ": There are " + std::to_string(remaining) + " additional results that can be requested");
        }

        res.code = 200;
        if (choice.multipart) {
            res.set_header("Content-Type", xml.content_type_header());
            res.body = xml.empty() ? "--" + std::string(xml.boundary()) + "--\r\n" : xml.build();
        } else {
            res.set_header("Content-Type", std::string(dicomweb::media_type::dicom_json));
            res.body = json + "]";
        }
        return res;
    });
}

// ---- Pixel routes ------------------------------------------------------------------------------------

[[nodiscard]] crow::response frames_response(const std::shared_ptr<rest_server_context>& ctx, const crow::request& req, const std::string& study_uid,
                                             const std::string& series_uid, const std::string& sop_uid, const std::string& frame_list) {
    return guarded(ctx, req, read_scope, [&](crow::response res) {
        const auto numbers = requested_frames(frame_list);
        if (!numbers) {
            return failure(std::move(res), 400, "INVALID_FRAME_LIST", "Frames are positive numbers in ascending order, each once");
        }
        const auto instance = instance_under(*ctx->database, study_uid, series_uid, sop_uid);
        if (!instance) {
            return failure(std::move(res), 404, "NOT_FOUND", "Instance not found");
        }
        auto bytes = read_file_bytes(instance->file_path);
        if (bytes.empty()) {
            return failure(std::move(res), 500, "READ_ERROR", "An indexed object cannot be read");
        }
        auto file = core::dicom_file::from_bytes(std::span<const uint8_t>(bytes.data(), bytes.size()));
        if (file.is_err()) {
            return failure(std::move(res), 500, "PARSE_ERROR", "An indexed object cannot be parsed");
        }
        auto layout = dicomweb::layout_of(file.value().dataset());
        if (!layout) {
            return failure(std::move(res), 404, "NOT_FOUND", "The instance has no frames");
        }
        if (numbers->back() > layout->frames) {
            return failure(std::move(res), 404, "NOT_FOUND", "The instance has " + std::to_string(layout->frames) + " frames");
        }
        const auto held = file.value().transfer_syntax();
        const auto choice = dicomweb::choose_media_type(acceptable_types(req), dicomweb::wado_resource::frames, numbers->size(), held.uid());
        if (!choice.ok()) {
            return refusal(std::move(res), choice);
        }
        const bool as_held = choice.media != dicomweb::media_type::octet_stream;
        auto frames = as_held ? dicomweb::held_frames(file.value(), *layout, *numbers)
                              : dicomweb::native_frames(file.value(), *layout, *numbers);
        if (frames.is_err()) {
            return failure(std::move(res), as_held ? 500 : 406, "FRAME_ERROR", frames.error().message);
        }
        const auto part_type = choice.media + "; transfer-syntax=" +
                               (as_held ? std::string(held.uid()) : std::string(encoding::transfer_syntax::explicit_vr_little_endian.uid()));
        if (!choice.multipart) {
            res.code = 200;
            res.set_header("Content-Type", part_type);
            res.body.assign(reinterpret_cast<const char*>(frames.value().front().data()), frames.value().front().size());
            return res;
        }
        const auto base = base_url(req, *ctx);
        dicomweb::multipart_builder builder(choice.media);
        for (std::size_t index = 0; index < numbers->size(); ++index) {
            builder.add_part_with_location(std::move(frames.value()[index]),
                                           base + instance_path(study_uid, series_uid, sop_uid) + "/frames/" + std::to_string((*numbers)[index]),
                                           std::string_view(part_type));
        }
        res.code = 200;
        res.set_header("Content-Type", builder.content_type_header());
        res.body = builder.build();
        return res;
    });
}

[[nodiscard]] crow::response rendered_response(const std::shared_ptr<rest_server_context>& ctx, const crow::request& req, const std::string& study_uid,
                                               const std::string& series_uid, const std::string& sop_uid, std::optional<uint32_t> frame) {
    return guarded(ctx, req, read_scope, [&](crow::response res) {
        const auto instance = instance_under(*ctx->database, study_uid, series_uid, sop_uid);
        if (!instance) {
            return failure(std::move(res), 404, "NOT_FOUND", "Instance not found");
        }
        const auto choice = dicomweb::choose_media_type(acceptable_types(req), dicomweb::wado_resource::rendered);
        if (!choice.ok()) {
            return refusal(std::move(res), choice);
        }
        auto params = dicomweb::parse_rendered_params(req.raw_url, choice.media);
        if (frame) {
            params.frame = *frame;
        }
        auto result = dicomweb::render_dicom_image(instance->file_path, params);
        if (!result.success) {
            return failure(std::move(res), result.status == 0 ? 400 : result.status, "RENDER_ERROR", result.error_message);
        }
        res.code = 200;
        res.set_header("Content-Type", result.content_type);
        res.body.assign(reinterpret_cast<const char*>(result.data.data()), result.data.size());
        return res;
    });
}

[[nodiscard]] crow::response bulk_response(const std::shared_ptr<rest_server_context>& ctx, const crow::request& req, const std::string& study_uid,
                                           const std::string& series_uid, const std::string& sop_uid, const std::string& tag_text) {
    return guarded(ctx, req, read_scope, [&](crow::response res) {
        const auto instance = instance_under(*ctx->database, study_uid, series_uid, sop_uid);
        if (!instance) {
            return failure(std::move(res), 404, "NOT_FOUND", "Instance not found");
        }
        uint32_t combined = 0;
        const auto parsed = std::from_chars(tag_text.data(), tag_text.data() + tag_text.size(), combined, 16);
        if (tag_text.size() != 8 || parsed.ec != std::errc{} || parsed.ptr != tag_text.data() + tag_text.size()) {
            return failure(std::move(res), 404, "NOT_FOUND", "Not a Bulk Data URI of this server");
        }
        const auto choice = dicomweb::choose_media_type(acceptable_types(req), dicomweb::wado_resource::bulk_data);
        if (!choice.ok()) {
            return refusal(std::move(res), choice);
        }
        std::vector<uint8_t> data;
        if (combined == tags::pixel_data.combined()) {
            auto pixels = native_pixels_of(instance->file_path);
            if (pixels.status != 200) {
                return failure(std::move(res), pixels.status, "BULK_DATA_ERROR", pixels.message);
            }
            data = std::move(pixels.data);
        } else {
            auto file = core::dicom_file::open(instance->file_path);
            if (file.is_err()) {
                return failure(std::move(res), 500, "READ_ERROR", "An indexed object cannot be read");
            }
            const auto* element = file.value().dataset().get(core::dicom_tag{combined});
            if (element == nullptr || !dicomweb::is_bulk_data_tag(combined)) {
                return failure(std::move(res), 404, "NOT_FOUND", "This instance has no such Bulk Data");
            }
            const auto raw = element->raw_data();
            data.assign(raw.begin(), raw.end());
        }
        if (!choice.multipart) {
            res.code = 200;
            res.set_header("Content-Type", std::string(dicomweb::media_type::octet_stream));
            res.body.assign(reinterpret_cast<const char*>(data.data()), data.size());
            return res;
        }
        dicomweb::multipart_builder builder(dicomweb::media_type::octet_stream);
        builder.add_part_with_location(std::move(data), base_url(req, *ctx) + instance_path(study_uid, series_uid, sop_uid) + "/bulkdata/" + tag_text);
        res.code = 200;
        res.set_header("Content-Type", builder.content_type_header());
        res.body = builder.build();
        return res;
    });
}

}  // namespace

void register_dicomweb_endpoints_impl(rest_app& app,
                                      std::shared_ptr<rest_server_context> ctx) {
    // ========================================================================
    // WADO-RS: Instance and Metadata Resources
    // ========================================================================

    // GET /dicomweb/studies/{study}
    CROW_ROUTE(app, "/dicomweb/studies/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid) {
                return guarded(ctx, req, read_scope, [&](crow::response res) {
                    const auto choice = dicomweb::choose_media_type(acceptable_types(req), dicomweb::wado_resource::collection);
                    if (!choice.ok()) {
                        return refusal(std::move(res), choice);
                    }
                    auto files = ctx->database->get_study_files(study_uid);
                    if (files.is_err()) {
                        return failure(std::move(res), 500, "QUERY_ERROR", files.error().message);
                    }
                    return instances_response(std::move(res), files.value(), choice, base_url(req, *ctx));
                });
            });

    // GET /dicomweb/studies/{study}/metadata
    CROW_ROUTE(app, "/dicomweb/studies/<string>/metadata")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid) {
                return guarded(ctx, req, read_scope, [&](crow::response res) {
                    const auto choice = dicomweb::choose_media_type(acceptable_types(req), dicomweb::wado_resource::metadata);
                    if (!choice.ok()) {
                        return refusal(std::move(res), choice);
                    }
                    auto files = ctx->database->get_study_files(study_uid);
                    if (files.is_err()) {
                        return failure(std::move(res), 500, "QUERY_ERROR", files.error().message);
                    }
                    return metadata_response(std::move(res), files.value(), choice, base_url(req, *ctx));
                });
            });

    // GET /dicomweb/studies/{study}/series/{series}
    CROW_ROUTE(app, "/dicomweb/studies/<string>/series/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid, const std::string& series_uid) {
                return guarded(ctx, req, read_scope, [&](crow::response res) {
                    if (!series_under(*ctx->database, study_uid, series_uid)) {
                        return failure(std::move(res), 404, "NOT_FOUND", "Series not found");
                    }
                    const auto choice = dicomweb::choose_media_type(acceptable_types(req), dicomweb::wado_resource::collection);
                    if (!choice.ok()) {
                        return refusal(std::move(res), choice);
                    }
                    auto files = ctx->database->get_series_files(series_uid);
                    if (files.is_err()) {
                        return failure(std::move(res), 500, "QUERY_ERROR", files.error().message);
                    }
                    return instances_response(std::move(res), files.value(), choice, base_url(req, *ctx));
                });
            });

    // GET /dicomweb/studies/{study}/series/{series}/metadata
    CROW_ROUTE(app, "/dicomweb/studies/<string>/series/<string>/metadata")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid, const std::string& series_uid) {
                return guarded(ctx, req, read_scope, [&](crow::response res) {
                    if (!series_under(*ctx->database, study_uid, series_uid)) {
                        return failure(std::move(res), 404, "NOT_FOUND", "Series not found");
                    }
                    const auto choice = dicomweb::choose_media_type(acceptable_types(req), dicomweb::wado_resource::metadata);
                    if (!choice.ok()) {
                        return refusal(std::move(res), choice);
                    }
                    auto files = ctx->database->get_series_files(series_uid);
                    if (files.is_err()) {
                        return failure(std::move(res), 500, "QUERY_ERROR", files.error().message);
                    }
                    return metadata_response(std::move(res), files.value(), choice, base_url(req, *ctx));
                });
            });

    // GET /dicomweb/studies/{study}/series/{series}/instances/{instance}
    CROW_ROUTE(app, "/dicomweb/studies/<string>/series/<string>/instances/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid, const std::string& series_uid, const std::string& sop_uid) {
                return guarded(ctx, req, read_scope, [&](crow::response res) {
                    const auto instance = instance_under(*ctx->database, study_uid, series_uid, sop_uid);
                    if (!instance) {
                        return failure(std::move(res), 404, "NOT_FOUND", "Instance not found");
                    }
                    const auto choice = dicomweb::choose_media_type(acceptable_types(req), dicomweb::wado_resource::instance);
                    if (!choice.ok()) {
                        return refusal(std::move(res), choice);
                    }
                    return instances_response(std::move(res), {instance->file_path}, choice, base_url(req, *ctx));
                });
            });

    // GET /dicomweb/studies/{study}/series/{series}/instances/{instance}/metadata
    CROW_ROUTE(app, "/dicomweb/studies/<string>/series/<string>/instances/<string>/metadata")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid, const std::string& series_uid, const std::string& sop_uid) {
                return guarded(ctx, req, read_scope, [&](crow::response res) {
                    const auto instance = instance_under(*ctx->database, study_uid, series_uid, sop_uid);
                    if (!instance) {
                        return failure(std::move(res), 404, "NOT_FOUND", "Instance not found");
                    }
                    const auto choice = dicomweb::choose_media_type(acceptable_types(req), dicomweb::wado_resource::metadata);
                    if (!choice.ok()) {
                        return refusal(std::move(res), choice);
                    }
                    return metadata_response(std::move(res), {instance->file_path}, choice, base_url(req, *ctx));
                });
            });

    // ========================================================================
    // WADO-RS: Bulk Data, Frames and Rendered Resources
    // ========================================================================

    // GET /dicomweb/studies/{study}/series/{series}/instances/{instance}/bulkdata/{tag}
    CROW_ROUTE(app, "/dicomweb/studies/<string>/series/<string>/instances/<string>/bulkdata/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid, const std::string& series_uid, const std::string& sop_uid,
                  const std::string& tag_text) {
                return bulk_response(ctx, req, study_uid, series_uid, sop_uid, tag_text);
            });

    // GET /dicomweb/studies/{study}/series/{series}/instances/{instance}/frames/{frames}
    CROW_ROUTE(app, "/dicomweb/studies/<string>/series/<string>/instances/<string>/frames/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid, const std::string& series_uid, const std::string& sop_uid,
                  const std::string& frame_list) {
                return frames_response(ctx, req, study_uid, series_uid, sop_uid, frame_list);
            });

    // GET /dicomweb/studies/{study}/series/{series}/instances/{instance}/rendered
    CROW_ROUTE(app, "/dicomweb/studies/<string>/series/<string>/instances/<string>/rendered")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid, const std::string& series_uid, const std::string& sop_uid) {
                return rendered_response(ctx, req, study_uid, series_uid, sop_uid, std::nullopt);
            });

    // GET /dicomweb/studies/{study}/series/{series}/instances/{instance}/frames/{frame}/rendered
    CROW_ROUTE(app, "/dicomweb/studies/<string>/series/<string>/instances/<string>/frames/<string>/rendered")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid, const std::string& series_uid, const std::string& sop_uid,
                  const std::string& frame_text) {
                const auto numbers = requested_frames(frame_text);
                if (!numbers || numbers->size() != 1) {
                    // Auth first, as everywhere: a refusal here says nothing to someone without a token.
                    return guarded(ctx, req, read_scope, [](crow::response res) {
                        return failure(std::move(res), 400, "INVALID_FRAME", "A rendered frame is one positive frame number");
                    });
                }
                return rendered_response(ctx, req, study_uid, series_uid, sop_uid, numbers->front());
            });

    // ========================================================================
    // STOW-RS
    // ========================================================================

    // POST /dicomweb/studies
    CROW_ROUTE(app, "/dicomweb/studies")
        .methods(crow::HTTPMethod::POST)(
            [ctx](const crow::request& req) {
                return store_instances(ctx, req, std::nullopt);
            });

    // POST /dicomweb/studies/{study}
    CROW_ROUTE(app, "/dicomweb/studies/<string>")
        .methods(crow::HTTPMethod::POST)(
            [ctx](const crow::request& req, const std::string& target_study_uid) {
                return store_instances(ctx, req, target_study_uid);
            });

    // ========================================================================
    // QIDO-RS
    // ========================================================================

    CROW_ROUTE(app, "/dicomweb/studies")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req) {
                return search_response(ctx, req, {services::query_level::study, std::nullopt, std::nullopt});
            });

    CROW_ROUTE(app, "/dicomweb/series")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req) {
                return search_response(ctx, req, {services::query_level::series, std::nullopt, std::nullopt});
            });

    CROW_ROUTE(app, "/dicomweb/instances")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req) {
                return search_response(ctx, req, {services::query_level::image, std::nullopt, std::nullopt});
            });

    CROW_ROUTE(app, "/dicomweb/studies/<string>/series")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid) {
                return search_response(ctx, req, {services::query_level::series, study_uid, std::nullopt});
            });

    CROW_ROUTE(app, "/dicomweb/studies/<string>/instances")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid) {
                return search_response(ctx, req, {services::query_level::image, study_uid, std::nullopt});
            });

    CROW_ROUTE(app, "/dicomweb/studies/<string>/series/<string>/instances")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid, const std::string& series_uid) {
                return search_response(ctx, req, {services::query_level::image, study_uid, series_uid});
            });

    // ========================================================================
    // CORS Preflight Handler for DICOMweb
    // ========================================================================

    CROW_ROUTE(app, "/dicomweb/<path>")
        .methods(crow::HTTPMethod::OPTIONS)(
            [ctx](const crow::request& /*req*/, const std::string& /*path*/) {
                crow::response res(204);
                add_cors_headers(res, *ctx);
                res.add_header("Access-Control-Allow-Methods",
                               "GET, POST, OPTIONS");
                res.add_header("Access-Control-Allow-Headers",
                               "Content-Type, Accept, Authorization");
                res.add_header("Access-Control-Max-Age", "86400");
                return res;
            });
}

} // namespace kcenon::pacs::web::endpoints
