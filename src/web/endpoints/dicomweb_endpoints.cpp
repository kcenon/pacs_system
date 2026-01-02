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

// Workaround for Windows: DELETE is defined as a macro in <winnt.h>
// which conflicts with crow::HTTPMethod::DELETE
#ifdef DELETE
#undef DELETE
#endif

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_element.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/compression/jpeg_baseline_codec.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/storage/index_database.hpp"
#include "pacs/storage/instance_record.hpp"
#include "pacs/storage/patient_record.hpp"
#include "pacs/storage/series_record.hpp"
#include "pacs/storage/study_record.hpp"
#include "pacs/web/endpoints/dicomweb_endpoints.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

namespace pacs::web::dicomweb {

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
std::string vr_enum_to_string(pacs::encoding::vr_type vr) {
    using vr_type = pacs::encoding::vr_type;
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

        for (size_t i = 1; i < params.size(); ++i) {
            auto param = trim(params[i]);
            if (param.starts_with("q=")) {
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

    // Sort by quality (descending)
    std::sort(result.begin(), result.end(),
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
        << "\"; boundary=" << boundary_;
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
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999999);

    std::ostringstream oss;
    oss << "----=_Part_" << timestamp << "_" << dis(gen);
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
    if (!boundary_opt) {
        result.error = parse_error{
            "INVALID_BOUNDARY",
            "Missing or invalid boundary in Content-Type header"};
        return result;
    }

    const std::string& boundary = *boundary_opt;
    std::string delimiter = "--" + boundary;
    std::string end_delimiter = "--" + boundary + "--";

    // Find first boundary
    auto pos = body.find(delimiter);
    if (pos == std::string_view::npos) {
        result.error = parse_error{
            "NO_PARTS",
            "No parts found in multipart body"};
        return result;
    }

    // Skip to after first boundary and CRLF
    pos += delimiter.size();
    if (pos < body.size() && body.substr(pos, 2) == "\r\n") {
        pos += 2;
    }

    // Parse each part
    while (pos < body.size()) {
        // Check for end delimiter
        if (body.substr(pos).starts_with("--")) {
            break; // End of multipart
        }

        // Find next boundary
        auto next_boundary = body.find(delimiter, pos);
        if (next_boundary == std::string_view::npos) {
            // No more parts
            break;
        }

        // Extract part content (excluding trailing CRLF before boundary)
        auto part_content = body.substr(pos, next_boundary - pos);
        if (part_content.size() >= 2 &&
            part_content.substr(part_content.size() - 2) == "\r\n") {
            part_content = part_content.substr(0, part_content.size() - 2);
        }

        // Split part into headers and body
        auto header_end = part_content.find("\r\n\r\n");
        if (header_end == std::string_view::npos) {
            // Malformed part - skip
            pos = next_boundary + delimiter.size();
            if (pos < body.size() && body.substr(pos, 2) == "\r\n") {
                pos += 2;
            }
            continue;
        }

        auto header_section = part_content.substr(0, header_end);
        auto body_section = part_content.substr(header_end + 4);

        // Parse headers
        auto headers = parse_part_headers(header_section);

        // Create multipart_part
        multipart_part part;
        for (const auto& [name, value] : headers) {
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

        // Copy body data
        part.data.assign(
            reinterpret_cast<const uint8_t*>(body_section.data()),
            reinterpret_cast<const uint8_t*>(body_section.data() +
                                             body_section.size()));

        result.parts.push_back(std::move(part));

        // Move to next part
        pos = next_boundary + delimiter.size();
        if (pos < body.size() && body.substr(pos, 2) == "\r\n") {
            pos += 2;
        }
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
            // Referenced SOP Class UID (0008,1150)
            oss << "\"00081150\":{\"vr\":\"UI\",\"Value\":[\""
                << inst.sop_class_uid << "\"]},";
            // Referenced SOP Instance UID (0008,1155)
            oss << "\"00081155\":{\"vr\":\"UI\",\"Value\":[\""
                << inst.sop_instance_uid << "\"]},";
            // Retrieve URL (0008,1190)
            oss << "\"00081190\":{\"vr\":\"UR\",\"Value\":[\""
                << base_url << inst.retrieve_url << "\"]}";
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
            // Referenced SOP Class UID (0008,1150)
            if (!inst.sop_class_uid.empty()) {
                oss << "\"00081150\":{\"vr\":\"UI\",\"Value\":[\""
                    << inst.sop_class_uid << "\"]},";
            }
            // Referenced SOP Instance UID (0008,1155)
            if (!inst.sop_instance_uid.empty()) {
                oss << "\"00081155\":{\"vr\":\"UI\",\"Value\":[\""
                    << inst.sop_instance_uid << "\"]},";
            }
            // Failure Reason (0008,1197)
            uint16_t failure_reason = 272; // Processing failure
            if (inst.error_code && *inst.error_code == "DUPLICATE") {
                failure_reason = 273; // Duplicate SOP Instance
            } else if (inst.error_code && *inst.error_code == "INVALID_DATA") {
                failure_reason = 272; // Processing failure
            }
            oss << "\"00081197\":{\"vr\":\"US\",\"Value\":["
                << failure_reason << "]}";
            oss << "}";
        }
        oss << "]}";
    }

    oss << "}";
    return oss.str();
}

auto dataset_to_dicom_json(
    const core::dicom_dataset& dataset,
    bool include_bulk_data,
    std::string_view bulk_data_uri_prefix) -> std::string {
    std::ostringstream oss;
    oss << "{";

    bool first = true;
    // Iterate using begin()/end() - dataset is iterable as map<dicom_tag, dicom_element>
    for (const auto& [tag_key, elem] : dataset) {
        if (!first) {
            oss << ",";
        }
        first = false;

        // Format tag as 8-digit hex
        uint32_t tag = tag_key.combined();
        oss << "\"";
        oss << std::hex << std::setfill('0') << std::setw(8) << tag;
        oss << std::dec << "\":{";

        // Add VR
        std::string vr_str = vr_enum_to_string(elem.vr());
        oss << "\"vr\":\"" << vr_str << "\"";

        // Check if bulk data
        if (is_bulk_data_tag(tag) && !include_bulk_data) {
            if (!bulk_data_uri_prefix.empty()) {
                oss << ",\"BulkDataURI\":\"" << bulk_data_uri_prefix
                    << std::hex << std::setfill('0') << std::setw(8) << tag
                    << std::dec << "\"";
            }
        } else {
            // Add Value based on VR type
            auto value_str = elem.as_string().unwrap_or("");
            if (!value_str.empty()) {
                oss << ",\"Value\":[";

                // Handle different VR types
                using vr_type = pacs::encoding::vr_type;
                switch (elem.vr()) {
                case vr_type::PN:
                    // Person Name - object with Alphabetic component
                    oss << "{\"Alphabetic\":\"" << json_escape(value_str) << "\"}";
                    break;

                case vr_type::IS:
                case vr_type::SL:
                case vr_type::SS:
                case vr_type::UL:
                case vr_type::US:
                    // Integer types - output as number
                    oss << value_str;
                    break;

                case vr_type::DS:
                case vr_type::FL:
                case vr_type::FD:
                    // Decimal types - output as number
                    oss << value_str;
                    break;

                default:
                    // String types
                    oss << "\"" << json_escape(value_str) << "\"";
                    break;
                }

                oss << "]";
            }
        }

        oss << "}";
    }

    oss << "}";
    return oss.str();
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
        if (str[i] == '%' && i + 2 < str.size()) {
            int value = 0;
            std::string hex_str(str.substr(i + 1, 2));
            try {
                value = std::stoi(hex_str, nullptr, 16);
                result += static_cast<char>(value);
                i += 2;
            } catch (...) {
                result += str[i];
            }
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

    // Remove leading '?' if present
    std::string_view qs = query_string;
    if (!qs.empty() && qs[0] == '?') {
        qs = qs.substr(1);
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
// Rendered Images
// ============================================================================

auto parse_rendered_params(
    std::string_view query_string,
    std::string_view accept_header) -> rendered_params {

    rendered_params params;

    // Determine format from Accept header
    if (accept_header.find("image/png") != std::string_view::npos) {
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
        } else if (key == "windowcenter" || key == "window-center") {
            try {
                params.window_center = std::stod(value);
            } catch (...) {}
        } else if (key == "windowwidth" || key == "window-width") {
            try {
                params.window_width = std::stod(value);
            } catch (...) {}
        } else if (key == "viewport") {
            // Format: WxH or W,H
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

    // Calculate window boundaries
    double window_min = window_center - window_width / 2.0;
    double window_max = window_center + window_width / 2.0;

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

        // Apply window/level
        uint8_t output_value;
        if (value <= window_min) {
            output_value = 0;
        } else if (value >= window_max) {
            output_value = 255;
        } else {
            output_value = static_cast<uint8_t>(
                (value - window_min) / window_width * 255.0);
        }

        output[i] = output_value;
    }

    return output;
}

auto render_dicom_image(
    std::string_view file_path,
    const rendered_params& params) -> rendered_result {

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

    // Get image parameters
    auto rows_elem = dataset.get(core::tags::rows);
    auto cols_elem = dataset.get(core::tags::columns);
    auto bits_stored_elem = dataset.get(core::tags::bits_stored);
    auto bits_allocated_elem = dataset.get(core::tags::bits_allocated);
    auto pixel_rep_elem = dataset.get(core::tags::pixel_representation);
    auto samples_elem = dataset.get(core::tags::samples_per_pixel);
    auto pixel_data_elem = dataset.get(core::tags::pixel_data);

    if (!rows_elem || !cols_elem || !pixel_data_elem) {
        return rendered_result::error("Missing required image attributes");
    }

    uint16_t rows = rows_elem->as_numeric<uint16_t>().unwrap_or(0);
    uint16_t cols = cols_elem->as_numeric<uint16_t>().unwrap_or(0);
    uint16_t bits_stored = bits_stored_elem ?
        bits_stored_elem->as_numeric<uint16_t>().unwrap_or(8) : 8;
    uint16_t bits_allocated = bits_allocated_elem ?
        bits_allocated_elem->as_numeric<uint16_t>().unwrap_or(8) : 8;
    uint16_t pixel_rep = pixel_rep_elem ?
        pixel_rep_elem->as_numeric<uint16_t>().unwrap_or(0) : 0;
    uint16_t samples_per_pixel = samples_elem ?
        samples_elem->as_numeric<uint16_t>().unwrap_or(1) : 1;

    bool is_signed = (pixel_rep == 1);

    // Get window/level from DICOM or use provided parameters
    double window_center = 128.0;
    double window_width = 256.0;

    if (params.window_center.has_value()) {
        window_center = *params.window_center;
    } else if (auto wc = dataset.get(core::tags::window_center)) {
        try {
            auto values_result = wc->as_string_list();
            if (values_result.is_ok() && !values_result.value().empty()) {
                window_center = std::stod(values_result.value()[0]);
            }
        } catch (...) {}
    }

    if (params.window_width.has_value()) {
        window_width = *params.window_width;
    } else if (auto ww = dataset.get(core::tags::window_width)) {
        try {
            auto values_result = ww->as_string_list();
            if (values_result.is_ok() && !values_result.value().empty()) {
                window_width = std::stod(values_result.value()[0]);
            }
        } catch (...) {}
    }

    // Get rescale parameters
    double rescale_slope = 1.0;
    double rescale_intercept = 0.0;

    if (auto rs = dataset.get(core::tags::rescale_slope)) {
        try {
            rescale_slope = std::stod(rs->as_string().unwrap_or("1.0"));
        } catch (...) {}
    }
    if (auto ri = dataset.get(core::tags::rescale_intercept)) {
        try {
            rescale_intercept = std::stod(ri->as_string().unwrap_or("0.0"));
        } catch (...) {}
    }

    // Get pixel data
    auto pixel_data = pixel_data_elem->raw_data();

    // Calculate frame size for multi-frame images
    size_t frame_size = static_cast<size_t>(rows) * cols * samples_per_pixel *
                        ((bits_allocated + 7) / 8);

    // Extract specific frame if multi-frame
    std::span<const uint8_t> frame_data = pixel_data;
    if (params.frame > 1) {
        auto extracted = extract_frame(pixel_data, params.frame, frame_size);
        if (extracted.empty()) {
            return rendered_result::error("Requested frame does not exist");
        }
        // For simplicity, we'll just use first frame for now
        // Full implementation would handle multi-frame properly
    }

    // Apply window/level for grayscale images
    std::vector<uint8_t> output_pixels;
    if (samples_per_pixel == 1) {
        output_pixels = apply_window_level(
            frame_data, cols, rows, bits_stored, is_signed,
            window_center, window_width, rescale_slope, rescale_intercept);
    } else {
        // For color images, just use raw data (convert to 8-bit if needed)
        output_pixels.resize(static_cast<size_t>(rows) * cols * samples_per_pixel);
        if (bits_allocated == 8) {
            std::copy(frame_data.begin(),
                      frame_data.begin() + (std::min)(frame_data.size(),
                                                     output_pixels.size()),
                      output_pixels.begin());
        } else {
            // Convert 16-bit to 8-bit
            for (size_t i = 0; i < output_pixels.size() && i * 2 + 1 < frame_data.size(); ++i) {
                uint16_t val = static_cast<uint16_t>(
                    frame_data[i * 2] | (frame_data[i * 2 + 1] << 8));
                output_pixels[i] = static_cast<uint8_t>(val >> (bits_stored - 8));
            }
        }
    }

    // Encode to JPEG or PNG
    if (params.format == rendered_format::jpeg) {
        encoding::compression::jpeg_baseline_codec codec;
        encoding::compression::image_params img_params;
        img_params.width = cols;
        img_params.height = rows;
        img_params.bits_allocated = 8;
        img_params.bits_stored = 8;
        img_params.high_bit = 7;
        img_params.samples_per_pixel = samples_per_pixel;
        img_params.photometric =
            (samples_per_pixel == 1) ?
            encoding::compression::photometric_interpretation::monochrome2 :
            encoding::compression::photometric_interpretation::rgb;

        encoding::compression::compression_options opts;
        opts.quality = params.quality;

        auto result = codec.encode(output_pixels, img_params, opts);
        if (result.is_err()) {
            return rendered_result::error("JPEG encoding failed: " +
                                          pacs::get_error(result).message);
        }

        return rendered_result::ok(std::move(pacs::get_value(result).data), media_type::jpeg);
    } else {
        // PNG encoding - not implemented yet
        // For now, return JPEG as fallback
        return rendered_result::error("PNG encoding not yet implemented");
    }
}

} // namespace pacs::web::dicomweb

namespace pacs::web::endpoints {

namespace {

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

/**
 * @brief Build retrieval response for multiple DICOM files
 */
crow::response build_multipart_dicom_response(
    const std::vector<std::string>& file_paths,
    const rest_server_context& ctx,
    std::string_view base_uri = "") {

    crow::response res;
    add_cors_headers(res, ctx);

    if (file_paths.empty()) {
        res.code = 404;
        res.add_header("Content-Type", "application/json");
        res.body = make_error_json("NOT_FOUND", "No instances found");
        return res;
    }

    // Single file - return directly
    if (file_paths.size() == 1) {
        auto data = read_file_bytes(file_paths[0]);
        if (data.empty()) {
            res.code = 500;
            res.add_header("Content-Type", "application/json");
            res.body = make_error_json("READ_ERROR", "Failed to read DICOM file");
            return res;
        }

        res.code = 200;
        res.add_header("Content-Type", std::string(dicomweb::media_type::dicom));
        res.body = std::string(reinterpret_cast<char*>(data.data()), data.size());
        return res;
    }

    // Multiple files - use multipart response
    dicomweb::multipart_builder builder(dicomweb::media_type::dicom);

    for (size_t i = 0; i < file_paths.size(); ++i) {
        auto data = read_file_bytes(file_paths[i]);
        if (data.empty()) {
            continue; // Skip files that can't be read
        }

        if (base_uri.empty()) {
            builder.add_part(std::move(data));
        } else {
            std::ostringstream location;
            location << base_uri << "/" << i;
            builder.add_part_with_location(std::move(data), location.str());
        }
    }

    if (builder.empty()) {
        res.code = 500;
        res.add_header("Content-Type", "application/json");
        res.body = make_error_json("READ_ERROR", "Failed to read DICOM files");
        return res;
    }

    res.code = 200;
    res.add_header("Content-Type", builder.content_type_header());
    res.body = builder.build();
    return res;
}

/**
 * @brief Build metadata response (DicomJSON)
 */
crow::response build_metadata_response(
    const std::vector<std::string>& file_paths,
    const rest_server_context& ctx,
    std::string_view bulk_data_uri_prefix = "") {

    crow::response res;
    add_cors_headers(res, ctx);
    res.add_header("Content-Type", std::string(dicomweb::media_type::dicom_json));

    if (file_paths.empty()) {
        res.code = 404;
        res.body = make_error_json("NOT_FOUND", "No instances found");
        return res;
    }

    std::ostringstream oss;
    oss << "[";

    bool first = true;
    for (const auto& path : file_paths) {
        auto result = core::dicom_file::open(path);
        if (result.is_err()) {
            continue;
        }

        if (!first) {
            oss << ",";
        }
        first = false;

        oss << dicomweb::dataset_to_dicom_json(
            result.value().dataset(), false, bulk_data_uri_prefix);
    }

    oss << "]";

    res.code = 200;
    res.body = oss.str();
    return res;
}

} // anonymous namespace

void register_dicomweb_endpoints_impl(crow::SimpleApp& app,
                                      std::shared_ptr<rest_server_context> ctx) {
    // ========================================================================
    // Study Retrieval
    // ========================================================================

    // GET /dicomweb/studies/{studyUID} - Retrieve entire study
    CROW_ROUTE(app, "/dicomweb/studies/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                // Check Accept header
                auto accept = req.get_header_value("Accept");
                auto accept_infos = dicomweb::parse_accept_header(accept);

                // Check if metadata is requested
                if (dicomweb::is_acceptable(accept_infos,
                                            dicomweb::media_type::dicom_json)) {
                    auto files_result = ctx->database->get_study_files(study_uid);
                    if (!files_result.is_ok()) {
                        res.code = 500;
                        res.add_header("Content-Type", "application/json");
                        res.body = make_error_json("QUERY_ERROR",
                                                   files_result.error().message);
                        return res;
                    }
                    std::string bulk_uri = "/dicomweb/studies/" + study_uid +
                                           "/bulkdata/";
                    return build_metadata_response(files_result.value(), *ctx, bulk_uri);
                }

                // Return DICOM instances
                auto files_result = ctx->database->get_study_files(study_uid);
                if (!files_result.is_ok()) {
                    res.code = 500;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("QUERY_ERROR",
                                               files_result.error().message);
                    return res;
                }
                std::string base_uri = "/dicomweb/studies/" + study_uid;
                return build_multipart_dicom_response(files_result.value(), *ctx, base_uri);
            });

    // GET /dicomweb/studies/{studyUID}/metadata - Study metadata
    CROW_ROUTE(app, "/dicomweb/studies/<string>/metadata")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& /*req*/, const std::string& study_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                auto files_result = ctx->database->get_study_files(study_uid);
                if (!files_result.is_ok()) {
                    res.code = 500;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("QUERY_ERROR",
                                               files_result.error().message);
                    return res;
                }
                std::string bulk_uri = "/dicomweb/studies/" + study_uid +
                                       "/bulkdata/";
                return build_metadata_response(files_result.value(), *ctx, bulk_uri);
            });

    // ========================================================================
    // Series Retrieval
    // ========================================================================

    // GET /dicomweb/studies/{studyUID}/series/{seriesUID} - Retrieve series
    CROW_ROUTE(app, "/dicomweb/studies/<string>/series/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req,
                  const std::string& study_uid,
                  const std::string& series_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                // Verify study exists
                auto study = ctx->database->find_study(study_uid);
                if (!study) {
                    res.code = 404;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("NOT_FOUND", "Study not found");
                    return res;
                }

                // Check Accept header
                auto accept = req.get_header_value("Accept");
                auto accept_infos = dicomweb::parse_accept_header(accept);

                // Check if metadata is requested
                if (dicomweb::is_acceptable(accept_infos,
                                            dicomweb::media_type::dicom_json)) {
                    auto files_result = ctx->database->get_series_files(series_uid);
                    if (!files_result.is_ok()) {
                        res.code = 500;
                        res.add_header("Content-Type", "application/json");
                        res.body = make_error_json("QUERY_ERROR",
                                                   files_result.error().message);
                        return res;
                    }
                    std::string bulk_uri = "/dicomweb/studies/" + study_uid +
                                           "/series/" + series_uid + "/bulkdata/";
                    return build_metadata_response(files_result.value(), *ctx, bulk_uri);
                }

                auto files_result = ctx->database->get_series_files(series_uid);
                if (!files_result.is_ok()) {
                    res.code = 500;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("QUERY_ERROR",
                                               files_result.error().message);
                    return res;
                }
                std::string base_uri = "/dicomweb/studies/" + study_uid +
                                       "/series/" + series_uid;
                return build_multipart_dicom_response(files_result.value(), *ctx, base_uri);
            });

    // GET /dicomweb/studies/{studyUID}/series/{seriesUID}/metadata
    CROW_ROUTE(app, "/dicomweb/studies/<string>/series/<string>/metadata")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& /*req*/,
                  const std::string& study_uid,
                  const std::string& series_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                auto files_result = ctx->database->get_series_files(series_uid);
                if (!files_result.is_ok()) {
                    res.code = 500;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("QUERY_ERROR",
                                               files_result.error().message);
                    return res;
                }
                std::string bulk_uri = "/dicomweb/studies/" + study_uid +
                                       "/series/" + series_uid + "/bulkdata/";
                return build_metadata_response(files_result.value(), *ctx, bulk_uri);
            });

    // ========================================================================
    // Instance Retrieval
    // ========================================================================

    // GET /dicomweb/studies/{studyUID}/series/{seriesUID}/instances/{sopUID}
    CROW_ROUTE(app,
               "/dicomweb/studies/<string>/series/<string>/instances/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req,
                  const std::string& study_uid,
                  const std::string& series_uid,
                  const std::string& sop_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                auto file_path_result = ctx->database->get_file_path(sop_uid);
                if (!file_path_result.is_ok()) {
                    res.code = 500;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("QUERY_ERROR",
                                               file_path_result.error().message);
                    return res;
                }
                const auto& file_path = file_path_result.value();
                if (!file_path) {
                    res.code = 404;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("NOT_FOUND", "Instance not found");
                    return res;
                }

                // Check Accept header
                auto accept = req.get_header_value("Accept");
                auto accept_infos = dicomweb::parse_accept_header(accept);

                // Check if metadata is requested
                if (dicomweb::is_acceptable(accept_infos,
                                            dicomweb::media_type::dicom_json)) {
                    std::vector<std::string> files = {*file_path};
                    std::string bulk_uri = "/dicomweb/studies/" + study_uid +
                                           "/series/" + series_uid +
                                           "/instances/" + sop_uid + "/bulkdata/";
                    return build_metadata_response(files, *ctx, bulk_uri);
                }

                // Return DICOM instance
                auto data = read_file_bytes(*file_path);
                if (data.empty()) {
                    res.code = 500;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("READ_ERROR",
                                               "Failed to read DICOM file");
                    return res;
                }

                res.code = 200;
                res.add_header("Content-Type",
                               std::string(dicomweb::media_type::dicom));
                res.body = std::string(reinterpret_cast<char*>(data.data()),
                                       data.size());
                return res;
            });

    // GET /dicomweb/studies/{studyUID}/series/{seriesUID}/instances/{sopUID}/metadata
    CROW_ROUTE(app,
               "/dicomweb/studies/<string>/series/<string>/instances/<string>/metadata")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& /*req*/,
                  const std::string& study_uid,
                  const std::string& series_uid,
                  const std::string& sop_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                auto file_path_result = ctx->database->get_file_path(sop_uid);
                if (!file_path_result.is_ok()) {
                    res.code = 500;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("QUERY_ERROR",
                                               file_path_result.error().message);
                    return res;
                }
                const auto& file_path = file_path_result.value();
                if (!file_path) {
                    res.code = 404;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("NOT_FOUND", "Instance not found");
                    return res;
                }

                std::vector<std::string> files = {*file_path};
                std::string bulk_uri = "/dicomweb/studies/" + study_uid +
                                       "/series/" + series_uid +
                                       "/instances/" + sop_uid + "/bulkdata/";
                return build_metadata_response(files, *ctx, bulk_uri);
            });

    // ========================================================================
    // Frame Retrieval (WADO-RS)
    // ========================================================================

    // GET /dicomweb/studies/{studyUID}/series/{seriesUID}/instances/{sopUID}/frames/{frameNumbers}
    CROW_ROUTE(app,
               "/dicomweb/studies/<string>/series/<string>/instances/<string>/frames/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req,
                  const std::string& study_uid,
                  const std::string& series_uid,
                  const std::string& sop_uid,
                  const std::string& frame_list) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                auto file_path_result = ctx->database->get_file_path(sop_uid);
                if (!file_path_result.is_ok()) {
                    res.code = 500;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("QUERY_ERROR",
                                               file_path_result.error().message);
                    return res;
                }
                const auto& file_path = file_path_result.value();
                if (!file_path) {
                    res.code = 404;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("NOT_FOUND", "Instance not found");
                    return res;
                }

                // Parse frame numbers
                auto frames = dicomweb::parse_frame_numbers(frame_list);
                if (frames.empty()) {
                    res.code = 400;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("INVALID_FRAME_LIST",
                                               "No valid frame numbers specified");
                    return res;
                }

                // Load DICOM file
                auto data = read_file_bytes(*file_path);
                if (data.empty()) {
                    res.code = 500;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("READ_ERROR",
                                               "Failed to read DICOM file");
                    return res;
                }

                auto dicom_result = core::dicom_file::from_bytes(
                    std::span<const uint8_t>(data.data(), data.size()));
                if (dicom_result.is_err()) {
                    res.code = 500;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("PARSE_ERROR",
                                               "Failed to parse DICOM file");
                    return res;
                }

                const auto& dataset = dicom_result.value().dataset();

                // Get image parameters
                auto rows_elem = dataset.get(core::tags::rows);
                auto cols_elem = dataset.get(core::tags::columns);
                auto bits_alloc_elem = dataset.get(core::tags::bits_allocated);
                auto samples_elem = dataset.get(core::tags::samples_per_pixel);
                // Number of Frames (0028,0008)
                constexpr core::dicom_tag number_of_frames_tag{0x0028, 0x0008};
                auto num_frames_elem = dataset.get(number_of_frames_tag);
                auto pixel_data_elem = dataset.get(core::tags::pixel_data);

                if (!rows_elem || !cols_elem || !pixel_data_elem) {
                    res.code = 400;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("NOT_IMAGE",
                                               "Instance does not contain image data");
                    return res;
                }

                uint16_t rows = rows_elem->as_numeric<uint16_t>().unwrap_or(0);
                uint16_t cols = cols_elem->as_numeric<uint16_t>().unwrap_or(0);
                uint16_t bits_allocated = bits_alloc_elem ?
                    bits_alloc_elem->as_numeric<uint16_t>().unwrap_or(16) : 16;
                uint16_t samples_per_pixel = samples_elem ?
                    samples_elem->as_numeric<uint16_t>().unwrap_or(1) : 1;
                uint32_t num_frames = 1;
                if (num_frames_elem) {
                    try {
                        num_frames = std::stoul(num_frames_elem->as_string().unwrap_or("1"));
                    } catch (...) {}
                }

                // Calculate frame size
                size_t frame_size = static_cast<size_t>(rows) * cols *
                                    samples_per_pixel * ((bits_allocated + 7) / 8);

                auto pixel_data = pixel_data_elem->raw_data();

                // Check Accept header
                auto accept = req.get_header_value("Accept");

                // Build multipart response for multiple frames
                dicomweb::multipart_builder builder(
                    dicomweb::media_type::octet_stream);

                for (uint32_t frame_num : frames) {
                    if (frame_num > num_frames) {
                        // Skip invalid frame numbers
                        continue;
                    }

                    auto frame_data = dicomweb::extract_frame(
                        pixel_data, frame_num, frame_size);

                    if (!frame_data.empty()) {
                        std::string location = "/dicomweb/studies/" + study_uid +
                                               "/series/" + series_uid +
                                               "/instances/" + sop_uid +
                                               "/frames/" + std::to_string(frame_num);
                        builder.add_part_with_location(std::move(frame_data), location);
                    }
                }

                if (builder.empty()) {
                    res.code = 404;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("NOT_FOUND",
                                               "No valid frames found");
                    return res;
                }

                // Return single part or multipart
                if (builder.size() == 1) {
                    // Single frame - return directly
                    auto body = builder.build();
                    res.code = 200;
                    res.add_header("Content-Type",
                                   std::string(dicomweb::media_type::octet_stream));
                    // Extract just the data part from multipart
                    auto frame_data = dicomweb::extract_frame(
                        pixel_data, frames[0], frame_size);
                    res.body = std::string(
                        reinterpret_cast<char*>(frame_data.data()),
                        frame_data.size());
                } else {
                    // Multiple frames - return multipart
                    res.code = 200;
                    res.add_header("Content-Type", builder.content_type_header());
                    res.body = builder.build();
                }

                return res;
            });

    // ========================================================================
    // Rendered Images (WADO-RS)
    // ========================================================================

    // GET /dicomweb/studies/{studyUID}/series/{seriesUID}/instances/{sopUID}/rendered
    CROW_ROUTE(app,
               "/dicomweb/studies/<string>/series/<string>/instances/<string>/rendered")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req,
                  const std::string& study_uid,
                  const std::string& series_uid,
                  const std::string& sop_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                auto file_path_result = ctx->database->get_file_path(sop_uid);
                if (!file_path_result.is_ok()) {
                    res.code = 500;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("QUERY_ERROR",
                                               file_path_result.error().message);
                    return res;
                }
                const auto& file_path = file_path_result.value();
                if (!file_path) {
                    res.code = 404;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("NOT_FOUND", "Instance not found");
                    return res;
                }

                // Parse rendering parameters
                auto accept = req.get_header_value("Accept");
                auto params = dicomweb::parse_rendered_params(req.raw_url, accept);

                // Render image
                auto result = dicomweb::render_dicom_image(*file_path, params);

                if (!result.success) {
                    res.code = 400;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("RENDER_ERROR", result.error_message);
                    return res;
                }

                res.code = 200;
                res.add_header("Content-Type", result.content_type);
                res.body = std::string(
                    reinterpret_cast<char*>(result.data.data()),
                    result.data.size());
                return res;
            });

    // GET /dicomweb/studies/{studyUID}/series/{seriesUID}/instances/{sopUID}/frames/{frameNumber}/rendered
    CROW_ROUTE(app,
               "/dicomweb/studies/<string>/series/<string>/instances/<string>/frames/<string>/rendered")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req,
                  const std::string& study_uid,
                  const std::string& series_uid,
                  const std::string& sop_uid,
                  const std::string& frame_str) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                auto file_path_result = ctx->database->get_file_path(sop_uid);
                if (!file_path_result.is_ok()) {
                    res.code = 500;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("QUERY_ERROR",
                                               file_path_result.error().message);
                    return res;
                }
                const auto& file_path = file_path_result.value();
                if (!file_path) {
                    res.code = 404;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("NOT_FOUND", "Instance not found");
                    return res;
                }

                // Parse frame number
                uint32_t frame_num = 1;
                try {
                    frame_num = std::stoul(frame_str);
                    if (frame_num == 0) frame_num = 1;
                } catch (...) {
                    res.code = 400;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("INVALID_FRAME",
                                               "Invalid frame number");
                    return res;
                }

                // Parse rendering parameters and set frame
                auto accept = req.get_header_value("Accept");
                auto params = dicomweb::parse_rendered_params(req.raw_url, accept);
                params.frame = frame_num;

                // Render image
                auto result = dicomweb::render_dicom_image(*file_path, params);

                if (!result.success) {
                    res.code = 400;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("RENDER_ERROR", result.error_message);
                    return res;
                }

                res.code = 200;
                res.add_header("Content-Type", result.content_type);
                res.body = std::string(
                    reinterpret_cast<char*>(result.data.data()),
                    result.data.size());
                return res;
            });

    // ========================================================================
    // STOW-RS (Store Over the Web)
    // ========================================================================

    // POST /dicomweb/studies - Store instances (new study)
    CROW_ROUTE(app, "/dicomweb/studies")
        .methods(crow::HTTPMethod::POST)(
            [ctx](const crow::request& req) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                // Parse Content-Type header
                auto content_type = req.get_header_value("Content-Type");
                if (content_type.empty() ||
                    content_type.find("multipart/related") == std::string::npos) {
                    res.code = 415; // Unsupported Media Type
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json(
                        "UNSUPPORTED_MEDIA_TYPE",
                        "Content-Type must be multipart/related");
                    return res;
                }

                // Parse multipart body
                auto parse_result = dicomweb::multipart_parser::parse(
                    content_type, req.body);

                if (!parse_result) {
                    res.code = 400;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json(
                        parse_result.error->code,
                        parse_result.error->message);
                    return res;
                }

                if (parse_result.parts.empty()) {
                    res.code = 400;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json(
                        "NO_INSTANCES",
                        "No DICOM instances in request body");
                    return res;
                }

                // Process each part
                dicomweb::store_response store_response;

                for (const auto& part : parse_result.parts) {
                    dicomweb::store_instance_result result;

                    // Only process application/dicom parts
                    if (part.content_type.find("application/dicom") ==
                        std::string::npos) {
                        continue;
                    }

                    // Parse DICOM from memory
                    auto dicom_result = core::dicom_file::from_bytes(
                        std::span<const uint8_t>(part.data.data(), part.data.size()));

                    if (dicom_result.is_err()) {
                        result.success = false;
                        result.error_code = "INVALID_DATA";
                        result.error_message = "Failed to parse DICOM data";
                        store_response.failed_instances.push_back(
                            std::move(result));
                        continue;
                    }

                    const auto& dataset = dicom_result.value().dataset();

                    // Validate instance
                    auto validation = dicomweb::validate_instance(dataset);
                    if (!validation) {
                        result.success = false;
                        result.error_code = validation.error_code;
                        result.error_message = validation.error_message;

                        // Try to extract UIDs for error reporting
                        if (auto elem = dataset.get(core::tags::sop_class_uid)) {
                            result.sop_class_uid = elem->as_string().unwrap_or("");
                        }
                        if (auto elem = dataset.get(core::tags::sop_instance_uid)) {
                            result.sop_instance_uid = elem->as_string().unwrap_or("");
                        }

                        store_response.failed_instances.push_back(
                            std::move(result));
                        continue;
                    }

                    // Extract UIDs
                    auto sop_class_elem = dataset.get(core::tags::sop_class_uid);
                    auto sop_instance_elem = dataset.get(core::tags::sop_instance_uid);
                    auto study_uid_elem = dataset.get(core::tags::study_instance_uid);
                    auto series_uid_elem = dataset.get(core::tags::series_instance_uid);

                    result.sop_class_uid = sop_class_elem->as_string().unwrap_or("");
                    result.sop_instance_uid = sop_instance_elem->as_string().unwrap_or("");

                    std::string study_uid = study_uid_elem->as_string().unwrap_or("");
                    std::string series_uid = series_uid_elem->as_string().unwrap_or("");

                    // Check for duplicate
                    auto existing_result = ctx->database->get_file_path(
                        result.sop_instance_uid);
                    if (existing_result.is_ok() && existing_result.value()) {
                        result.success = false;
                        result.error_code = "DUPLICATE";
                        result.error_message = "Instance already exists";
                        store_response.failed_instances.push_back(
                            std::move(result));
                        continue;
                    }

                    // TODO: Implement full storage pipeline with file_storage
                    // For now, just validate and report success
                    // Full implementation requires:
                    // 1. Generate file path
                    // 2. Write file via file_storage
                    // 3. Upsert patient/study/series/instance records

                    // Success (validation only for now)
                    result.success = true;
                    result.retrieve_url = "/dicomweb/studies/" + study_uid +
                                          "/series/" + series_uid +
                                          "/instances/" + result.sop_instance_uid;
                    store_response.referenced_instances.push_back(
                        std::move(result));
                }

                // Build response
                std::string base_url;  // Empty base URL, client should use relative URLs

                res.add_header("Content-Type",
                               std::string(dicomweb::media_type::dicom_json));

                if (store_response.all_failed()) {
                    res.code = 409; // Conflict
                } else if (store_response.partial_success()) {
                    res.code = 202; // Accepted with warnings
                } else {
                    res.code = 200; // OK
                }

                res.body = dicomweb::build_store_response_json(
                    store_response, base_url);
                return res;
            });

    // POST /dicomweb/studies/{studyUID} - Store instances to existing study
    CROW_ROUTE(app, "/dicomweb/studies/<string>")
        .methods(crow::HTTPMethod::POST)(
            [ctx](const crow::request& req, const std::string& target_study_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->database) {
                    res.code = 503;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                // Parse Content-Type header
                auto content_type = req.get_header_value("Content-Type");
                if (content_type.empty() ||
                    content_type.find("multipart/related") == std::string::npos) {
                    res.code = 415;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json(
                        "UNSUPPORTED_MEDIA_TYPE",
                        "Content-Type must be multipart/related");
                    return res;
                }

                // Parse multipart body
                auto parse_result = dicomweb::multipart_parser::parse(
                    content_type, req.body);

                if (!parse_result) {
                    res.code = 400;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json(
                        parse_result.error->code,
                        parse_result.error->message);
                    return res;
                }

                if (parse_result.parts.empty()) {
                    res.code = 400;
                    res.add_header("Content-Type", "application/json");
                    res.body = make_error_json(
                        "NO_INSTANCES",
                        "No DICOM instances in request body");
                    return res;
                }

                // Process each part
                dicomweb::store_response store_response;

                for (const auto& part : parse_result.parts) {
                    dicomweb::store_instance_result result;

                    // Only process application/dicom parts
                    if (part.content_type.find("application/dicom") ==
                        std::string::npos) {
                        continue;
                    }

                    // Parse DICOM from memory
                    auto dicom_result = core::dicom_file::from_bytes(
                        std::span<const uint8_t>(part.data.data(), part.data.size()));

                    if (dicom_result.is_err()) {
                        result.success = false;
                        result.error_code = "INVALID_DATA";
                        result.error_message = "Failed to parse DICOM data";
                        store_response.failed_instances.push_back(
                            std::move(result));
                        continue;
                    }

                    const auto& dataset = dicom_result.value().dataset();

                    // Validate instance with target study UID check
                    auto validation = dicomweb::validate_instance(
                        dataset, target_study_uid);
                    if (!validation) {
                        result.success = false;
                        result.error_code = validation.error_code;
                        result.error_message = validation.error_message;

                        if (auto elem = dataset.get(core::tags::sop_class_uid)) {
                            result.sop_class_uid = elem->as_string().unwrap_or("");
                        }
                        if (auto elem = dataset.get(core::tags::sop_instance_uid)) {
                            result.sop_instance_uid = elem->as_string().unwrap_or("");
                        }

                        store_response.failed_instances.push_back(
                            std::move(result));
                        continue;
                    }

                    // Extract UIDs
                    auto sop_class_elem = dataset.get(core::tags::sop_class_uid);
                    auto sop_instance_elem = dataset.get(core::tags::sop_instance_uid);
                    auto series_uid_elem = dataset.get(core::tags::series_instance_uid);

                    result.sop_class_uid = sop_class_elem->as_string().unwrap_or("");
                    result.sop_instance_uid = sop_instance_elem->as_string().unwrap_or("");
                    std::string series_uid = series_uid_elem->as_string().unwrap_or("");

                    // Check for duplicate
                    auto existing_result = ctx->database->get_file_path(
                        result.sop_instance_uid);
                    if (existing_result.is_ok() && existing_result.value()) {
                        result.success = false;
                        result.error_code = "DUPLICATE";
                        result.error_message = "Instance already exists";
                        store_response.failed_instances.push_back(
                            std::move(result));
                        continue;
                    }

                    // TODO: Implement full storage pipeline with file_storage
                    // For now, just validate and report success
                    // Full implementation requires:
                    // 1. Generate file path
                    // 2. Write file via file_storage
                    // 3. Upsert patient/study/series/instance records

                    // Success (validation only for now)
                    result.success = true;
                    result.retrieve_url = "/dicomweb/studies/" + target_study_uid +
                                          "/series/" + series_uid +
                                          "/instances/" + result.sop_instance_uid;
                    store_response.referenced_instances.push_back(
                        std::move(result));
                }

                // Build response
                std::string base_url;  // Empty base URL, client should use relative URLs

                res.add_header("Content-Type",
                               std::string(dicomweb::media_type::dicom_json));

                if (store_response.all_failed()) {
                    res.code = 409; // Conflict
                } else if (store_response.partial_success()) {
                    res.code = 202; // Accepted with warnings
                } else {
                    res.code = 200; // OK
                }

                res.body = dicomweb::build_store_response_json(
                    store_response, base_url);
                return res;
            });

    // ========================================================================
    // QIDO-RS (Query based on ID for DICOM Objects)
    // ========================================================================

    // GET /dicomweb/studies - Search for studies
    CROW_ROUTE(app, "/dicomweb/studies")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req) {
                crow::response res;
                add_cors_headers(res, *ctx);
                res.add_header("Content-Type",
                               std::string(dicomweb::media_type::dicom_json));

                if (!ctx->database) {
                    res.code = 503;
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                // Parse query parameters
                auto query = dicomweb::parse_study_query_params(req.raw_url);

                // Set default limit if not specified (prevent unlimited queries)
                if (query.limit == 0) {
                    query.limit = 100;
                }

                // Execute search
                auto studies_result = ctx->database->search_studies(query);
                if (!studies_result.is_ok()) {
                    res.code = 500;
                    res.body = make_error_json("QUERY_ERROR",
                                               studies_result.error().message);
                    return res;
                }

                // Build response
                std::ostringstream oss;
                oss << "[";

                bool first = true;
                for (const auto& study : studies_result.value()) {
                    if (!first) oss << ",";
                    first = false;

                    // Get patient info for this study
                    std::string patient_id;
                    std::string patient_name;
                    if (auto patient = ctx->database->find_patient_by_pk(study.patient_pk)) {
                        patient_id = patient->patient_id;
                        patient_name = patient->patient_name;
                    }

                    oss << dicomweb::study_record_to_dicom_json(
                        study, patient_id, patient_name);
                }

                oss << "]";

                res.code = 200;
                res.body = oss.str();
                return res;
            });

    // GET /dicomweb/series - Search for all series
    CROW_ROUTE(app, "/dicomweb/series")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req) {
                crow::response res;
                add_cors_headers(res, *ctx);
                res.add_header("Content-Type",
                               std::string(dicomweb::media_type::dicom_json));

                if (!ctx->database) {
                    res.code = 503;
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                // Parse query parameters
                auto query = dicomweb::parse_series_query_params(req.raw_url);

                // Set default limit if not specified
                if (query.limit == 0) {
                    query.limit = 100;
                }

                // Execute search
                auto series_list_result = ctx->database->search_series(query);
                if (!series_list_result.is_ok()) {
                    res.code = 500;
                    res.body = make_error_json("QUERY_ERROR",
                                               series_list_result.error().message);
                    return res;
                }

                // Build response
                std::ostringstream oss;
                oss << "[";

                bool first = true;
                for (const auto& series : series_list_result.value()) {
                    if (!first) oss << ",";
                    first = false;

                    // Get study UID for this series
                    std::string study_uid;
                    if (auto study = ctx->database->find_study_by_pk(series.study_pk)) {
                        study_uid = study->study_uid;
                    }

                    oss << dicomweb::series_record_to_dicom_json(series, study_uid);
                }

                oss << "]";

                res.code = 200;
                res.body = oss.str();
                return res;
            });

    // GET /dicomweb/instances - Search for all instances
    CROW_ROUTE(app, "/dicomweb/instances")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req) {
                crow::response res;
                add_cors_headers(res, *ctx);
                res.add_header("Content-Type",
                               std::string(dicomweb::media_type::dicom_json));

                if (!ctx->database) {
                    res.code = 503;
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                // Parse query parameters
                auto query = dicomweb::parse_instance_query_params(req.raw_url);

                // Set default limit if not specified
                if (query.limit == 0) {
                    query.limit = 100;
                }

                // Execute search
                auto instances_result = ctx->database->search_instances(query);
                if (!instances_result.is_ok()) {
                    res.code = 500;
                    res.body = make_error_json("QUERY_ERROR",
                                               instances_result.error().message);
                    return res;
                }

                // Build response
                std::ostringstream oss;
                oss << "[";

                bool first = true;
                for (const auto& instance : instances_result.value()) {
                    if (!first) oss << ",";
                    first = false;

                    // Get series and study UIDs
                    std::string series_uid;
                    std::string study_uid;
                    if (auto series = ctx->database->find_series_by_pk(instance.series_pk)) {
                        series_uid = series->series_uid;
                        if (auto study = ctx->database->find_study_by_pk(series->study_pk)) {
                            study_uid = study->study_uid;
                        }
                    }

                    oss << dicomweb::instance_record_to_dicom_json(
                        instance, series_uid, study_uid);
                }

                oss << "]";

                res.code = 200;
                res.body = oss.str();
                return res;
            });

    // GET /dicomweb/studies/{studyUID}/series - Search series in a study (QIDO-RS)
    CROW_ROUTE(app, "/dicomweb/studies/<string>/series")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);
                res.add_header("Content-Type",
                               std::string(dicomweb::media_type::dicom_json));

                if (!ctx->database) {
                    res.code = 503;
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                // Verify study exists
                auto study = ctx->database->find_study(study_uid);
                if (!study) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Study not found");
                    return res;
                }

                // Parse query parameters and add study filter
                auto query = dicomweb::parse_series_query_params(req.raw_url);
                query.study_uid = study_uid;

                // Set default limit if not specified
                if (query.limit == 0) {
                    query.limit = 100;
                }

                // Execute search
                auto series_list_result = ctx->database->search_series(query);
                if (!series_list_result.is_ok()) {
                    res.code = 500;
                    res.body = make_error_json("QUERY_ERROR",
                                               series_list_result.error().message);
                    return res;
                }

                // Build response
                std::ostringstream oss;
                oss << "[";

                bool first = true;
                for (const auto& series : series_list_result.value()) {
                    if (!first) oss << ",";
                    first = false;

                    oss << dicomweb::series_record_to_dicom_json(series, study_uid);
                }

                oss << "]";

                res.code = 200;
                res.body = oss.str();
                return res;
            });

    // GET /dicomweb/studies/{studyUID}/instances - Search instances in a study (QIDO-RS)
    CROW_ROUTE(app, "/dicomweb/studies/<string>/instances")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req, const std::string& study_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);
                res.add_header("Content-Type",
                               std::string(dicomweb::media_type::dicom_json));

                if (!ctx->database) {
                    res.code = 503;
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                // Verify study exists
                auto study = ctx->database->find_study(study_uid);
                if (!study) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Study not found");
                    return res;
                }

                // Get all series in this study and then instances
                storage::series_query series_query;
                series_query.study_uid = study_uid;
                auto series_list_result = ctx->database->search_series(series_query);
                if (!series_list_result.is_ok()) {
                    res.code = 500;
                    res.body = make_error_json("QUERY_ERROR",
                                               series_list_result.error().message);
                    return res;
                }

                // Parse query parameters for additional filters
                auto inst_query = dicomweb::parse_instance_query_params(req.raw_url);
                if (inst_query.limit == 0) {
                    inst_query.limit = 100;
                }

                // Collect instances from all series
                std::ostringstream oss;
                oss << "[";

                bool first = true;
                size_t count = 0;
                size_t skipped = 0;

                for (const auto& series : series_list_result.value()) {
                    if (count >= inst_query.limit) break;

                    // Search instances in this series
                    storage::instance_query query;
                    query.series_uid = series.series_uid;
                    if (inst_query.sop_uid.has_value()) {
                        query.sop_uid = inst_query.sop_uid;
                    }
                    if (inst_query.sop_class_uid.has_value()) {
                        query.sop_class_uid = inst_query.sop_class_uid;
                    }
                    if (inst_query.instance_number.has_value()) {
                        query.instance_number = inst_query.instance_number;
                    }
                    query.limit = inst_query.limit - count;

                    auto instances_result = ctx->database->search_instances(query);
                    if (!instances_result.is_ok()) {
                        continue;
                    }
                    for (const auto& instance : instances_result.value()) {
                        // Handle offset
                        if (skipped < inst_query.offset) {
                            ++skipped;
                            continue;
                        }

                        if (count >= inst_query.limit) break;

                        if (!first) oss << ",";
                        first = false;

                        oss << dicomweb::instance_record_to_dicom_json(
                            instance, series.series_uid, study_uid);
                        ++count;
                    }
                }

                oss << "]";

                res.code = 200;
                res.body = oss.str();
                return res;
            });

    // GET /dicomweb/studies/{studyUID}/series/{seriesUID}/instances - Search instances in a series (QIDO-RS)
    CROW_ROUTE(app, "/dicomweb/studies/<string>/series/<string>/instances")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& req,
                  const std::string& study_uid,
                  const std::string& series_uid) {
                crow::response res;
                add_cors_headers(res, *ctx);
                res.add_header("Content-Type",
                               std::string(dicomweb::media_type::dicom_json));

                if (!ctx->database) {
                    res.code = 503;
                    res.body = make_error_json("DATABASE_UNAVAILABLE",
                                               "Database not configured");
                    return res;
                }

                // Verify study exists
                auto study = ctx->database->find_study(study_uid);
                if (!study) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Study not found");
                    return res;
                }

                // Verify series exists
                auto series = ctx->database->find_series(series_uid);
                if (!series) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Series not found");
                    return res;
                }

                // Parse query parameters and add series filter
                auto query = dicomweb::parse_instance_query_params(req.raw_url);
                query.series_uid = series_uid;

                // Set default limit if not specified
                if (query.limit == 0) {
                    query.limit = 100;
                }

                // Execute search
                auto instances_result = ctx->database->search_instances(query);
                if (!instances_result.is_ok()) {
                    res.code = 500;
                    res.body = make_error_json("QUERY_ERROR",
                                               instances_result.error().message);
                    return res;
                }

                // Build response
                std::ostringstream oss;
                oss << "[";

                bool first = true;
                for (const auto& instance : instances_result.value()) {
                    if (!first) oss << ",";
                    first = false;

                    oss << dicomweb::instance_record_to_dicom_json(
                        instance, series_uid, study_uid);
                }

                oss << "]";

                res.code = 200;
                res.body = oss.str();
                return res;
            });

    // ========================================================================
    // CORS Preflight Handler for DICOMweb
    // ========================================================================

    CROW_ROUTE(app, "/dicomweb/<path>")
        .methods(crow::HTTPMethod::OPTIONS)(
            [ctx](const crow::request& /*req*/, const std::string& /*path*/) {
                crow::response res(204);
                if (ctx->config) {
                    res.add_header("Access-Control-Allow-Origin",
                                   ctx->config->cors_allowed_origins);
                }
                res.add_header("Access-Control-Allow-Methods",
                               "GET, POST, OPTIONS");
                res.add_header("Access-Control-Allow-Headers",
                               "Content-Type, Accept, Authorization");
                res.add_header("Access-Control-Max-Age", "86400");
                return res;
            });
}

} // namespace pacs::web::endpoints
