/**
 * @file main.cpp
 * @brief JSON to DICOM Converter - DICOM PS3.18 JSON Representation
 *
 * A command-line utility for converting JSON files to DICOM format
 * following the DICOM PS3.18 JSON representation standard.
 *
 * @see Issue #282 - dcm_to_json / json_to_dcm Implementation
 * @see DICOM PS3.18 Section F.2 - JSON Representation
 *
 * Usage:
 *   json_to_dcm <json-file> <output-dcm> [options]
 *
 * Example:
 *   json_to_dcm metadata.json output.dcm
 *   json_to_dcm metadata.json output.dcm --template template.dcm
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
#include <variant>
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
// Minimal JSON Parser
// ============================================================================

/**
 * @brief JSON value type
 */
struct json_value;
using json_object = std::map<std::string, json_value>;
using json_array = std::vector<json_value>;

struct json_value {
    std::variant<std::nullptr_t, bool, double, std::string, json_array, json_object> data;

    bool is_null() const { return std::holds_alternative<std::nullptr_t>(data); }
    bool is_bool() const { return std::holds_alternative<bool>(data); }
    bool is_number() const { return std::holds_alternative<double>(data); }
    bool is_string() const { return std::holds_alternative<std::string>(data); }
    bool is_array() const { return std::holds_alternative<json_array>(data); }
    bool is_object() const { return std::holds_alternative<json_object>(data); }

    bool as_bool() const { return std::get<bool>(data); }
    double as_number() const { return std::get<double>(data); }
    const std::string& as_string() const { return std::get<std::string>(data); }
    const json_array& as_array() const { return std::get<json_array>(data); }
    const json_object& as_object() const { return std::get<json_object>(data); }

    bool has(const std::string& key) const {
        if (!is_object()) return false;
        return as_object().count(key) > 0;
    }

    const json_value& operator[](const std::string& key) const {
        return as_object().at(key);
    }

    const json_value& operator[](size_t idx) const {
        return as_array().at(idx);
    }
};

/**
 * @brief Simple JSON parser
 */
class json_parser {
public:
    explicit json_parser(const std::string& input) : input_(input), pos_(0) {}

    json_value parse() {
        skip_whitespace();
        return parse_value();
    }

private:
    const std::string& input_;
    size_t pos_;

    char peek() const {
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

    void expect(char c) {
        skip_whitespace();
        if (get() != c) {
            throw std::runtime_error("Expected '" + std::string(1, c) + "'");
        }
    }

    json_value parse_value() {
        skip_whitespace();
        char c = peek();

        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') return parse_string();
        if (c == 't' || c == 'f') return parse_bool();
        if (c == 'n') return parse_null();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number();

        throw std::runtime_error("Unexpected character in JSON");
    }

    json_value parse_object() {
        json_object obj;
        expect('{');
        skip_whitespace();

        if (peek() == '}') {
            get();
            return json_value{obj};
        }

        while (true) {
            skip_whitespace();
            if (peek() != '"') {
                throw std::runtime_error("Expected string key in object");
            }
            std::string key = parse_string_value();
            expect(':');
            obj[key] = parse_value();
            skip_whitespace();

            char c = get();
            if (c == '}') break;
            if (c != ',') throw std::runtime_error("Expected ',' or '}' in object");
        }

        return json_value{obj};
    }

    json_value parse_array() {
        json_array arr;
        expect('[');
        skip_whitespace();

        if (peek() == ']') {
            get();
            return json_value{arr};
        }

        while (true) {
            arr.push_back(parse_value());
            skip_whitespace();

            char c = get();
            if (c == ']') break;
            if (c != ',') throw std::runtime_error("Expected ',' or ']' in array");
        }

        return json_value{arr};
    }

    std::string parse_string_value() {
        expect('"');
        std::string result;

        while (peek() != '"') {
            char c = get();
            if (c == '\\') {
                c = get();
                switch (c) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        std::string hex;
                        for (int i = 0; i < 4; ++i) hex += get();
                        int code = std::stoi(hex, nullptr, 16);
                        if (code < 0x80) {
                            result += static_cast<char>(code);
                        } else if (code < 0x800) {
                            result += static_cast<char>(0xC0 | (code >> 6));
                            result += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            result += static_cast<char>(0xE0 | (code >> 12));
                            result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            result += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default: result += c; break;
                }
            } else {
                result += c;
            }
        }
        get();  // consume closing quote
        return result;
    }

    json_value parse_string() {
        return json_value{parse_string_value()};
    }

    json_value parse_number() {
        size_t start = pos_;
        if (peek() == '-') get();
        while (std::isdigit(static_cast<unsigned char>(peek()))) get();
        if (peek() == '.') {
            get();
            while (std::isdigit(static_cast<unsigned char>(peek()))) get();
        }
        if (peek() == 'e' || peek() == 'E') {
            get();
            if (peek() == '+' || peek() == '-') get();
            while (std::isdigit(static_cast<unsigned char>(peek()))) get();
        }
        return json_value{std::stod(input_.substr(start, pos_ - start))};
    }

    json_value parse_bool() {
        if (input_.substr(pos_, 4) == "true") {
            pos_ += 4;
            return json_value{true};
        }
        if (input_.substr(pos_, 5) == "false") {
            pos_ += 5;
            return json_value{false};
        }
        throw std::runtime_error("Invalid boolean");
    }

    json_value parse_null() {
        if (input_.substr(pos_, 4) == "null") {
            pos_ += 4;
            return json_value{nullptr};
        }
        throw std::runtime_error("Invalid null");
    }
};

// ============================================================================
// Base64 Decoding
// ============================================================================

/**
 * @brief Base64 decode table
 */
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

/**
 * @brief Decode Base64 string to binary data
 * @param input Base64 encoded string
 * @return Decoded binary data
 */
[[nodiscard]] std::vector<uint8_t> from_base64(const std::string& input) {
    std::vector<uint8_t> result;
    result.reserve((input.size() * 3) / 4);

    size_t i = 0;
    while (i < input.size()) {
        // Skip whitespace
        while (i < input.size() && std::isspace(static_cast<unsigned char>(input[i]))) ++i;
        if (i >= input.size()) break;

        uint32_t sextet[4] = {0, 0, 0, 0};
        int padding = 0;

        for (int j = 0; j < 4 && i < input.size(); ++j) {
            char c = input[i++];
            if (c == '=') {
                padding++;
                sextet[j] = 0;
            } else if (static_cast<unsigned char>(c) < 128 && base64_decode_table[static_cast<unsigned char>(c)] >= 0) {
                sextet[j] = static_cast<uint32_t>(base64_decode_table[static_cast<unsigned char>(c)]);
            } else {
                // Invalid character, skip
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

/**
 * @brief Parse VR string to vr_type
 * @param vr_str VR string (e.g., "PN", "LO")
 * @return Parsed vr_type
 */
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

/**
 * @brief Parse tag string (GGGGEEEE) to dicom_tag
 * @param tag_str Tag string
 * @return Parsed dicom_tag or nullopt
 */
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

/**
 * @brief Read file contents
 * @param path File path
 * @return File contents as string
 */
[[nodiscard]] std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + path.string());
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

/**
 * @brief Read bulk data from URI
 * @param uri The BulkDataURI
 * @param bulk_dir Base directory for relative paths
 * @return Binary data
 */
[[nodiscard]] std::vector<uint8_t> read_bulk_data(const std::string& uri,
                                                   const std::filesystem::path& bulk_dir) {
    std::string path = uri;

    // Remove file:// prefix if present
    if (path.substr(0, 7) == "file://") {
        path = path.substr(7);
    }

    // Handle relative paths
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
void parse_dataset(const json_object& json_obj,
                   pacs::core::dicom_dataset& dataset,
                   const options& opts);

/**
 * @brief Create DICOM element from JSON value
 * @param tag The DICOM tag
 * @param element_json The JSON element object
 * @param opts Command line options
 * @return Created dicom_element
 */
[[nodiscard]] pacs::core::dicom_element create_element(
    const pacs::core::dicom_tag& tag,
    const json_value& element_json,
    const options& opts) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    if (!element_json.is_object()) {
        throw std::runtime_error("Element value must be an object");
    }

    const auto& obj = element_json.as_object();

    // Get VR
    std::string vr_str = "UN";
    if (obj.count("vr")) {
        vr_str = obj.at("vr").as_string();
    }
    auto vr = parse_vr(vr_str);

    // Handle sequence
    if (vr == vr_type::SQ) {
        dicom_element elem{tag, vr};
        if (obj.count("Value") && obj.at("Value").is_array()) {
            auto& items = elem.sequence_items();
            for (const auto& item_json : obj.at("Value").as_array()) {
                if (item_json.is_object()) {
                    dicom_dataset item_dataset;
                    parse_dataset(item_json.as_object(), item_dataset, opts);
                    items.push_back(std::move(item_dataset));
                }
            }
        }
        return elem;
    }

    // Handle InlineBinary (Base64)
    if (obj.count("InlineBinary")) {
        std::string base64 = obj.at("InlineBinary").as_string();
        auto data = from_base64(base64);
        return dicom_element{tag, vr, std::span<const uint8_t>(data)};
    }

    // Handle BulkDataURI
    if (obj.count("BulkDataURI")) {
        std::string uri = obj.at("BulkDataURI").as_string();
        auto data = read_bulk_data(uri, opts.bulk_data_dir);
        return dicom_element{tag, vr, std::span<const uint8_t>(data)};
    }

    // Handle Value array
    if (!obj.count("Value") || !obj.at("Value").is_array()) {
        // Empty element
        return dicom_element{tag, vr};
    }

    const auto& values = obj.at("Value").as_array();
    if (values.empty()) {
        return dicom_element{tag, vr};
    }

    // Handle PersonName (PN)
    if (vr == vr_type::PN) {
        std::string combined;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) combined += "\\";
            if (values[i].is_object()) {
                const auto& pn = values[i].as_object();
                if (pn.count("Alphabetic")) {
                    combined += pn.at("Alphabetic").as_string();
                }
            } else if (values[i].is_string()) {
                combined += values[i].as_string();
            }
        }
        return dicom_element::from_string(tag, vr, combined);
    }

    // Handle string VRs
    if (is_string_vr(vr)) {
        std::string combined;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) combined += "\\";
            if (values[i].is_string()) {
                combined += values[i].as_string();
            } else if (values[i].is_number()) {
                // Some string VRs like IS, DS contain numeric values
                std::ostringstream oss;
                oss << std::setprecision(17) << values[i].as_number();
                combined += oss.str();
            }
        }
        return dicom_element::from_string(tag, vr, combined);
    }

    // Handle numeric VRs
    if (is_numeric_vr(vr)) {
        std::vector<uint8_t> data;

        auto write_values = [&]<typename T>() {
            data.reserve(values.size() * sizeof(T));
            for (const auto& v : values) {
                T num{};
                if (v.is_number()) {
                    num = static_cast<T>(v.as_number());
                } else if (v.is_string()) {
                    // Handle string representation of numbers
                    if constexpr (std::is_floating_point_v<T>) {
                        num = static_cast<T>(std::stod(v.as_string()));
                    } else {
                        num = static_cast<T>(std::stoll(v.as_string()));
                    }
                }
                const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&num);
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
        for (const auto& v : values) {
            if (v.is_string()) {
                auto tag_opt = parse_tag(v.as_string());
                if (tag_opt) {
                    uint16_t group = tag_opt->group();
                    uint16_t elem = tag_opt->element();
                    data.push_back(static_cast<uint8_t>(group & 0xFF));
                    data.push_back(static_cast<uint8_t>((group >> 8) & 0xFF));
                    data.push_back(static_cast<uint8_t>(elem & 0xFF));
                    data.push_back(static_cast<uint8_t>((elem >> 8) & 0xFF));
                }
            }
        }
        return dicom_element{tag, vr, std::span<const uint8_t>(data)};
    }

    // Fallback: treat as string
    if (values[0].is_string()) {
        std::string combined;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) combined += "\\";
            combined += values[i].as_string();
        }
        return dicom_element::from_string(tag, vr, combined);
    }

    return dicom_element{tag, vr};
}

/**
 * @brief Parse JSON object into DICOM dataset
 * @param json_obj The JSON object
 * @param dataset Output dataset
 * @param opts Command line options
 */
void parse_dataset(const json_object& json_obj,
                   pacs::core::dicom_dataset& dataset,
                   const options& opts) {
    for (const auto& [key, value] : json_obj) {
        // Skip non-tag keys (e.g., metadata fields)
        auto tag_opt = parse_tag(key);
        if (!tag_opt) {
            continue;
        }

        try {
            auto element = create_element(*tag_opt, value, opts);
            dataset.insert(std::move(element));
        } catch (const std::exception& e) {
            if (opts.verbose) {
                std::cerr << "Warning: Failed to parse element " << key
                          << ": " << e.what() << "\n";
            }
        }
    }
}

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
JSON to DICOM Converter (DICOM PS3.18)

Usage: )" << program_name
              << R"( <json-file> <output-dcm> [options]

Arguments:
  json-file         Input JSON file (DICOM PS3.18 format)
  output-dcm        Output DICOM file

Options:
  -h, --help              Show this help message
  -t, --transfer-syntax   Transfer Syntax UID (default: Explicit VR Little Endian)
  --template <dcm>        Template DICOM file (copies pixel data and missing tags)
  --bulk-data-dir <dir>   Directory for BulkDataURI resolution
  -v, --verbose           Verbose output
  -q, --quiet             Quiet mode (errors only)

Transfer Syntax Options:
  1.2.840.10008.1.2      Implicit VR Little Endian
  1.2.840.10008.1.2.1    Explicit VR Little Endian (default)
  1.2.840.10008.1.2.2    Explicit VR Big Endian

Examples:
  )" << program_name
              << R"( metadata.json output.dcm
  )" << program_name
              << R"( metadata.json output.dcm --template original.dcm
  )" << program_name
              << R"( metadata.json output.dcm --bulk-data-dir ./bulk/
  )" << program_name
              << R"( metadata.json output.dcm -t 1.2.840.10008.1.2

Input Format (DICOM PS3.18 JSON):
  {
    "00100010": {
      "vr": "PN",
      "Value": [{"Alphabetic": "DOE^JOHN"}]
    },
    "00100020": {
      "vr": "LO",
      "Value": ["12345678"]
    }
  }

Exit Codes:
  0  Success
  1  Invalid arguments
  2  File error or invalid JSON
)";
}

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @param opts Output: parsed options
 * @return true if arguments are valid
 */
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

/**
 * @brief Convert JSON file to DICOM
 * @param opts Command line options
 * @return 0 on success, non-zero on error
 */
int convert_file(const options& opts) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    // Read and parse JSON
    std::string json_content;
    try {
        json_content = read_file(opts.input_path);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }

    json_value json;
    try {
        json_parser parser(json_content);
        json = parser.parse();
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to parse JSON: " << e.what() << "\n";
        return 2;
    }

    if (!json.is_object()) {
        std::cerr << "Error: JSON root must be an object\n";
        return 2;
    }

    // Create dataset from JSON
    dicom_dataset dataset;
    try {
        parse_dataset(json.as_object(), dataset, opts);
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

        // Merge template data (JSON values override template)
        for (const auto& [tag, element] : template_file->dataset()) {
            if (dataset.get(tag) == nullptr) {
                // Copy from template if not in JSON
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
      _ ____   ___  _   _   _____  ___    ____   ____ __  __
     | / ___| / _ \| \ | | |_   _|/ _ \  |  _ \ / ___|  \/  |
  _  | \___ \| | | |  \| |   | | | | | | | | | | |   | |\/| |
 | |_| |___) | |_| | |\  |   | | | |_| | | |_| | |___| |  | |
  \___/|____/ \___/|_| \_|   |_|  \___/  |____/ \____|_|  |_|

        JSON to DICOM Converter (PS3.18)
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
      _ ____   ___  _   _   _____  ___    ____   ____ __  __
     | / ___| / _ \| \ | | |_   _|/ _ \  |  _ \ / ___|  \/  |
  _  | \___ \| | | |  \| |   | | | | | | | | | | |   | |\/| |
 | |_| |___) | |_| | |\  |   | | | |_| | | |_| | |___| |  | |
  \___/|____/ \___/|_| \_|   |_|  \___/  |____/ \____|_|  |_|

        JSON to DICOM Converter (PS3.18)
)" << "\n";
    }

    return convert_file(opts);
}
