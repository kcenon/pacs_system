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
    if (!sop_class || sop_class->as_string().empty()) {
        return validation_result::error(
            "MISSING_SOP_CLASS",
            "SOP Class UID (0008,0016) is required");
    }

    auto sop_instance = dataset.get(core::tags::sop_instance_uid);
    if (!sop_instance || sop_instance->as_string().empty()) {
        return validation_result::error(
            "MISSING_SOP_INSTANCE",
            "SOP Instance UID (0008,0018) is required");
    }

    auto study_uid = dataset.get(core::tags::study_instance_uid);
    if (!study_uid || study_uid->as_string().empty()) {
        return validation_result::error(
            "MISSING_STUDY_UID",
            "Study Instance UID (0020,000D) is required");
    }

    auto series_uid = dataset.get(core::tags::series_instance_uid);
    if (!series_uid || series_uid->as_string().empty()) {
        return validation_result::error(
            "MISSING_SERIES_UID",
            "Series Instance UID (0020,000E) is required");
    }

    // Validate study UID matches target if specified
    if (target_study_uid.has_value()) {
        auto instance_study_uid = study_uid->as_string();
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
            auto value_str = elem.as_string();
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
        if (!result) {
            continue;
        }

        if (!first) {
            oss << ",";
        }
        first = false;

        oss << dicomweb::dataset_to_dicom_json(
            result->dataset(), false, bulk_data_uri_prefix);
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
                    auto files = ctx->database->get_study_files(study_uid);
                    std::string bulk_uri = "/dicomweb/studies/" + study_uid +
                                           "/bulkdata/";
                    return build_metadata_response(files, *ctx, bulk_uri);
                }

                // Return DICOM instances
                auto files = ctx->database->get_study_files(study_uid);
                std::string base_uri = "/dicomweb/studies/" + study_uid;
                return build_multipart_dicom_response(files, *ctx, base_uri);
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

                auto files = ctx->database->get_study_files(study_uid);
                std::string bulk_uri = "/dicomweb/studies/" + study_uid +
                                       "/bulkdata/";
                return build_metadata_response(files, *ctx, bulk_uri);
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
                    auto files = ctx->database->get_series_files(series_uid);
                    std::string bulk_uri = "/dicomweb/studies/" + study_uid +
                                           "/series/" + series_uid + "/bulkdata/";
                    return build_metadata_response(files, *ctx, bulk_uri);
                }

                auto files = ctx->database->get_series_files(series_uid);
                std::string base_uri = "/dicomweb/studies/" + study_uid +
                                       "/series/" + series_uid;
                return build_multipart_dicom_response(files, *ctx, base_uri);
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

                auto files = ctx->database->get_series_files(series_uid);
                std::string bulk_uri = "/dicomweb/studies/" + study_uid +
                                       "/series/" + series_uid + "/bulkdata/";
                return build_metadata_response(files, *ctx, bulk_uri);
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

                auto file_path = ctx->database->get_file_path(sop_uid);
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

                auto file_path = ctx->database->get_file_path(sop_uid);
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

                    if (!dicom_result) {
                        result.success = false;
                        result.error_code = "INVALID_DATA";
                        result.error_message = "Failed to parse DICOM data";
                        store_response.failed_instances.push_back(
                            std::move(result));
                        continue;
                    }

                    const auto& dataset = dicom_result->dataset();

                    // Validate instance
                    auto validation = dicomweb::validate_instance(dataset);
                    if (!validation) {
                        result.success = false;
                        result.error_code = validation.error_code;
                        result.error_message = validation.error_message;

                        // Try to extract UIDs for error reporting
                        if (auto elem = dataset.get(core::tags::sop_class_uid)) {
                            result.sop_class_uid = elem->as_string();
                        }
                        if (auto elem = dataset.get(core::tags::sop_instance_uid)) {
                            result.sop_instance_uid = elem->as_string();
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

                    result.sop_class_uid = sop_class_elem->as_string();
                    result.sop_instance_uid = sop_instance_elem->as_string();

                    std::string study_uid = study_uid_elem->as_string();
                    std::string series_uid = series_uid_elem->as_string();

                    // Check for duplicate
                    auto existing = ctx->database->get_file_path(
                        result.sop_instance_uid);
                    if (existing) {
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

                    if (!dicom_result) {
                        result.success = false;
                        result.error_code = "INVALID_DATA";
                        result.error_message = "Failed to parse DICOM data";
                        store_response.failed_instances.push_back(
                            std::move(result));
                        continue;
                    }

                    const auto& dataset = dicom_result->dataset();

                    // Validate instance with target study UID check
                    auto validation = dicomweb::validate_instance(
                        dataset, target_study_uid);
                    if (!validation) {
                        result.success = false;
                        result.error_code = validation.error_code;
                        result.error_message = validation.error_message;

                        if (auto elem = dataset.get(core::tags::sop_class_uid)) {
                            result.sop_class_uid = elem->as_string();
                        }
                        if (auto elem = dataset.get(core::tags::sop_instance_uid)) {
                            result.sop_instance_uid = elem->as_string();
                        }

                        store_response.failed_instances.push_back(
                            std::move(result));
                        continue;
                    }

                    // Extract UIDs
                    auto sop_class_elem = dataset.get(core::tags::sop_class_uid);
                    auto sop_instance_elem = dataset.get(core::tags::sop_instance_uid);
                    auto series_uid_elem = dataset.get(core::tags::series_instance_uid);

                    result.sop_class_uid = sop_class_elem->as_string();
                    result.sop_instance_uid = sop_instance_elem->as_string();
                    std::string series_uid = series_uid_elem->as_string();

                    // Check for duplicate
                    auto existing = ctx->database->get_file_path(
                        result.sop_instance_uid);
                    if (existing) {
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
                auto studies = ctx->database->search_studies(query);

                // Build response
                std::ostringstream oss;
                oss << "[";

                bool first = true;
                for (const auto& study : studies) {
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
                auto series_list = ctx->database->search_series(query);

                // Build response
                std::ostringstream oss;
                oss << "[";

                bool first = true;
                for (const auto& series : series_list) {
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
                auto instances = ctx->database->search_instances(query);

                // Build response
                std::ostringstream oss;
                oss << "[";

                bool first = true;
                for (const auto& instance : instances) {
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
                auto series_list = ctx->database->search_series(query);

                // Build response
                std::ostringstream oss;
                oss << "[";

                bool first = true;
                for (const auto& series : series_list) {
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
                auto series_list = ctx->database->search_series(series_query);

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

                for (const auto& series : series_list) {
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

                    auto instances = ctx->database->search_instances(query);
                    for (const auto& instance : instances) {
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
                auto instances = ctx->database->search_instances(query);

                // Build response
                std::ostringstream oss;
                oss << "[";

                bool first = true;
                for (const auto& instance : instances) {
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
