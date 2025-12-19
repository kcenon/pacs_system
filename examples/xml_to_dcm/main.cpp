/**
 * @file main.cpp
 * @brief XML to DICOM Converter - DICOM Native XML PS3.19
 *
 * A command-line utility for converting XML files to DICOM format
 * following the DICOM Native XML representation standard (PS3.19).
 *
 * @see Issue #283 - dcm_to_xml / xml_to_dcm Implementation
 * @see DICOM PS3.19 - Application Hosting
 *
 * Usage:
 *   xml_to_dcm <xml-file> <output-dcm> [options]
 *
 * Example:
 *   xml_to_dcm metadata.xml output.dcm
 *   xml_to_dcm metadata.xml output.dcm --template template.dcm
 */

#include "pacs/core/dicom_dictionary.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/transfer_syntax.hpp"
#include "pacs/encoding/vr_type.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

/**
 * @brief Command line options
 */
struct options {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    std::filesystem::path template_path;
    std::filesystem::path bulk_data_dir;
    std::string transfer_syntax;
    bool verbose{false};
    bool quiet{false};
};

// ============================================================================
// Minimal XML Parser
// ============================================================================

/**
 * @brief Simple XML node representation
 */
struct xml_node {
    std::string name;
    std::map<std::string, std::string> attributes;
    std::string text;
    std::vector<xml_node> children;

    [[nodiscard]] bool has_child(const std::string& child_name) const {
        for (const auto& child : children) {
            if (child.name == child_name) return true;
        }
        return false;
    }

    [[nodiscard]] const xml_node* find_child(const std::string& child_name) const {
        for (const auto& child : children) {
            if (child.name == child_name) return &child;
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<const xml_node*> find_children(const std::string& child_name) const {
        std::vector<const xml_node*> result;
        for (const auto& child : children) {
            if (child.name == child_name) {
                result.push_back(&child);
            }
        }
        return result;
    }

    [[nodiscard]] std::string get_attr(const std::string& attr_name,
                                        const std::string& default_value = "") const {
        auto it = attributes.find(attr_name);
        return it != attributes.end() ? it->second : default_value;
    }
};

/**
 * @brief Simple XML parser for DICOM Native XML
 */
class xml_parser {
public:
    explicit xml_parser(const std::string& input) : input_(input), pos_(0) {}

    xml_node parse() {
        skip_whitespace();
        skip_xml_declaration();
        skip_whitespace();
        return parse_element();
    }

private:
    const std::string& input_;
    size_t pos_;

    [[nodiscard]] char peek() const {
        return pos_ < input_.size() ? input_[pos_] : '\0';
    }

    char get() {
        return pos_ < input_.size() ? input_[pos_++] : '\0';
    }

    void skip_whitespace() {
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
            ++pos_;
        }
    }

    void skip_xml_declaration() {
        if (input_.substr(pos_, 5) == "<?xml") {
            while (pos_ < input_.size() && input_.substr(pos_, 2) != "?>") {
                ++pos_;
            }
            if (pos_ < input_.size()) pos_ += 2;
        }
    }

    void skip_comment() {
        if (input_.substr(pos_, 4) == "<!--") {
            pos_ += 4;
            while (pos_ < input_.size() && input_.substr(pos_, 3) != "-->") {
                ++pos_;
            }
            if (pos_ < input_.size()) pos_ += 3;
        }
    }

    [[nodiscard]] std::string parse_name() {
        size_t start = pos_;
        while (pos_ < input_.size() &&
               (std::isalnum(static_cast<unsigned char>(input_[pos_])) ||
                input_[pos_] == '_' || input_[pos_] == ':' || input_[pos_] == '-')) {
            ++pos_;
        }
        return input_.substr(start, pos_ - start);
    }

    [[nodiscard]] std::string parse_attribute_value() {
        char quote = get();  // consume opening quote
        std::string result;

        while (peek() != quote && peek() != '\0') {
            if (peek() == '&') {
                result += parse_entity();
            } else {
                result += get();
            }
        }
        get();  // consume closing quote
        return result;
    }

    [[nodiscard]] std::string parse_entity() {
        get();  // consume '&'
        std::string entity;
        while (peek() != ';' && peek() != '\0') {
            entity += get();
        }
        get();  // consume ';'

        if (entity == "lt") return "<";
        if (entity == "gt") return ">";
        if (entity == "amp") return "&";
        if (entity == "quot") return "\"";
        if (entity == "apos") return "'";
        if (!entity.empty() && entity[0] == '#') {
            // Numeric entity
            int code = 0;
            if (entity.size() > 1 && entity[1] == 'x') {
                code = std::stoi(entity.substr(2), nullptr, 16);
            } else {
                code = std::stoi(entity.substr(1));
            }
            return std::string(1, static_cast<char>(code));
        }
        return "&" + entity + ";";  // Unknown entity
    }

    [[nodiscard]] std::string parse_text() {
        std::string result;
        while (peek() != '<' && peek() != '\0') {
            if (peek() == '&') {
                result += parse_entity();
            } else {
                result += get();
            }
        }
        // Trim whitespace
        size_t start = result.find_first_not_of(" \t\n\r");
        size_t end = result.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        return result.substr(start, end - start + 1);
    }

    xml_node parse_element() {
        xml_node node;
        skip_whitespace();

        // Skip comments
        while (input_.substr(pos_, 4) == "<!--") {
            skip_comment();
            skip_whitespace();
        }

        if (peek() != '<') {
            throw std::runtime_error("Expected '<' at position " + std::to_string(pos_));
        }
        get();  // consume '<'

        // Parse element name
        node.name = parse_name();
        skip_whitespace();

        // Parse attributes
        while (peek() != '>' && peek() != '/' && peek() != '\0') {
            std::string attr_name = parse_name();
            skip_whitespace();
            if (peek() == '=') {
                get();  // consume '='
                skip_whitespace();
                std::string attr_value = parse_attribute_value();
                node.attributes[attr_name] = attr_value;
            }
            skip_whitespace();
        }

        // Check for self-closing tag
        if (peek() == '/') {
            get();  // consume '/'
            if (peek() == '>') {
                get();  // consume '>'
                return node;
            }
        }

        if (peek() != '>') {
            throw std::runtime_error("Expected '>' at position " + std::to_string(pos_));
        }
        get();  // consume '>'

        // Parse content
        while (true) {
            skip_whitespace();

            // Skip comments
            while (input_.substr(pos_, 4) == "<!--") {
                skip_comment();
                skip_whitespace();
            }

            if (input_.substr(pos_, 2) == "</") {
                // End tag
                pos_ += 2;
                std::string end_name = parse_name();
                skip_whitespace();
                if (peek() == '>') get();
                break;
            } else if (peek() == '<') {
                // Child element
                node.children.push_back(parse_element());
            } else if (peek() != '\0') {
                // Text content
                node.text += parse_text();
            } else {
                break;
            }
        }

        return node;
    }
};

// ============================================================================
// Base64 Decoding
// ============================================================================

constexpr int8_t base64_decode_table[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1
};

[[nodiscard]] std::vector<uint8_t> from_base64(const std::string& input) {
    std::vector<uint8_t> result;
    result.reserve((input.size() * 3) / 4);

    size_t i = 0;
    while (i < input.size()) {
        while (i < input.size() && std::isspace(static_cast<unsigned char>(input[i]))) ++i;
        if (i >= input.size()) break;

        uint32_t sextet[4] = {0, 0, 0, 0};
        int padding = 0;

        for (int j = 0; j < 4 && i < input.size(); ++j) {
            char c = input[i++];
            if (c == '=') {
                padding++;
                sextet[j] = 0;
            } else if (static_cast<unsigned char>(c) < 128 &&
                       base64_decode_table[static_cast<unsigned char>(c)] >= 0) {
                sextet[j] = static_cast<uint32_t>(base64_decode_table[static_cast<unsigned char>(c)]);
            } else {
                --j;
            }
        }

        uint32_t triple = (sextet[0] << 18) | (sextet[1] << 12) | (sextet[2] << 6) | sextet[3];

        result.push_back(static_cast<uint8_t>((triple >> 16) & 0xFF));
        if (padding < 2) result.push_back(static_cast<uint8_t>((triple >> 8) & 0xFF));
        if (padding < 1) result.push_back(static_cast<uint8_t>(triple & 0xFF));
    }

    return result;
}

// ============================================================================
// Utility Functions
// ============================================================================

[[nodiscard]] pacs::encoding::vr_type parse_vr(const std::string& vr_str) {
    using namespace pacs::encoding;

    static const std::map<std::string, vr_type> vr_map = {
        {"AE", vr_type::AE}, {"AS", vr_type::AS}, {"AT", vr_type::AT},
        {"CS", vr_type::CS}, {"DA", vr_type::DA}, {"DS", vr_type::DS},
        {"DT", vr_type::DT}, {"FL", vr_type::FL}, {"FD", vr_type::FD},
        {"IS", vr_type::IS}, {"LO", vr_type::LO}, {"LT", vr_type::LT},
        {"OB", vr_type::OB}, {"OD", vr_type::OD}, {"OF", vr_type::OF},
        {"OL", vr_type::OL}, {"OV", vr_type::OV}, {"OW", vr_type::OW},
        {"PN", vr_type::PN}, {"SH", vr_type::SH}, {"SL", vr_type::SL},
        {"SQ", vr_type::SQ}, {"SS", vr_type::SS}, {"ST", vr_type::ST},
        {"SV", vr_type::SV}, {"TM", vr_type::TM}, {"UC", vr_type::UC},
        {"UI", vr_type::UI}, {"UL", vr_type::UL}, {"UN", vr_type::UN},
        {"UR", vr_type::UR}, {"US", vr_type::US}, {"UT", vr_type::UT},
        {"UV", vr_type::UV}
    };

    auto it = vr_map.find(vr_str);
    return it != vr_map.end() ? it->second : vr_type::UN;
}

[[nodiscard]] std::optional<pacs::core::dicom_tag> parse_tag(const std::string& tag_str) {
    if (tag_str.length() != 8) {
        return std::nullopt;
    }

    try {
        uint16_t group = static_cast<uint16_t>(std::stoul(tag_str.substr(0, 4), nullptr, 16));
        uint16_t elem = static_cast<uint16_t>(std::stoul(tag_str.substr(4, 4), nullptr, 16));
        return pacs::core::dicom_tag{group, elem};
    } catch (...) {
        return std::nullopt;
    }
}

[[nodiscard]] std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + path.string());
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

[[nodiscard]] std::vector<uint8_t> read_bulk_data(const std::string& uri,
                                                   const std::filesystem::path& bulk_dir) {
    std::string path = uri;

    if (path.substr(0, 7) == "file://") {
        path = path.substr(7);
    }

    std::filesystem::path file_path = path;
    if (!file_path.is_absolute() && !bulk_dir.empty()) {
        file_path = bulk_dir / file_path;
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open bulk data file: " + file_path.string());
    }

    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

// Forward declaration
void parse_dataset(const xml_node& node,
                   pacs::core::dicom_dataset& dataset,
                   const options& opts);

/**
 * @brief Build PersonName string from XML PersonName element
 */
[[nodiscard]] std::string build_person_name(const xml_node& pn_node) {
    std::string result;

    auto build_component = [](const xml_node* rep_node) -> std::string {
        if (!rep_node) return "";

        std::string family, given, middle, prefix, suffix;
        if (auto* n = rep_node->find_child("FamilyName")) family = n->text;
        if (auto* n = rep_node->find_child("GivenName")) given = n->text;
        if (auto* n = rep_node->find_child("MiddleName")) middle = n->text;
        if (auto* n = rep_node->find_child("NamePrefix")) prefix = n->text;
        if (auto* n = rep_node->find_child("NameSuffix")) suffix = n->text;

        std::string comp = family;
        if (!given.empty() || !middle.empty() || !prefix.empty() || !suffix.empty()) {
            comp += "^" + given;
        }
        if (!middle.empty() || !prefix.empty() || !suffix.empty()) {
            comp += "^" + middle;
        }
        if (!prefix.empty() || !suffix.empty()) {
            comp += "^" + prefix;
        }
        if (!suffix.empty()) {
            comp += "^" + suffix;
        }

        return comp;
    };

    std::string alphabetic = build_component(pn_node.find_child("Alphabetic"));
    std::string ideographic = build_component(pn_node.find_child("Ideographic"));
    std::string phonetic = build_component(pn_node.find_child("Phonetic"));

    result = alphabetic;
    if (!ideographic.empty() || !phonetic.empty()) {
        result += "=" + ideographic;
    }
    if (!phonetic.empty()) {
        result += "=" + phonetic;
    }

    return result;
}

/**
 * @brief Create DICOM element from XML DicomAttribute node
 */
[[nodiscard]] pacs::core::dicom_element create_element(
    const pacs::core::dicom_tag& tag,
    const xml_node& attr_node,
    const options& opts) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    std::string vr_str = attr_node.get_attr("vr", "UN");
    auto vr = parse_vr(vr_str);

    // Handle sequence
    if (vr == vr_type::SQ) {
        dicom_element elem{tag, vr};
        auto items = attr_node.find_children("Item");
        for (const auto* item_node : items) {
            dicom_dataset item_dataset;
            parse_dataset(*item_node, item_dataset, opts);
            elem.sequence_items().push_back(std::move(item_dataset));
        }
        return elem;
    }

    // Handle InlineBinary (Base64)
    if (auto* inline_binary = attr_node.find_child("InlineBinary")) {
        auto data = from_base64(inline_binary->text);
        return dicom_element{tag, vr, std::span<const uint8_t>(data)};
    }

    // Handle BulkData URI
    if (auto* bulk_data = attr_node.find_child("BulkData")) {
        std::string uri = bulk_data->get_attr("uri");
        if (!uri.empty()) {
            auto data = read_bulk_data(uri, opts.bulk_data_dir);
            return dicom_element{tag, vr, std::span<const uint8_t>(data)};
        }
    }

    // Handle PersonName
    if (vr == vr_type::PN) {
        auto pn_nodes = attr_node.find_children("PersonName");
        if (!pn_nodes.empty()) {
            std::string combined;
            for (size_t i = 0; i < pn_nodes.size(); ++i) {
                if (i > 0) combined += "\\";
                combined += build_person_name(*pn_nodes[i]);
            }
            return dicom_element::from_string(tag, vr, combined);
        }
    }

    // Handle Value elements
    auto value_nodes = attr_node.find_children("Value");
    if (value_nodes.empty()) {
        return dicom_element{tag, vr};
    }

    // Collect values sorted by number attribute
    std::vector<std::pair<int, std::string>> numbered_values;
    for (const auto* val_node : value_nodes) {
        int num = 1;
        try {
            num = std::stoi(val_node->get_attr("number", "1"));
        } catch (...) {}
        numbered_values.emplace_back(num, val_node->text);
    }
    std::sort(numbered_values.begin(), numbered_values.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    // Handle string VRs
    if (is_string_vr(vr)) {
        std::string combined;
        for (size_t i = 0; i < numbered_values.size(); ++i) {
            if (i > 0) combined += "\\";
            combined += numbered_values[i].second;
        }
        return dicom_element::from_string(tag, vr, combined);
    }

    // Handle numeric VRs
    if (is_numeric_vr(vr)) {
        std::vector<uint8_t> data;

        auto write_values = [&]<typename T>() {
            for (const auto& [num, val_str] : numbered_values) {
                T num_val{};
                try {
                    if constexpr (std::is_floating_point_v<T>) {
                        num_val = static_cast<T>(std::stod(val_str));
                    } else {
                        num_val = static_cast<T>(std::stoll(val_str));
                    }
                } catch (...) {}
                const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&num_val);
                data.insert(data.end(), ptr, ptr + sizeof(T));
            }
        };

        switch (vr) {
            case vr_type::US: write_values.template operator()<uint16_t>(); break;
            case vr_type::SS: write_values.template operator()<int16_t>(); break;
            case vr_type::UL: write_values.template operator()<uint32_t>(); break;
            case vr_type::SL: write_values.template operator()<int32_t>(); break;
            case vr_type::FL: write_values.template operator()<float>(); break;
            case vr_type::FD: write_values.template operator()<double>(); break;
            case vr_type::UV: write_values.template operator()<uint64_t>(); break;
            case vr_type::SV: write_values.template operator()<int64_t>(); break;
            default: break;
        }

        return dicom_element{tag, vr, std::span<const uint8_t>(data)};
    }

    // Handle AT (Attribute Tag)
    if (vr == vr_type::AT) {
        std::vector<uint8_t> data;
        for (const auto& [num, val_str] : numbered_values) {
            auto tag_opt = parse_tag(val_str);
            if (tag_opt) {
                uint16_t group = tag_opt->group();
                uint16_t elem = tag_opt->element();
                data.push_back(static_cast<uint8_t>(group & 0xFF));
                data.push_back(static_cast<uint8_t>((group >> 8) & 0xFF));
                data.push_back(static_cast<uint8_t>(elem & 0xFF));
                data.push_back(static_cast<uint8_t>((elem >> 8) & 0xFF));
            }
        }
        return dicom_element{tag, vr, std::span<const uint8_t>(data)};
    }

    // Fallback: treat as string
    if (!numbered_values.empty()) {
        std::string combined;
        for (size_t i = 0; i < numbered_values.size(); ++i) {
            if (i > 0) combined += "\\";
            combined += numbered_values[i].second;
        }
        return dicom_element::from_string(tag, vr, combined);
    }

    return dicom_element{tag, vr};
}

/**
 * @brief Parse XML into DICOM dataset
 */
void parse_dataset(const xml_node& node,
                   pacs::core::dicom_dataset& dataset,
                   const options& opts) {
    for (const auto& child : node.children) {
        if (child.name != "DicomAttribute") continue;

        std::string tag_str = child.get_attr("tag");
        auto tag_opt = parse_tag(tag_str);
        if (!tag_opt) {
            if (opts.verbose) {
                std::cerr << "Warning: Invalid tag '" << tag_str << "', skipping\n";
            }
            continue;
        }

        try {
            auto element = create_element(*tag_opt, child, opts);
            dataset.insert(std::move(element));
        } catch (const std::exception& e) {
            if (opts.verbose) {
                std::cerr << "Warning: Failed to parse element " << tag_str
                          << ": " << e.what() << "\n";
            }
        }
    }
}

void print_usage(const char* program_name) {
    std::cout << R"(
XML to DICOM Converter (DICOM Native XML PS3.19)

Usage: )" << program_name
              << R"( <xml-file> <output-dcm> [options]

Arguments:
  xml-file          Input XML file (DICOM Native XML PS3.19 format)
  output-dcm        Output DICOM file

Options:
  -h, --help              Show this help message
  -t, --transfer-syntax   Transfer Syntax UID (default: Explicit VR Little Endian)
  --template <dcm>        Template DICOM file (copies pixel data and missing tags)
  --bulk-data-dir <dir>   Directory for BulkData URI resolution
  -v, --verbose           Verbose output
  -q, --quiet             Quiet mode (errors only)

Transfer Syntax Options:
  1.2.840.10008.1.2      Implicit VR Little Endian
  1.2.840.10008.1.2.1    Explicit VR Little Endian (default)
  1.2.840.10008.1.2.2    Explicit VR Big Endian

Examples:
  )" << program_name
              << R"( metadata.xml output.dcm
  )" << program_name
              << R"( metadata.xml output.dcm --template original.dcm
  )" << program_name
              << R"( metadata.xml output.dcm --bulk-data-dir ./bulk/
  )" << program_name
              << R"( metadata.xml output.dcm -t 1.2.840.10008.1.2

Input Format (DICOM Native XML PS3.19):
  <?xml version="1.0" encoding="UTF-8"?>
  <NativeDicomModel>
    <DicomAttribute tag="00100010" vr="PN" keyword="PatientName">
      <PersonName>
        <Alphabetic>
          <FamilyName>DOE</FamilyName>
          <GivenName>JOHN</GivenName>
        </Alphabetic>
      </PersonName>
    </DicomAttribute>
  </NativeDicomModel>

Exit Codes:
  0  Success
  1  Invalid arguments
  2  File error or invalid XML
)";
}

bool parse_arguments(int argc, char* argv[], options& opts) {
    if (argc < 3) {
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            return false;
        } else if ((arg == "--transfer-syntax" || arg == "-t") && i + 1 < argc) {
            opts.transfer_syntax = argv[++i];
        } else if (arg == "--template" && i + 1 < argc) {
            opts.template_path = argv[++i];
        } else if (arg == "--bulk-data-dir" && i + 1 < argc) {
            opts.bulk_data_dir = argv[++i];
        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--quiet" || arg == "-q") {
            opts.quiet = true;
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        } else if (opts.input_path.empty()) {
            opts.input_path = arg;
        } else if (opts.output_path.empty()) {
            opts.output_path = arg;
        } else {
            std::cerr << "Error: Too many arguments\n";
            return false;
        }
    }

    if (opts.input_path.empty()) {
        std::cerr << "Error: No input file specified\n";
        return false;
    }

    if (opts.output_path.empty()) {
        std::cerr << "Error: No output file specified\n";
        return false;
    }

    if (opts.quiet) {
        opts.verbose = false;
    }

    return true;
}

int convert_file(const options& opts) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    // Read and parse XML
    std::string xml_content;
    try {
        xml_content = read_file(opts.input_path);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }

    xml_node root;
    try {
        xml_parser parser(xml_content);
        root = parser.parse();
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to parse XML: " << e.what() << "\n";
        return 2;
    }

    if (root.name != "NativeDicomModel") {
        std::cerr << "Error: XML root element must be 'NativeDicomModel', got '"
                  << root.name << "'\n";
        return 2;
    }

    // Create dataset from XML
    dicom_dataset dataset;
    try {
        parse_dataset(root, dataset, opts);
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to create dataset: " << e.what() << "\n";
        return 2;
    }

    // Load template if specified
    std::optional<dicom_file> template_file;
    if (!opts.template_path.empty()) {
        auto result = dicom_file::open(opts.template_path);
        if (result.is_err()) {
            std::cerr << "Error: Failed to open template file: "
                      << result.error().message << "\n";
            return 2;
        }
        template_file = std::move(result.value());

        // Merge template data (XML values override template)
        for (const auto& [tag, element] : template_file->dataset()) {
            if (dataset.get(tag) == nullptr) {
                dataset.insert(element);
            }
        }
    }

    // Determine transfer syntax
    transfer_syntax ts = transfer_syntax::explicit_vr_little_endian;
    if (!opts.transfer_syntax.empty()) {
        auto ts_opt = find_transfer_syntax(opts.transfer_syntax);
        if (ts_opt) {
            ts = *ts_opt;
        } else {
            std::cerr << "Warning: Unknown transfer syntax '" << opts.transfer_syntax
                      << "', using Explicit VR Little Endian\n";
        }
    } else if (template_file) {
        ts = template_file->transfer_syntax();
    }

    // Create DICOM file
    auto file = dicom_file::create(dataset, ts);

    // Save to output
    auto save_result = file.save(opts.output_path);
    if (save_result.is_err()) {
        std::cerr << "Error: Failed to save DICOM file: "
                  << save_result.error().message << "\n";
        return 2;
    }

    if (!opts.quiet) {
        std::cout << "Successfully converted: " << opts.input_path.string()
                  << " -> " << opts.output_path.string() << "\n";
    }

    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        std::cout << R"(
 __  ____  __ _      _____  ___    ____   ____ __  __
 \ \/ /  \/  | |    |_   _|/ _ \  |  _ \ / ___|  \/  |
  \  /| |\/| | |      | | | | | | | | | | |   | |\/| |
  /  \| |  | | |___   | | | |_| | | |_| | |___| |  | |
 /_/\_\_|  |_|_____|  |_|  \___/  |____/ \____|_|  |_|

        XML to DICOM Converter (PS3.19)
)" << "\n";
        print_usage(argv[0]);
        return 1;
    }

    // Check if input path exists
    if (!std::filesystem::exists(opts.input_path)) {
        std::cerr << "Error: Input file does not exist: " << opts.input_path.string() << "\n";
        return 2;
    }

    // Show banner in non-quiet mode
    if (!opts.quiet) {
        std::cout << R"(
 __  ____  __ _      _____  ___    ____   ____ __  __
 \ \/ /  \/  | |    |_   _|/ _ \  |  _ \ / ___|  \/  |
  \  /| |\/| | |      | | | | | | | | | | |   | |\/| |
  /  \| |  | | |___   | | | |_| | | |_| | |___| |  | |
 /_/\_\_|  |_|_____|  |_|  \___/  |____/ \____|_|  |_|

        XML to DICOM Converter (PS3.19)
)" << "\n";
    }

    return convert_file(opts);
}
